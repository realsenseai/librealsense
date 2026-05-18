// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "rsutils/accelerators/gpu.h"
#include <rsutils/easylogging/easyloggingpp.h>

#ifdef RS2_USE_CUDA
#ifdef RS2_USE_HIP
#include <hip/hip_runtime.h>
#define cudaGetDeviceCount hipGetDeviceCount
#define cudaGetErrorString hipGetErrorString
#define cudaError_t hipError_t
#define cudaSuccess hipSuccess
#else
#include <cuda_runtime.h>
#endif
#endif

namespace rsutils {

    class GPUChecker {
    public:
        static bool is_gpu_available() {
            static int gpuDeviceCount = -1;
#ifdef RS2_USE_CUDA

            if (gpuDeviceCount < 0)
            {
                cudaError_t error = cudaGetDeviceCount(&gpuDeviceCount);
                if (error != cudaSuccess) {
                    LOG_ERROR("cudaGetDeviceCount failed: " << cudaGetErrorString(error));
                    gpuDeviceCount = 0; // Set to 0 to avoid repeated error logging
                }
                if (gpuDeviceCount <= 0)
                {
#ifdef RS2_USE_HIP
                    LOG_INFO("Avoid HIP execution as no AMD GPU found.");
                }
                else
                {
                    LOG_INFO("Found " << gpuDeviceCount << " AMD GPU.");
#else
                    LOG_INFO("Avoid CUDA execution as no NVIDIA GPU found.");
                }
                else
                {
                    LOG_INFO("Found " << gpuDeviceCount << " NVIDIA GPU.");
#endif
                }
            }
#endif
            return gpuDeviceCount > 0;
        }
    };

    bool rs2_is_gpu_available() {
        return rsutils::GPUChecker::is_gpu_available();
    }

} // namespace rsutils

