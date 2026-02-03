// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.
#pragma once

#include "synthetic-stream.h"
#include <src/float3.h>


namespace librealsense
{
    class occlusion_filter;

    class LRS_EXTENSION_API pointcloud : public stream_filter_processing_block
    {
    public:
        static std::shared_ptr<pointcloud> create();

        pointcloud();

        virtual const float3 * depth_to_points(
            rs2::points output,
            const rs2_intrinsics &depth_intrinsics, 
            const rs2::depth_frame& depth_frame);
        virtual void get_texture_map(
            rs2::points output,
            const float3* points,
            const unsigned int width,
            const unsigned int height,
            const rs2_intrinsics &other_intrinsics,
            const rs2_extrinsics& extr,
            float2* pixels_ptr);
        virtual rs2::points allocate_points(const rs2::frame_source& source, const rs2::frame& f);
        virtual void preprocess();
        virtual bool run__occlusion_filter(const rs2_extrinsics& extr);

    protected:
        pointcloud(const char* name);

        bool should_process(const rs2::frame& frame) override;
        rs2::frame process_frame(const rs2::frame_source& source, const rs2::frame& f) override;

        optional_value<rs2_intrinsics>         _depth_intrinsics;
        optional_value<rs2_intrinsics>         _other_intrinsics;
        optional_value<float>                  _depth_units;
        optional_value<rs2_extrinsics>         _extrinsics;
        std::shared_ptr<occlusion_filter>      _occlusion_filter;

        // Intermediate translation table of (depth_x*depth_y) with actual texel coordinates per depth pixel
        std::vector<float2>                    _pixels_map;

        rs2::stream_profile _output_stream;
        rs2::frame _other_stream;
        rs2::frame _depth_stream;

        void inspect_depth_frame(const rs2::frame& depth);
        void inspect_other_frame(const rs2::frame& other);
        rs2::frame process_depth_frame(const rs2::frame_source& source, const rs2::depth_frame& depth);
        void set_extrinsics();

        stream_filter _prev_stream_filter;
        std::shared_ptr< pointcloud > _registered_auto_calib_cb;

        std::vector<float> _pre_compute_map_x;
        std::vector<float> _pre_compute_map_y;
    };
}
