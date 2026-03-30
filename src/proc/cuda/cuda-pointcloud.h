// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once
#include "../pointcloud.h"

// When both CUDA and NEON are available, inherit from pointcloud_neon
// to get NEON optimizations for methods not accelerated by CUDA
#if defined(__ARM_NEON) && defined(BUILD_WITH_NEON) && !defined(ANDROID)
#include "../neon/neon-pointcloud.h"
#endif

namespace librealsense
{
#if defined(__ARM_NEON) && defined(BUILD_WITH_NEON) && !defined(ANDROID)
    class pointcloud_cuda : public pointcloud_neon
#else
    class pointcloud_cuda : public pointcloud
#endif
    {
    public:
        pointcloud_cuda();
    private:
        const float3 * depth_to_points(
            rs2::points output,
            const rs2_intrinsics &depth_intrinsics,
            const rs2::depth_frame& depth_frame) override;
    };
}
