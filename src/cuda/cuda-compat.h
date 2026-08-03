// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

// Small CUDA-version compatibility shims shared across the CUDA translation units, kept
// dependency-free (only <cuda_runtime.h>) so any .cu/.cuh can include it without pulling in
// librealsense types.
#ifdef RS2_USE_CUDA
#include <cuda_runtime.h>

// cudaPointerAttributes::type was named ::memoryType before CUDA 11.0. The CUDA arch list in
// CMake/cuda_config.cmake still supports pre-11 toolkits, so read the field portably.
#if CUDART_VERSION >= 11000
    #define RS_CUDA_MEMTYPE( a ) ( (a).type )
#else
    #define RS_CUDA_MEMTYPE( a ) ( (a).memoryType )
#endif

#endif  // RS2_USE_CUDA
