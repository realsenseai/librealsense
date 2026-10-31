// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#include "depth-formats-converter.h"

#include "stream.h"

#ifdef RS2_USE_CUDA
#include "cuda/cuda-conversion.cuh"
#include "rsutils/accelerators/gpu.h"
#endif

namespace librealsense
{
    void unpack_z16_y8_from_sr300_inzi( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size)
    {
        auto count = width * height;
        auto in = reinterpret_cast<const uint16_t*>(source);
        auto out_ir = reinterpret_cast<uint8_t *>(dest[1]);
#ifdef RS2_USE_CUDA
        if (rsutils::rs2_is_gpu_available())
        {
            rscuda::unpack_z16_y8_from_sr300_inzi_cuda(out_ir, in, count);
            in += count;
        }
        else
#endif
        for (int i = 0; i < count; ++i) *out_ir++ = *in++ >> 2;
        std::memcpy( dest[0], in, count * 2 );
    }

    void unpack_z16_y16_from_sr300_inzi( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size)
    {
        auto count = width * height;
        auto in = reinterpret_cast<const uint16_t*>(source);
        auto out_ir = reinterpret_cast<uint16_t*>(dest[1]);
#ifdef RS2_USE_CUDA
        if (rsutils::rs2_is_gpu_available())
        {
            rscuda::unpack_z16_y16_from_sr300_inzi_cuda(out_ir, in, count);
            in += count;
        }
        else
#endif
        for (int i = 0; i < count; ++i) *out_ir++ = *in++ << 6;
        std::memcpy( dest[0], in, count * 2 );
    }

    void unpack_inzi(rs2_format dst_ir_format, uint8_t * const d[], const uint8_t * s, int width, int height, int actual_size)
    {
        switch (dst_ir_format)
        {
        case RS2_FORMAT_Y8:
            unpack_z16_y8_from_sr300_inzi(d, s, width, height, actual_size);
            break;
        case RS2_FORMAT_Y16:
            unpack_z16_y16_from_sr300_inzi(d, s, width, height, actual_size);
            break;
        default:
            LOG_ERROR("Unsupported format for INZI conversion.");
            break;
        }
    }

    template<class SOURCE, class UNPACK> void unpack_pixels( uint8_t * const dest[], int count, const SOURCE * source, UNPACK unpack, int actual_size)
    {
        auto out = reinterpret_cast<decltype(unpack(SOURCE())) *>(dest[0]);
        for (int i = 0; i < count; ++i) *out++ = unpack(*source++);
    }

    void unpack_y16_from_y16_10( uint8_t * const d[], const uint8_t * s, int width, int height, int actual_size) { unpack_pixels(d, width * height, reinterpret_cast<const uint16_t*>(s), [](uint16_t pixel) -> uint16_t { return pixel << 6; }, actual_size); }
    void unpack_y8_from_y16_10( uint8_t * const d[], const uint8_t * s, int width, int height, int actual_size) { unpack_pixels(d, width * height, reinterpret_cast<const uint16_t*>(s), [](uint16_t pixel) -> uint8_t { return pixel >> 2; }, actual_size); }

    void unpack_invi(rs2_format dst_format, uint8_t * const d[], const uint8_t * s, int width, int height, int actual_size)
    {
        switch (dst_format)
        {
        case RS2_FORMAT_Y8:
            unpack_y8_from_y16_10(d, s, width, height, actual_size);
            break;
        case RS2_FORMAT_Y16:
            unpack_y16_from_y16_10(d, s, width, height, actual_size);
            break;
        default:
            LOG_ERROR("Unsupported format for INVI conversion.");
            break;
        }
    }

    void copy_raw10( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size)
    {
        auto count = width * height; // num of pixels
        std::memcpy( dest[0], source, size_t( 5.0 * ( count / 4.0 ) ) );
    }

    void unpack_y10bpack( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size)
    {
        auto count = width * height / 4; // num of pixels
        uint8_t  * from = (uint8_t*)(source);
        uint16_t * to = (uint16_t*)(dest[0]);

        // Put the 10 bit into the msb of uint16_t
        for (int i = 0; i < count; i++, from += 5) // traverse macro-pixels
        {
            *to++ = ((from[0] << 2) | (from[4] & 3)) << 6;
            *to++ = ((from[1] << 2) | ((from[4] >> 2) & 3)) << 6;
            *to++ = ((from[2] << 2) | ((from[4] >> 4) & 3)) << 6;
            *to++ = ((from[3] << 2) | ((from[4] >> 6) & 3)) << 6;
        }
    }

    void unpack_w10(rs2_format dst_format, uint8_t * const d[], const uint8_t * s, int width, int height, int actual_size)
    {
        switch (dst_format)
        {
        case RS2_FORMAT_W10:
        case RS2_FORMAT_RAW10:
            copy_raw10(d, s, width, height, actual_size);
            break;
        case RS2_FORMAT_Y10BPACK:
            unpack_y10bpack(d, s, width, height, actual_size);
            break;
        default:
            LOG_ERROR("Unsupported format for W10 unpacking.");
            break;
        }
    }

    // INZI converter
    inzi_converter::inzi_converter(const char * name, rs2_format target_ir_format)
        : interleaved_functional_processing_block(name, RS2_FORMAT_INZI, RS2_FORMAT_Z16, RS2_STREAM_DEPTH, RS2_EXTENSION_DEPTH_FRAME, 0,
                                                                         target_ir_format, RS2_STREAM_INFRARED, RS2_EXTENSION_VIDEO_FRAME, 1)
    {}

    void inzi_converter::process_function( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size, int input_size)
    {
        // convension: right frame is IR and left is Z16
        unpack_inzi(_right_target_format, dest, source, width, height, actual_size);
    }

    void invi_converter::process_function( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size, int input_size)
    {
        unpack_invi(_target_format, dest, source, width, height, actual_size);
    }

    w10_converter::w10_converter(const char * name, const rs2_format& target_format) :
        functional_processing_block(name, target_format, RS2_STREAM_INFRARED, RS2_EXTENSION_VIDEO_FRAME) {}

    void w10_converter::process_function( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size, int input_size)
    {
        unpack_w10(_target_format, dest, source, width, height, actual_size);
    }
}
