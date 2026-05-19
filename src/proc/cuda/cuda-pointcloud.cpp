// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.
#include "proc/cuda/cuda-pointcloud.h"

#ifdef RS2_USE_CUDA
#include "../../cuda/cuda-pointcloud.cuh"
#endif

namespace librealsense
{
    pointcloud_cuda::pointcloud_cuda()
#if defined(__ARM_NEON) && defined(BUILD_WITH_NEON) && !defined(ANDROID)
        // When NEON is available, inherit from pointcloud_neon which calls pointcloud("Pointcloud (NEON)")
        // We keep the NEON name since this is a hybrid using CUDA for depth_to_points and NEON for get_texture_map
        : pointcloud_neon()
#else
        : pointcloud("Pointcloud (CUDA)")
#endif
    {}

    const float3 * pointcloud_cuda::depth_to_points(
        rs2::points output,
        const rs2_intrinsics &depth_intrinsics,
        const rs2::depth_frame& depth_frame)
    {
        auto image = output.get_vertices();
        auto depth_data = (uint16_t*)depth_frame.get_data();
        auto depth_scale = depth_frame.get_units();
#ifdef RS2_USE_CUDA
        rscuda::deproject_depth_cuda((float*)image, depth_intrinsics, depth_data, depth_scale);
#endif
        return (float3*)image;
    }
}
