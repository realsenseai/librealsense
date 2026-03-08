// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>

#include "neon-pointcloud.h"

#include <iostream>

#if defined(__ARM_NEON)  && ! defined ANDROID
#include <arm_neon.h>

namespace librealsense
{
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
        const auto r2 = vfmaq_f32(vmulq_f32(x, x), y, y);
        // f = 1 + c[0] * r2 + c[1] * r2 ^ 2 + c[4] * r2 ^ 3
        //   = 1 + r2 * (c[0] + r2 * (c[1] + r2 * c[4]))
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

    pointcloud_neon::pointcloud_neon() : pointcloud("Pointcloud (NEON)") {}

    void pointcloud_neon::preprocess()
    {
        _pre_compute_map_x.resize(_depth_intrinsics->width * _depth_intrinsics->height);
        _pre_compute_map_y.resize(_depth_intrinsics->width * _depth_intrinsics->height);

        for (int h = 0; h < _depth_intrinsics->height; ++h)
        {
            for (int w = 0; w < _depth_intrinsics->width; ++w)
            {
                const float pixel[] = {(float)w, (float)h};

                float x = (pixel[0] - _depth_intrinsics->ppx) / _depth_intrinsics->fx;
                float y = (pixel[1] - _depth_intrinsics->ppy) / _depth_intrinsics->fy;

                if (_depth_intrinsics->model == RS2_DISTORTION_INVERSE_BROWN_CONRADY)
                {
                    const float r2 = x * x + y * y;
                    const float f = 1.0f + _depth_intrinsics->coeffs[0] * r2 + _depth_intrinsics->coeffs[1] * r2 * r2 + _depth_intrinsics->coeffs[4] * r2 * r2 * r2;
                    const float ux = x * f + 2.0f * _depth_intrinsics->coeffs[2] * x * y + _depth_intrinsics->coeffs[3] * (r2 + 2.0f * x * x);
                    const float uy = y * f + 2.0f * _depth_intrinsics->coeffs[3] * x * y + _depth_intrinsics->coeffs[2] * (r2 + 2.0f * y * y);
                    x = ux;
                    y = uy;
                }

                _pre_compute_map_x[h * _depth_intrinsics->width + w] = x;
                _pre_compute_map_y[h * _depth_intrinsics->width + w] = y;
            }
        }
    }

    const float3 *pointcloud_neon::depth_to_points(rs2::points output,
                                                   const rs2_intrinsics &depth_intrinsics,
                                                   const rs2::depth_frame &depth_frame)
    {
        auto depth_image = (const uint16_t *)depth_frame.get_data();

        float *pre_compute_x = _pre_compute_map_x.data();
        float *pre_compute_y = _pre_compute_map_y.data();

        const uint32_t size = depth_intrinsics.height * depth_intrinsics.width;

        auto points = (float *)output.get_vertices();
        const auto scale = vdupq_n_f32(depth_frame.get_units());

        for (unsigned int i = 0; i < size; i += 8)
        {
            const auto x0 = vld1q_f32(pre_compute_x + i);
            const auto x1 = vld1q_f32(pre_compute_x + i + 4);

            const auto y0 = vld1q_f32(pre_compute_y + i);
            const auto y1 = vld1q_f32(pre_compute_y + i + 4);

            const auto d = vld1q_u16(depth_image + i);
            const auto depth0 = vmulq_f32(vcvtq_f32_s32((int32x4_t)vmovl_u16(vget_low_u16(d))), scale);
            const auto depth1 = vmulq_f32(vcvtq_f32_s32((int32x4_t)vmovl_u16(vget_high_u16(d))), scale);

            // calculate 3D points
            float32x4x3_t xyz0;
            xyz0.val[0] = vmulq_f32(depth0, x0);
            xyz0.val[1] = vmulq_f32(depth0, y0);
            xyz0.val[2] = depth0;
            vst3q_f32(&points[0], xyz0);

            float32x4x3_t xyz1;
            xyz1.val[0] = vmulq_f32(depth1, x1);
            xyz1.val[1] = vmulq_f32(depth1, y1);
            xyz1.val[2] = depth1;
            vst3q_f32(&points[12], xyz1);

            points += 24;
        }
        return (float3 *)output.get_vertices();
    }

