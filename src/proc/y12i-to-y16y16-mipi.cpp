// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include "y12i-to-y16y16-mipi.h"
#include "stream.h"
#ifdef RS2_USE_CUDA
#include "cuda/cuda-conversion.cuh"
#include "rsutils/accelerators/gpu.h"
#endif

namespace librealsense
{
//D457 dev - padding of 8 bits added after each bits, should be removed after it is corrected in SerDes
    struct y12i_pixel_mipi { uint8_t rl : 8, rh : 4, ll : 4, lh : 8, padding : 8; int l() const { return lh << 4 | ll; } int r() const { return rh << 8 | rl; } };

    void unpack_y16_y16_from_y12i_10_mipi( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size)
    {
        auto count = width * height;
#ifdef RS2_USE_CUDA
        if (rsutils::rs2_is_gpu_available())
        {
            rscuda::split_frame_y16_y16_from_y12i_cuda(dest, count, reinterpret_cast<const rscuda::y12i_pixel_mipi*>(source));
            return;
        }
#endif
        split_frame(dest, count, reinterpret_cast<const y12i_pixel_mipi*>(source),
            [](const y12i_pixel_mipi& p) -> uint16_t { return p.l() << 6 | p.l() >> 4; },  // We want to convert 10-bit data to 16-bit data
            [](const y12i_pixel_mipi& p) -> uint16_t { return p.r() << 6 | p.r() >> 4; }); // Multiply by 64 1/16 to efficiently approximate 65535/1023
    }

    y12i_to_y16y16_mipi::y12i_to_y16y16_mipi(int left_idx, int right_idx)
        : y12i_to_y16y16_mipi("Y12I to Y16L Y16R Transform", left_idx, right_idx) {}

    y12i_to_y16y16_mipi::y12i_to_y16y16_mipi(const char * name, int left_idx, int right_idx)
        : interleaved_functional_processing_block(name, RS2_FORMAT_Y12I, RS2_FORMAT_Y16, RS2_STREAM_INFRARED, RS2_EXTENSION_VIDEO_FRAME, 1,
                                                                         RS2_FORMAT_Y16, RS2_STREAM_INFRARED, RS2_EXTENSION_VIDEO_FRAME, 2)
    {}

    void y12i_to_y16y16_mipi::process_function( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size, int input_size)
    {
        unpack_y16_y16_from_y12i_10_mipi(dest, source, width, height, actual_size);
    }
}
