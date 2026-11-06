// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019-24 RealSense, Inc. All Rights Reserved.

#include "y8i-to-y8y8.h"

#include <src/stream.h>
#include <src/image.h>

#ifdef RS2_USE_CUDA
#include "cuda/cuda-conversion.cuh"
#include "rsutils/accelerators/gpu.h"
#endif

namespace librealsense
{
    struct y8i_pixel { uint8_t l, r; };
    void unpack_y8_y8_from_y8i( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size)
    {
        auto count = width * height;
#ifdef RS2_USE_CUDA
        if (rsutils::rs2_is_gpu_available())
        {
            rscuda::split_frame_y8_y8_from_y8i_cuda(dest, count, reinterpret_cast<const y8i_pixel*>(source));
            return;
        }
#endif
        split_frame(dest, count, reinterpret_cast<const y8i_pixel*>(source),
            [](const y8i_pixel & p) -> uint8_t { return p.l; },
            [](const y8i_pixel & p) -> uint8_t { return p.r; });
    }

    y8i_to_y8y8::y8i_to_y8y8(int left_idx, int right_idx) :
        y8i_to_y8y8("Y8i to Y8-Y8 Converter", left_idx, right_idx) {}

    y8i_to_y8y8::y8i_to_y8y8(const char * name, int left_idx, int right_idx)
        : interleaved_functional_processing_block(name, RS2_FORMAT_Y8I, RS2_FORMAT_Y8, RS2_STREAM_INFRARED, RS2_EXTENSION_VIDEO_FRAME, 1,
                                                                        RS2_FORMAT_Y8, RS2_STREAM_INFRARED, RS2_EXTENSION_VIDEO_FRAME, 2)
    {}

    void y8i_to_y8y8::process_function( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size, int input_size)
    {
        unpack_y8_y8_from_y8i(dest, source, width, height, actual_size);
    }
} // namespace librealsense
