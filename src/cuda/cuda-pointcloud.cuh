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
#include <memory>
#include <cstring>

// CUDA headers
#include <cuda_runtime.h>

#ifdef _MSC_VER
// Add library dependencies if using VS
#pragma comment(lib, "cudart_static")
#endif

namespace rscuda
{
    // Per-instance helper that owns persistent device buffers and a one-time intrinsics upload,
    // reused across frames. Eliminates per-frame cudaMalloc/cudaFree overhead.
    // One helper is owned by each pointcloud_cuda instance, so multiple cameras/streams stay isolated.
    class pointcloud_cuda_helper
    {
    public:
        static constexpr int THREADS_PER_BLOCK = 256; // Conventional NVIDIA "if in doubt" default

        void deproject_depth_cuda( float * points, const rs2_intrinsics & intrin, const uint16_t * depth, float depth_scale );

    private:
        std::shared_ptr<float>          _d_points;           // device output points (count * 3 floats)
        std::shared_ptr<uint16_t>       _d_depth;            // device depth input  (count uint16)
        std::shared_ptr<rs2_intrinsics> _d_intrin;           // device intrinsics (uploaded once)
        int                             _count = 0;          // pixel count the buffers are sized for
        rs2_intrinsics                  _intrin_cached = {}; // last-uploaded intrinsics (change guard)
    };
}

#endif // RS2_USE_CUDA

#endif // LIBREALSENSE_CUDA_POINTCLOUD_H
