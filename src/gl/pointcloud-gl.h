// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once
#include "opengl3.h"
#include "proc/pointcloud.h"
#include "synthetic-stream-gl.h"

namespace librealsense
{
    namespace gl
    {
        class pointcloud_gl : public pointcloud, public gpu_processing_object
        {
        public:
            pointcloud_gl();
            ~pointcloud_gl() override;

        private:
            void cleanup_gpu_resources() override;
            void create_gpu_resources() override;

            const float3 * depth_to_points(
                rs2::points output,
                const rs2_intrinsics &depth_intrinsics, 
                const rs2::depth_frame& depth_frame) override;
            void get_texture_map(rs2::points output,
                const float3* points,
                const unsigned int width,
                const unsigned int height,
                const rs2_intrinsics &other_intrinsics,
                const rs2_extrinsics& extr,
                float2* pixels_ptr) override;
            void get_gl_points(rs2::points& output,
                const unsigned int width,
                const unsigned int height);
            rs2::points allocate_points(
                const rs2::frame_source& source, 
                const rs2::frame& f) override;

            bool run__occlusion_filter(const rs2_extrinsics& extr) override;

            std::shared_ptr<rs2::visualizer_2d> _projection_renderer;
            std::shared_ptr<rs2::visualizer_2d> _occu_renderer;
            std::shared_ptr<rs2::visualizer_2d> _point_cloud_processor;

            rs2::depth_frame _depth_data;
            float _depth_scale;
            rs2_intrinsics _depth_intr;
            int _enabled = 0;
        };
    }
}
