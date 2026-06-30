// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include "neon-align.h"

#if defined(__ARM_NEON) && defined(BUILD_WITH_NEON) && !defined(ANDROID)

#include <arm_neon.h>
namespace librealsense
{
    template <int N>
    struct bytes
    {
        uint8_t b[N];
    };

    static inline bool is_special_resolution(const rs2_intrinsics &depth, const rs2_intrinsics &to)
    {
        if ((depth.width == 640 && depth.height == 240 && to.width == 320 && to.height == 180) ||
            (depth.width == 640 && depth.height == 480 && to.width == 640 && to.height == 360))
            return true;
        return false;
    }

    template <rs2_distortion dist>
    static inline void distorte_x_y(
        const float32x4_t &x, const float32x4_t &y,
        float32x4_t *distorted_x, float32x4_t *distorted_y, const float32x4_t(&c)[5])
    {
        *distorted_x = x;
        *distorted_y = y;
    }

    template <>
    inline void distorte_x_y<RS2_DISTORTION_MODIFIED_BROWN_CONRADY>(
        const float32x4_t &x, const float32x4_t &y,
        float32x4_t *distorted_x, float32x4_t *distorted_y, const float32x4_t(&c)[5])
    {
        const auto one = vdupq_n_f32(1);
        const auto two = vdupq_n_f32(2);

        // r2 = x * x + y * y
        const auto r2 = vaddq_f32(vmulq_f32(x, x), vmulq_f32(y, y));
        // f = 1 + c[0] * r2 + c[1] * r2 ^ 2 + c[4] * r2 ^ 3
        //   = 1 + (c[0] + (c[1] + c[4] * r2) * r2) * r2
        const auto f = vfmaq_f32(one, r2, vfmaq_f32(c[0], r2, vfmaq_f32(c[1], r2, c[4])));

        const auto x_f = vmulq_f32(x, f);
        const auto y_f = vmulq_f32(y, f);

        // dx = x_f + 2 * c[2] * x_f * y_f + c[3] * (r2 + 2 * x_f * x_f)
        //    = x_f * (1 + 2 * c[2] * y_f + c[3] * 2 * x_f) + c[3] * r2
        //    = x_f * (1 + 2 * (c[2] * y_f + c[3] * x_f)) + c[3] * r2
        *distorted_x = vfmaq_f32(vmulq_f32(x_f, vfmaq_f32(one, two, vfmaq_f32(vmulq_f32(c[2], y_f), c[3], x_f))), c[3], r2);

        // dy = y_f + 2 * c[3] * x_f * y_f + c[2] * (r2 + 2 * y_f * y_f)
        //    = y_f * (1 + 2 * c[3] * x_f + c[2] * 2 * y_f) + c[2] * r2
        //    = y_f * (1 + 2 * (c[3] * x_f + c[2] * y_f)) + c[2] * r2
        *distorted_y = vfmaq_f32(vmulq_f32(y_f, vfmaq_f32(one, two, vfmaq_f32(vmulq_f32(c[3], x_f), c[2], y_f))), c[2], r2);
    }

