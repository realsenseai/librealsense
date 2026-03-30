// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#pragma once

#include "proc/align.h"
#include <src/float3.h>

namespace librealsense
{
#ifndef ANDROID
#if defined(__ARM_NEON) && defined(BUILD_WITH_NEON)
    class align_neon_helper
    {
    public:
        align_neon_helper(const rs2_intrinsics& from, float depth_scale);

        inline void align_depth_to_other(const uint16_t* z_pixels,
            uint16_t* dest, int bpp,
            const rs2_intrinsics& depth,
            const rs2_intrinsics& to,
            const rs2_extrinsics& from_to_other);

        inline void align_other_to_depth(const uint16_t* z_pixels,
            const uint8_t * source,
            uint8_t* dest, int bpp, const rs2_intrinsics& to,
            const rs2_extrinsics& from_to_other);

        void pre_compute_x_y_map_corners();

    private:
        const rs2_intrinsics _depth;
        float _depth_scale;

        std::vector<float> _pre_compute_map_x_top_left;
        std::vector<float> _pre_compute_map_y_top_left;
        std::vector<float> _pre_compute_map_x_bottom_right;
        std::vector<float> _pre_compute_map_y_bottom_right;

        std::vector<int2> _pixel_top_left_int;
        std::vector<int2> _pixel_bottom_right_int;

        void pre_compute_x_y_map(std::vector<float>& pre_compute_map_x,
            std::vector<float>& pre_compute_map_y,
            float offset = 0);

        template<rs2_distortion dist = RS2_DISTORTION_NONE>
        inline void align_depth_to_other_neon(const uint16_t* z_pixels,
            uint16_t* dest, const rs2_intrinsics& depth,
            const rs2_intrinsics& to,
            const rs2_extrinsics& from_to_other);

        template<rs2_distortion dist = RS2_DISTORTION_NONE>
        inline void align_other_to_depth_neon(const uint16_t* z_pixels,
            const uint8_t * source,
            uint8_t* dest, int bpp, const rs2_intrinsics& to,
            const rs2_extrinsics& from_to_other);

        inline void move_depth_to_other(const uint16_t* z_pixels,
            uint16_t* dest, const rs2_intrinsics& to,
            const std::vector<int2>& pixel_top_left_int,
            const std::vector<int2>& pixel_bottom_right_int);

        template<class T >
        inline void move_other_to_depth(const uint16_t* z_pixels,
            const T* source,
            T* dest, const rs2_intrinsics& to,
            const std::vector<int2>& pixel_top_left_int,
            const std::vector<int2>& pixel_bottom_right_int);
    };

    class align_neon : public align
    {
    public:
        align_neon(rs2_stream align_to) : align(align_to, "Align (NEON)") {}
    protected:
        void reset_cache(rs2_stream from, rs2_stream to) override;

        void align_z_to_other(rs2::video_frame& aligned, const rs2::video_frame& depth, const rs2::video_stream_profile& other_profile, float z_scale) override;

        void align_other_to_z(rs2::video_frame& aligned, const rs2::video_frame& depth, const rs2::video_frame& other, float z_scale) override;
    private:
        std::shared_ptr<align_neon_helper> _neon_helper;
    };
#endif
#endif
} // namespace librealsense
