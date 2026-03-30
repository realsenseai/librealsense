// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#pragma once
#include "../pointcloud.h"

namespace librealsense
{
#if defined(__ARM_NEON) && defined(BUILD_WITH_NEON) && !defined(ANDROID)
    class pointcloud_neon : public pointcloud
    {
    public:
        pointcloud_neon();

        void preprocess() override;
        const float3 * depth_to_points(
            rs2::points output,
            const rs2_intrinsics &depth_intrinsics,
            const rs2::depth_frame& depth_frame) override;
        void get_texture_map(
            rs2::points output,
            const float3* points,
            const unsigned int width,
            const unsigned int height,
            const rs2_intrinsics &other_intrinsics,
            const rs2_extrinsics& extr,
            float2* pixels_ptr) override;

    private:
        template <rs2_distortion dist>
        void get_texture_map_neon(float2 * texture_map,
            const float3 * points,
            const unsigned int width,
            const unsigned int height,
            const rs2_intrinsics & other_intrinsics,
            const rs2_extrinsics & extr,
            float2 * pixels_ptr);

        std::vector<float> _pre_compute_map_x;
        std::vector<float> _pre_compute_map_y;

        void pre_compute_x_y_map();
    };
#endif
}
