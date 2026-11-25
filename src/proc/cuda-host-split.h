// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#ifdef RS2_USE_CUDA

#include "rsutils/accelerators/gpu.h"

namespace librealsense
{
    // Interleaved-to-planar splits (Y8I, ...) are memory bound. On integrated GPUs (Jetson) the host splitter
    // is faster than staging through CUDA, and the first CUDA conversion maps device memory, which can block
    // the capture thread long enough to overrun the V4L2 queue at stream start.
    inline bool use_gpu_for_host_split()
    {
#ifdef RS2_USE_HIP
        return rsutils::rs2_is_gpu_available();
#else
        return rsutils::rs2_is_gpu_available() && ! rsutils::rs2_is_cuda_integrated();
#endif
    }
}  // namespace librealsense

#endif  // RS2_USE_CUDA