    template <rs2_distortion dist>
    void pointcloud_neon::get_texture_map_neon(float2 *texture_map,
                                               const float3 *points,
                                               const unsigned int width,
                                               const unsigned int height,
                                               const rs2_intrinsics &other_intrinsics,
                                               const rs2_extrinsics &extr,
                                               float2 *pixels_ptr)
    {
        auto point = reinterpret_cast<const float *>(points);
        auto res = reinterpret_cast<float *>(texture_map);
        auto res1 = reinterpret_cast<float *>(pixels_ptr);

        float32x4_t r[9];
        float32x4_t t[3];
        float32x4_t c[5];
        for (int i = 0; i < 9; ++i)
        {
            r[i] = vdupq_n_f32(extr.rotation[i]);
        }
        for (int i = 0; i < 3; ++i)
        {
            t[i] = vdupq_n_f32(extr.translation[i]);
        }
        for (int i = 0; i < 5; ++i)
        {
            c[i] = vdupq_n_f32(other_intrinsics.coeffs[i]);
        }
        const auto fx = vdupq_n_f32(other_intrinsics.fx);
        const auto fy = vdupq_n_f32(other_intrinsics.fy);
        const auto ppx = vdupq_n_f32(other_intrinsics.ppx);
        const auto ppy = vdupq_n_f32(other_intrinsics.ppy);
        const auto w = vdupq_n_f32(float(other_intrinsics.width));
        const auto h = vdupq_n_f32(float(other_intrinsics.height));
        const auto zero = vdupq_n_f32(0.0f);

        const uint32_t size = height * width * 3;
        for (uint32_t i = 0; i < size; i+=12)
        {
            // load 4 points (x,y,z)
            const float32x4x3_t xyz = vld3q_f32(point + i);

            // transform to other
            auto p_x = vfmaq_f32(vfmaq_f32(vfmaq_f32(t[0], r[6], xyz.val[2]), r[3], xyz.val[1]), r[0], xyz.val[0]);
            auto p_y = vfmaq_f32(vfmaq_f32(vfmaq_f32(t[1], r[7], xyz.val[2]), r[4], xyz.val[1]), r[1], xyz.val[0]);
            auto p_z = vfmaq_f32(vfmaq_f32(vfmaq_f32(t[2], r[8], xyz.val[2]), r[5], xyz.val[1]), r[2], xyz.val[0]);

            p_x = vdivq_f32(p_x, p_z);
            p_y = vdivq_f32(p_y, p_z);

            distorte_x_y<dist>(p_x, p_y, &p_x, &p_y, c);

            p_x = vfmaq_f32(ppx, p_x, fx);
            p_y = vfmaq_f32(ppy, p_y, fy);

            // zero the x and y if original depth z is zero
            {
                const uint32x4_t gt_zero = vcgtq_f32(xyz.val[2], zero);
                p_x = vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(p_x), gt_zero));
                p_y = vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(p_y), gt_zero));
            }

            // texture_map
            {
                float32x4x2_t xy;
                xy.val[0] = p_x;
                xy.val[1] = p_y;
                vst2q_f32(res1, xy);
                res1 += 8;
            }

            // pixels_ptr
            {
                float32x4x2_t xy;
                xy.val[0] = vdivq_f32(p_x, w);
                xy.val[1] = vdivq_f32(p_y, h);
                vst2q_f32(res, xy);
                res += 8;
            }
        }
    }

    void pointcloud_neon::get_texture_map(rs2::points output,
                                          const float3 *points,
                                          const unsigned int width,
                                          const unsigned int height,
                                          const rs2_intrinsics &other_intrinsics,
                                          const rs2_extrinsics &extr,
                                          float2 *pixels_ptr)
    {
        if (other_intrinsics.model == RS2_DISTORTION_MODIFIED_BROWN_CONRADY)
        {
            get_texture_map_neon<RS2_DISTORTION_MODIFIED_BROWN_CONRADY>(
                (float2 *)output.get_texture_coordinates(),
                points,
                width,
                height,
                other_intrinsics,
                extr,
                pixels_ptr);
        }
        else
        {
            get_texture_map_neon<RS2_DISTORTION_NONE>(
                (float2 *)output.get_texture_coordinates(),
                points,
                width,
                height,
                other_intrinsics,
                extr,
                pixels_ptr);
        }
    }
}
#endif