    template <rs2_distortion dist>
    static inline void get_texture_map_neon(const uint16_t *depth,
                                            float depth_scale,
                                            const unsigned int size,
                                            const float *pre_compute_x, const float *pre_compute_y,
                                            uint8_t *pixels_ptr_int,
                                            const rs2_intrinsics &to,
                                            const rs2_extrinsics &from_to_other)
    {
        auto res = reinterpret_cast<int32_t *>(pixels_ptr_int);

        float32x4_t r[9];
        float32x4_t t[3];
        float32x4_t c[5];
        for (int i = 0; i < 9; ++i)
        {
            r[i] = vdupq_n_f32(from_to_other.rotation[i]);
        }
        for (int i = 0; i < 3; ++i)
        {
            t[i] = vdupq_n_f32(from_to_other.translation[i]);
        }
        for (int i = 0; i < 5; ++i)
        {
            c[i] = vdupq_n_f32(to.coeffs[i]);
        }
        const auto zero = vdupq_n_f32(0.0f);
        const auto half = vdupq_n_f32(0.5f);
        const auto fx = vdupq_n_f32(to.fx);
        const auto fy = vdupq_n_f32(to.fy);
        const auto ppx = vdupq_n_f32(to.ppx);
        const auto ppy = vdupq_n_f32(to.ppy);
        const auto scale = vdupq_n_f32(depth_scale);

        for (unsigned int i = 0; i < size; i += 8)
        {
            const auto x0 = vld1q_f32(pre_compute_x + i);
            const auto x1 = vld1q_f32(pre_compute_x + i + 4);

            const auto y0 = vld1q_f32(pre_compute_y + i);
            const auto y1 = vld1q_f32(pre_compute_y + i + 4);

            const auto d = vld1q_u16(depth + i);
            const auto depth0 = vmulq_f32(vcvtq_f32_s32((int32x4_t)vmovl_u16(vget_low_u16(d))), scale);
            const auto depth1 = vmulq_f32(vcvtq_f32_s32((int32x4_t)vmovl_u16(vget_high_u16(d))), scale);

            // calculate 3D point
            // rs2_deproject_pixel_to_point
            const auto p0x = vmulq_f32(depth0, x0);
            const auto p0y = vmulq_f32(depth0, y0);

            const auto p1x = vmulq_f32(depth1, x1);
            const auto p1y = vmulq_f32(depth1, y1);

            // transform to other
            // rs2_transform_point_to_point
            auto p_x0 = vfmaq_f32(vfmaq_f32(vfmaq_f32(t[0], r[6], depth0), r[3], p0y), r[0], p0x);
            auto p_y0 = vfmaq_f32(vfmaq_f32(vfmaq_f32(t[1], r[7], depth0), r[4], p0y), r[1], p0x);
            auto p_z0 = vfmaq_f32(vfmaq_f32(vfmaq_f32(t[2], r[8], depth0), r[5], p0y), r[2], p0x);

            auto p_x1 = vfmaq_f32(vfmaq_f32(vfmaq_f32(t[0], r[6], depth1), r[3], p1y), r[0], p1x);
            auto p_y1 = vfmaq_f32(vfmaq_f32(vfmaq_f32(t[1], r[7], depth1), r[4], p1y), r[1], p1x);
            auto p_z1 = vfmaq_f32(vfmaq_f32(vfmaq_f32(t[2], r[8], depth1), r[5], p1y), r[2], p1x);

            // rs2_project_point_to_pixel
            {
                p_x0 = vdivq_f32(p_x0, p_z0);
                p_y0 = vdivq_f32(p_y0, p_z0);

                p_x1 = vdivq_f32(p_x1, p_z1);
                p_y1 = vdivq_f32(p_y1, p_z1);

                distorte_x_y<dist>(p_x0, p_y0, &p_x0, &p_y0, c);
                distorte_x_y<dist>(p_x1, p_y1, &p_x1, &p_y1, c);

                p_x0 = vfmaq_f32(ppx, p_x0, fx);
                p_y0 = vfmaq_f32(ppy, p_y0, fy);

                p_x1 = vfmaq_f32(ppx, p_x1, fx);
                p_y1 = vfmaq_f32(ppy, p_y1, fy);
            }

            // float to int32
            {
                // zero the x and y if z is zero
                const uint32x4_t gt_zero = vcgtq_f32(depth0, zero);

                const auto u = vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(vaddq_f32(p_x0, half)), gt_zero));
                const auto v = vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(vaddq_f32(p_y0, half)), gt_zero));

                int32x4x2_t res_int;
                res_int.val[0] = vcvtq_s32_f32(u);
                res_int.val[1] = vcvtq_s32_f32(v);
                vst2q_s32(res, res_int);
                res += 8;
            }

            {
                // zero the x and y if z is zero
                const uint32x4_t gt_zero = vcgtq_f32(depth1, zero);

                const auto u = vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(vaddq_f32(p_x1, half)), gt_zero));
                const auto v = vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(vaddq_f32(p_y1, half)), gt_zero));

                int32x4x2_t res_int;
                res_int.val[0] = vcvtq_s32_f32(u);
                res_int.val[1] = vcvtq_s32_f32(v);
                vst2q_s32(res, res_int);
                res += 8;
            }
        }
    }

    align_neon_helper::align_neon_helper(const rs2_intrinsics &from, float depth_scale)
        : _depth(from),
          _depth_scale(depth_scale),
          _pixel_top_left_int(from.width * from.height),
          _pixel_bottom_right_int(from.width * from.height)
    {
    }

    void align_neon_helper::pre_compute_x_y_map_corners()
    {
        pre_compute_x_y_map(_pre_compute_map_x_top_left, _pre_compute_map_y_top_left, -0.5f);
        pre_compute_x_y_map(_pre_compute_map_x_bottom_right, _pre_compute_map_y_bottom_right, 0.5f);
    }

    void align_neon_helper::pre_compute_x_y_map(std::vector<float> &pre_compute_map_x,
                                                std::vector<float> &pre_compute_map_y,
                                                float offset)
    {
        pre_compute_map_x.resize(_depth.width * _depth.height);
        pre_compute_map_y.resize(_depth.width * _depth.height);

        for (int h = 0; h < _depth.height; ++h)
        {
            for (int w = 0; w < _depth.width; ++w)
            {
                const float pixel[] = {(float)w + offset, (float)h + offset};

                float x = (pixel[0] - _depth.ppx) / _depth.fx;
                float y = (pixel[1] - _depth.ppy) / _depth.fy;

                if (_depth.model == RS2_DISTORTION_INVERSE_BROWN_CONRADY)
                {
                    const float r2 = x * x + y * y;
                    const float f = 1.0f + (_depth.coeffs[0] + (_depth.coeffs[1] + _depth.coeffs[4] * r2) * r2) * r2;
                    const float ux = x * f + 2.0f * _depth.coeffs[2] * x * y + _depth.coeffs[3] * (r2 + 2.0f * x * x);
                    const float uy = y * f + 2.0f * _depth.coeffs[3] * x * y + _depth.coeffs[2] * (r2 + 2.0f * y * y);
                    x = ux;
                    y = uy;
                }

                pre_compute_map_x[h * _depth.width + w] = x;
                pre_compute_map_y[h * _depth.width + w] = y;
            }
        }
    }

    void align_neon_helper::align_depth_to_other(
        const uint16_t *z_pixels, uint16_t *dest, int bpp, const rs2_intrinsics &depth,
        const rs2_intrinsics &to, const rs2_extrinsics &from_to_other)
    {
        switch (to.model)
        {
        case RS2_DISTORTION_MODIFIED_BROWN_CONRADY:
            align_depth_to_other_neon<RS2_DISTORTION_MODIFIED_BROWN_CONRADY>(z_pixels, dest, depth, to, from_to_other);
            break;
        default:
            align_depth_to_other_neon(z_pixels, dest, depth, to, from_to_other);
            break;
        }
    }

    inline void align_neon_helper::move_depth_to_other(
        const uint16_t *z_pixels, uint16_t *dest, const rs2_intrinsics &to,
        const std::vector<librealsense::int2> &pixel_top_left_int,
        const std::vector<librealsense::int2> &pixel_bottom_right_int)
    {
        for (int y = 0; y < _depth.height; ++y)
        {
            for (int x = 0; x < _depth.width; ++x)
            {
                const auto depth_pixel_index = y * _depth.width + x;
                // Skip over depth pixels with the value of zero, we have no depth data so we will not write anything into our aligned images
                if (z_pixels[depth_pixel_index])
                {
                    for (int other_y = pixel_top_left_int[depth_pixel_index].y; other_y <= pixel_bottom_right_int[depth_pixel_index].y; ++other_y)
                    {
                        if (other_y < 0 || other_y >= to.height)
                            continue;

                        const auto y_width = other_y * to.width;
                        for (int other_x = pixel_top_left_int[depth_pixel_index].x; other_x <= pixel_bottom_right_int[depth_pixel_index].x; ++other_x)
                        {
                            if (other_x < 0 || other_x >= to.width)
                                continue;
                            const auto other_ind = y_width + other_x;

                            dest[other_ind] = dest[other_ind] ? std::min(dest[other_ind], z_pixels[depth_pixel_index]) : z_pixels[depth_pixel_index];
                        }
                    }
                }
            }
        }
    }

    void align_neon_helper::align_other_to_depth(
        const uint16_t *z_pixels, const uint8_t *source, uint8_t *dest,
        int bpp, const rs2_intrinsics &to, const rs2_extrinsics &from_to_other)
    {
        switch (to.model)
        {
        case RS2_DISTORTION_MODIFIED_BROWN_CONRADY:
        case RS2_DISTORTION_INVERSE_BROWN_CONRADY:
            align_other_to_depth_neon<RS2_DISTORTION_MODIFIED_BROWN_CONRADY>(z_pixels, source, dest, bpp, to, from_to_other);
            break;
        default:
            align_other_to_depth_neon(z_pixels, source, dest, bpp, to, from_to_other);
            break;
        }
    }

    template <rs2_distortion dist>
    inline void align_neon_helper::align_depth_to_other_neon(
        const uint16_t *z_pixels, uint16_t *dest, const rs2_intrinsics &depth,
        const rs2_intrinsics &to, const rs2_extrinsics &from_to_other)
    {
        // Map the top-left corner of the depth pixel onto the other image
        get_texture_map_neon<dist>(
            z_pixels, _depth_scale, _depth.height * _depth.width,
            _pre_compute_map_x_top_left.data(), _pre_compute_map_y_top_left.data(),
            (uint8_t *)_pixel_top_left_int.data(), to, from_to_other);

        float fov[2];
        rs2_fov(&depth, fov);
        float2 pixels_per_angle_depth = {(float)depth.width / fov[0], (float)depth.height / fov[1]};

        rs2_fov(&to, fov);
        float2 pixels_per_angle_target = {(float)to.width / fov[0], (float)to.height / fov[1]};

        if (pixels_per_angle_depth.x < pixels_per_angle_target.x || pixels_per_angle_depth.y < pixels_per_angle_target.y || is_special_resolution(depth, to))
        {
            // Map the bottom-right corner of the depth pixel onto the other image
            get_texture_map_neon<dist>(
                z_pixels, _depth_scale, _depth.height * _depth.width,
                _pre_compute_map_x_bottom_right.data(), _pre_compute_map_y_bottom_right.data(),
                (uint8_t *)_pixel_bottom_right_int.data(), to, from_to_other);

            move_depth_to_other(z_pixels, dest, to, _pixel_top_left_int, _pixel_bottom_right_int);
        }
        else
        {
            move_depth_to_other(z_pixels, dest, to, _pixel_top_left_int, _pixel_top_left_int);
        }
    }

    template <rs2_distortion dist>
    inline void align_neon_helper::align_other_to_depth_neon(
        const uint16_t *z_pixels, const uint8_t *source, uint8_t *dest,
        int bpp, const rs2_intrinsics &to, const rs2_extrinsics &from_to_other)
    {
        // Map the top-left corner of the depth pixel onto the other image
        get_texture_map_neon<dist>(
            z_pixels, _depth_scale, _depth.height * _depth.width,
            _pre_compute_map_x_top_left.data(), _pre_compute_map_y_top_left.data(),
            (uint8_t *)_pixel_top_left_int.data(), to, from_to_other);

        std::vector<int2> &bottom_right = _pixel_top_left_int;
        if (to.height < _depth.height && to.width < _depth.width)
        {
            // Map the bottom-right corner of the depth pixel onto the other image
            get_texture_map_neon<dist>(
                z_pixels, _depth_scale, _depth.height * _depth.width,
                _pre_compute_map_x_bottom_right.data(), _pre_compute_map_y_bottom_right.data(),
                (uint8_t *)_pixel_bottom_right_int.data(), to, from_to_other);

            bottom_right = _pixel_bottom_right_int;
        }

        switch (bpp)
        {
        case 1:
            move_other_to_depth(
                z_pixels,
                reinterpret_cast<const bytes<1> *>(source), reinterpret_cast<bytes<1> *>(dest),
                to, _pixel_top_left_int, bottom_right);
            break;
        case 2:
            move_other_to_depth(
                z_pixels,
                reinterpret_cast<const bytes<2> *>(source), reinterpret_cast<bytes<2> *>(dest),
                to, _pixel_top_left_int, bottom_right);
            break;
        case 3:
            move_other_to_depth(
                z_pixels,
                reinterpret_cast<const bytes<3> *>(source), reinterpret_cast<bytes<3> *>(dest),
                to, _pixel_top_left_int, bottom_right);
            break;
        case 4:
            move_other_to_depth(
                z_pixels,
                reinterpret_cast<const bytes<4> *>(source), reinterpret_cast<bytes<4> *>(dest),
                to, _pixel_top_left_int, bottom_right);
            break;
        default:
            break;
        }
    }

    template <class T>
    void align_neon_helper::move_other_to_depth(
        const uint16_t *z_pixels, const T *source, T *dest,
        const rs2_intrinsics &to,
        const std::vector<librealsense::int2> &pixel_top_left_int,
        const std::vector<librealsense::int2> &pixel_bottom_right_int)
    {
        // Iterate over the pixels of the depth image
        for (int y = 0; y < _depth.height; ++y)
        {
            for (int x = 0; x < _depth.width; ++x)
            {
                const auto depth_pixel_index = y * _depth.width + x;
                // Skip over depth pixels with the value of zero, we have no depth data so we will not write anything into our aligned images
                if (z_pixels[depth_pixel_index])
                {
                    for (int other_y = pixel_top_left_int[depth_pixel_index].y; other_y <= pixel_bottom_right_int[depth_pixel_index].y; ++other_y)
                    {
                        if (other_y < 0 || other_y >= to.height)
                            continue;

                        const auto y_width = other_y * to.width;
                        for (int other_x = pixel_top_left_int[depth_pixel_index].x; other_x <= pixel_bottom_right_int[depth_pixel_index].x; ++other_x)
                        {
                            if (other_x < 0 || other_x >= to.width)
                                continue;
                            const auto other_ind = y_width + other_x;

                            dest[depth_pixel_index] = source[other_ind];
                        }
                    }
                }
            }
        }
    }

    void align_neon::reset_cache(rs2_stream from, rs2_stream to)
    {
        _neon_helper = nullptr;
    }

    void align_neon::align_z_to_other(
        rs2::video_frame &aligned, const rs2::video_frame &depth,
        const rs2::video_stream_profile &other_profile, float z_scale)
    {
        uint8_t *aligned_data = reinterpret_cast<uint8_t *>(const_cast<void *>(aligned.get_data()));
        auto aligned_profile = aligned.get_profile().as<rs2::video_stream_profile>();
        memset(aligned_data, 0, aligned_profile.height() * aligned_profile.width() * aligned.get_bytes_per_pixel());

        auto depth_profile = depth.get_profile().as<rs2::video_stream_profile>();

        auto z_intrin = depth_profile.get_intrinsics();
        auto other_intrin = other_profile.get_intrinsics();
        auto z_to_other = depth_profile.get_extrinsics_to(other_profile);

        auto z_pixels = reinterpret_cast<const uint16_t *>(depth.get_data());

        if (_neon_helper == nullptr)
        {
            _neon_helper = std::make_shared<align_neon_helper>(z_intrin, z_scale);
            _neon_helper->pre_compute_x_y_map_corners();
        }
        _neon_helper->align_depth_to_other(
            z_pixels, reinterpret_cast<uint16_t *>(aligned_data), 2,
            z_intrin, other_intrin, z_to_other);
    }

    void align_neon::align_other_to_z(
        rs2::video_frame &aligned, const rs2::video_frame &depth,
        const rs2::video_frame &other, float z_scale)
    {
        uint8_t *aligned_data = reinterpret_cast<uint8_t *>(const_cast<void *>(aligned.get_data()));
        auto aligned_profile = aligned.get_profile().as<rs2::video_stream_profile>();
        memset(aligned_data, 0, aligned_profile.height() * aligned_profile.width() * aligned.get_bytes_per_pixel());

        auto depth_profile = depth.get_profile().as<rs2::video_stream_profile>();
        auto other_profile = other.get_profile().as<rs2::video_stream_profile>();

        auto z_intrin = depth_profile.get_intrinsics();
        auto other_intrin = other_profile.get_intrinsics();
        auto z_to_other = depth_profile.get_extrinsics_to(other_profile);

        auto z_pixels = reinterpret_cast<const uint16_t *>(depth.get_data());
        auto other_pixels = reinterpret_cast<const uint8_t *>(other.get_data());

        if (_neon_helper == nullptr)
        {
            _neon_helper = std::make_shared<align_neon_helper>(z_intrin, z_scale);
            _neon_helper->pre_compute_x_y_map_corners();
        }

        _neon_helper->align_other_to_depth(
            z_pixels, other_pixels, aligned_data, other.get_bytes_per_pixel(),
            other_intrin, z_to_other);
    }
} // namespace librealsense
#endif
