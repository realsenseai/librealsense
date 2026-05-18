#pragma once
#ifndef LIBREALSENSE_CUDA_POINTCLOUD_H
#define LIBREALSENSE_CUDA_POINTCLOUD_H

#ifdef RS2_USE_CUDA

// Types
#include <stdint.h>
#include "../../include/librealsense2/rs.h"
#include "assert.h"
#include "../../include/librealsense2/rsutil.h"
#include <functional>

// GPU runtime headers
#ifdef RS2_USE_HIP
#include <hip/hip_runtime.h>
#else
#include <cuda_runtime.h>
#endif

#ifdef _MSC_VER 
// Add library dependencies if using VS
#pragma comment(lib, "cudart_static")
#endif

#define RS2_CUDA_THREADS_PER_BLOCK 256

namespace rscuda
{
    void deproject_depth_cuda(float * points, const rs2_intrinsics & intrin, const uint16_t * depth, float depth_scale);

}

#endif // RS2_USE_CUDA

#endif // LIBREALSENSE_CUDA_POINTCLOUD_H
