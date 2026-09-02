// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

// Small CUDA/HIP-version compatibility shims shared across the GPU translation units, kept
// dependency-free (only <cuda_runtime.h> / <hip/hip_runtime.h>) so any .cu/.cuh/.hip can
// include it without pulling in librealsense types.
// Guarded on either macro (not just RS2_USE_CUDA) so this compiles under HIP regardless of
// whether global_config.cmake's BUILD_WITH_HIP branch also defines RS2_USE_CUDA (today, for
// back-compat) or drops it in favor of RS2_USE_HIP alone.
#if defined(RS2_USE_CUDA) || defined(RS2_USE_HIP)
#ifdef RS2_USE_HIP
#include <hip/hip_runtime.h>

// hipPointerAttribute_t has always used `.type` (ROCm never had a `.memoryType`-named
// pre-11-style field to support), so no version switch is needed on this branch.
#define RS_CUDA_MEMTYPE( a ) ( (a).type )

#else
#include <cuda_runtime.h>

// cudaPointerAttributes::type was named ::memoryType before CUDA 11.0. The CUDA arch list in
// CMake/cuda_config.cmake still supports pre-11 toolkits, so read the field portably.
#if CUDART_VERSION >= 11000
    #define RS_CUDA_MEMTYPE( a ) ( (a).type )
#else
    #define RS_CUDA_MEMTYPE( a ) ( (a).memoryType )
#endif

#endif  // RS2_USE_HIP
#endif  // RS2_USE_CUDA || RS2_USE_HIP
