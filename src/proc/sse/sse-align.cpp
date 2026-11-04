// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.
#ifdef __SSSE3__

#include "sse-align.h"
#include <tmmintrin.h> // For SSE3 intrinsic used in unpack_yuy2_sse
#include "../include/librealsense2/hpp/rs_sensor.hpp"
#include "../include/librealsense2/hpp/rs_processing.hpp"

#include "core/video.h"
#include "proc/synthetic-stream.h"
#include "environment.h"
#include "stream.h"

using namespace librealsense;

template<int N> struct bytes { uint8_t b[N]; };

bool is_special_resolution(const rs2_intrinsics& depth, const rs2_intrinsics& to)
{
    if ((depth.width == 640 && depth.height == 240 && to.width == 320 && to.height == 180) ||
        (depth.width == 640 && depth.height == 480 && to.width == 640 && to.height == 360))
        return true;
    return false;
}

template<rs2_distortion dist>
inline void distorte_x_y(const __m128 & x, const __m128 & y, __m128 * distorted_x, __m128 * distorted_y, const rs2_intrinsics& to)
{
    *distorted_x = x;
    *distorted_y = y;
}
template<>
inline void distorte_x_y<RS2_DISTORTION_MODIFIED_BROWN_CONRADY>(const __m128& x, const __m128& y, __m128* distorted_x, __m128* distorted_y, const rs2_intrinsics& to)
{
    __m128 c[5];
    auto one = _mm_set_ps1(1);
    auto two = _mm_set_ps1(2);

    for (int i = 0; i < 5; ++i)
    {
        c[i] = _mm_set_ps1(to.coeffs[i]);
    }
    auto r2_0 = _mm_add_ps(_mm_mul_ps(x, x), _mm_mul_ps(y, y));
    auto r3_0 = _mm_add_ps(_mm_mul_ps(c[1], _mm_mul_ps(r2_0, r2_0)), _mm_mul_ps(c[4], _mm_mul_ps(r2_0, _mm_mul_ps(r2_0, r2_0))));
    auto f_0 = _mm_add_ps(one, _mm_add_ps(_mm_mul_ps(c[0], r2_0), r3_0));

    auto x_f0 = _mm_mul_ps(x, f_0);
    auto y_f0 = _mm_mul_ps(y, f_0);

    auto r4_0 = _mm_mul_ps(c[3], _mm_add_ps(r2_0, _mm_mul_ps(two, _mm_mul_ps(x_f0, x_f0))));
    auto d_x0 = _mm_add_ps(x_f0, _mm_add_ps(_mm_mul_ps(two, _mm_mul_ps(c[2], _mm_mul_ps(x_f0, y_f0))), r4_0));

    auto r5_0 = _mm_mul_ps(c[2], _mm_add_ps(r2_0, _mm_mul_ps(two, _mm_mul_ps(y_f0, y_f0))));
    auto d_y0 = _mm_add_ps(y_f0, _mm_add_ps(_mm_mul_ps(two, _mm_mul_ps(c[3], _mm_mul_ps(x_f0, y_f0))), r5_0));

    *distorted_x = d_x0;
    *distorted_y = d_y0;
}


template<rs2_distortion dist>
inline void get_texture_map_sse(const uint16_t * depth,
    float depth_scale,
    const unsigned int size,
    const float * pre_compute_x, const float * pre_compute_y,
    uint8_t * pixels_ptr_int,
    const rs2_intrinsics& to,
    const rs2_extrinsics& from_to_other)
{
    //mask for shuffle
    const __m128i mask0 = _mm_set_epi8((char)0xff, (char)0xff, (char)7, (char)6, (char)0xff, (char)0xff, (char)5, (char)4,
        (char)0xff, (char)0xff, (char)3, (char)2, (char)0xff, (char)0xff, (char)1, (char)0);
    const __m128i mask1 = _mm_set_epi8((char)0xff, (char)0xff, (char)15, (char)14, (char)0xff, (char)0xff, (char)13, (char)12,
        (char)0xff, (char)0xff, (char)11, (char)10, (char)0xff, (char)0xff, (char)9, (char)8);

    auto scale = _mm_set_ps1(depth_scale);

    auto mapx = pre_compute_x;
    auto mapy = pre_compute_y;

    auto res = reinterpret_cast<__m128i*>(pixels_ptr_int);

    __m128 r[9];
    __m128 t[3];
    __m128 c[5];

    for (int i = 0; i < 9; ++i)
    {
        r[i] = _mm_set_ps1(from_to_other.rotation[i]);
    }
    for (int i = 0; i < 3; ++i)
    {
        t[i] = _mm_set_ps1(from_to_other.translation[i]);
    }
    for (int i = 0; i < 5; ++i)
    {
        c[i] = _mm_set_ps1(to.coeffs[i]);
    }
    auto zero = _mm_set_ps1(0);
    auto fx = _mm_set_ps1(to.fx);
    auto fy = _mm_set_ps1(to.fy);
    auto ppx = _mm_set_ps1(to.ppx);
    auto ppy = _mm_set_ps1(to.ppy);

    for (unsigned int i = 0; i < size; i += 8)
    {
        auto x0 = _mm_load_ps(mapx + i);
        auto x1 = _mm_load_ps(mapx + i + 4);

        auto y0 = _mm_load_ps(mapy + i);
        auto y1 = _mm_load_ps(mapy + i + 4);


        __m128i d = _mm_load_si128((__m128i const*)(depth + i));        //d7 d7 d6 d6 d5 d5 d4 d4 d3 d3 d2 d2 d1 d1 d0 d0

                                                                        //split the depth pixel to 2 registers of 4 floats each
        __m128i d0 = _mm_shuffle_epi8(d, mask0);        // 00 00 d3 d3 00 00 d2 d2 00 00 d1 d1 00 00 d0 d0
        __m128i d1 = _mm_shuffle_epi8(d, mask1);        // 00 00 d7 d7 00 00 d6 d6 00 00 d5 d5 00 00 d4 d4

        __m128 depth0 = _mm_cvtepi32_ps(d0); //convert depth to float
        __m128 depth1 = _mm_cvtepi32_ps(d1); //convert depth to float

        depth0 = _mm_mul_ps(depth0, scale);
        depth1 = _mm_mul_ps(depth1, scale);

        auto p0x = _mm_mul_ps(depth0, x0);
        auto p0y = _mm_mul_ps(depth0, y0);

        auto p1x = _mm_mul_ps(depth1, x1);
        auto p1y = _mm_mul_ps(depth1, y1);

        auto p_x0 = _mm_add_ps(_mm_mul_ps(r[0], p0x), _mm_add_ps(_mm_mul_ps(r[3], p0y), _mm_add_ps(_mm_mul_ps(r[6], depth0), t[0])));
        auto p_y0 = _mm_add_ps(_mm_mul_ps(r[1], p0x), _mm_add_ps(_mm_mul_ps(r[4], p0y), _mm_add_ps(_mm_mul_ps(r[7], depth0), t[1])));
        auto p_z0 = _mm_add_ps(_mm_mul_ps(r[2], p0x), _mm_add_ps(_mm_mul_ps(r[5], p0y), _mm_add_ps(_mm_mul_ps(r[8], depth0), t[2])));

        auto p_x1 = _mm_add_ps(_mm_mul_ps(r[0], p1x), _mm_add_ps(_mm_mul_ps(r[3], p1y), _mm_add_ps(_mm_mul_ps(r[6], depth1), t[0])));
        auto p_y1 = _mm_add_ps(_mm_mul_ps(r[1], p1x), _mm_add_ps(_mm_mul_ps(r[4], p1y), _mm_add_ps(_mm_mul_ps(r[7], depth1), t[1])));
        auto p_z1 = _mm_add_ps(_mm_mul_ps(r[2], p1x), _mm_add_ps(_mm_mul_ps(r[5], p1y), _mm_add_ps(_mm_mul_ps(r[8], depth1), t[2])));

        p_x0 = _mm_div_ps(p_x0, p_z0);
        p_y0 = _mm_div_ps(p_y0, p_z0);

        p_x1 = _mm_div_ps(p_x1, p_z1);
        p_y1 = _mm_div_ps(p_y1, p_z1);

        distorte_x_y<dist>(p_x0, p_y0, &p_x0, &p_y0, to);
        distorte_x_y<dist>(p_x1, p_y1, &p_x1, &p_y1, to);

        //zero the x and y if z is zero
        auto cmp = _mm_cmpneq_ps(depth0, zero);
        p_x0 = _mm_and_ps(_mm_add_ps(_mm_mul_ps(p_x0, fx), ppx), cmp);
        p_y0 = _mm_and_ps(_mm_add_ps(_mm_mul_ps(p_y0, fy), ppy), cmp);


        p_x1 = _mm_add_ps(_mm_mul_ps(p_x1, fx), ppx);
        p_y1 = _mm_add_ps(_mm_mul_ps(p_y1, fy), ppy);

        cmp = _mm_cmpneq_ps(depth0, zero);
        auto half = _mm_set_ps1(0.5);
        auto u_round0 = _mm_and_ps(_mm_add_ps(p_x0, half), cmp);
        auto v_round0 = _mm_and_ps(_mm_add_ps(p_y0, half), cmp);

        auto uuvv1_0 = _mm_shuffle_ps(u_round0, v_round0, _MM_SHUFFLE(1, 0, 1, 0));
        auto uuvv2_0 = _mm_shuffle_ps(u_round0, v_round0, _MM_SHUFFLE(3, 2, 3, 2));

        auto res1_0 = _mm_shuffle_ps(uuvv1_0, uuvv1_0, _MM_SHUFFLE(3, 1, 2, 0));
        auto res2_0 = _mm_shuffle_ps(uuvv2_0, uuvv2_0, _MM_SHUFFLE(3, 1, 2, 0));

        auto res1_int0 = _mm_cvttps_epi32(res1_0);
        auto res2_int0 = _mm_cvttps_epi32(res2_0);

        _mm_stream_si128(&res[0], res1_int0);
        _mm_stream_si128(&res[1], res2_int0);
        res += 2;

        cmp = _mm_cmpneq_ps(depth1, zero);
        auto u_round1 = _mm_and_ps(_mm_add_ps(p_x1, half), cmp);
        auto v_round1 = _mm_and_ps(_mm_add_ps(p_y1, half), cmp);

        auto uuvv1_1 = _mm_shuffle_ps(u_round1, v_round1, _MM_SHUFFLE(1, 0, 1, 0));
        auto uuvv2_1 = _mm_shuffle_ps(u_round1, v_round1, _MM_SHUFFLE(3, 2, 3, 2));

        auto res1 = _mm_shuffle_ps(uuvv1_1, uuvv1_1, _MM_SHUFFLE(3, 1, 2, 0));
        auto res2 = _mm_shuffle_ps(uuvv2_1, uuvv2_1, _MM_SHUFFLE(3, 1, 2, 0));

        auto res1_int1 = _mm_cvttps_epi32(res1);
        auto res2_int1 = _mm_cvttps_epi32(res2);

        _mm_stream_si128(&res[0], res1_int1);
        _mm_stream_si128(&res[1], res2_int1);
        res += 2;
    }
}

image_transform::image_transform(const rs2_intrinsics& from, float depth_scale)
    :_depth(from),
    _depth_scale(depth_scale),
    _pixel_top_left_int(from.width*from.height),
    _pixel_bottom_right_int(from.width*from.height)
{
}

void image_transform::pre_compute_x_y_map_corners()
{
    pre_compute_x_y_map(_pre_compute_map_x_top_left, _pre_compute_map_y_top_left, -0.5f);
    pre_compute_x_y_map(_pre_compute_map_x_bottom_right, _pre_compute_map_y_bottom_right, 0.5f);
}

void image_transform::pre_compute_x_y_map(std::vector<float>& pre_compute_map_x,
    std::vector<float>& pre_compute_map_y,
    float offset)
{
    pre_compute_map_x.resize(_depth.width*_depth.height);
    pre_compute_map_y.resize(_depth.width*_depth.height);

    for (int h = 0; h < _depth.height; ++h)
    {
        for (int w = 0; w < _depth.width; ++w)
        {
            const float pixel[] = { (float)w + offset, (float)h + offset };

            float x = (pixel[0] - _depth.ppx) / _depth.fx;
            float y = (pixel[1] - _depth.ppy) / _depth.fy;

            if (_depth.model == RS2_DISTORTION_INVERSE_BROWN_CONRADY)
            {
                float r2 = x * x + y * y;
                float f = 1 + _depth.coeffs[0] * r2 + _depth.coeffs[1] * r2*r2 + _depth.coeffs[4] * r2*r2*r2;
                float ux = x * f + 2 * _depth.coeffs[2] * x*y + _depth.coeffs[3] * (r2 + 2 * x*x);
                float uy = y * f + 2 * _depth.coeffs[3] * x*y + _depth.coeffs[2] * (r2 + 2 * y*y);
                x = ux;
                y = uy;
            }

            pre_compute_map_x[h*_depth.width + w] = x;
            pre_compute_map_y[h*_depth.width + w] = y;
        }
    }
}

void image_transform::align_depth_to_other(const uint16_t* z_pixels, uint16_t* dest, int bpp, const rs2_intrinsics& depth, const rs2_intrinsics& to,
    const rs2_extrinsics& from_to_other)
{
    switch (to.model)
    {
    case RS2_DISTORTION_MODIFIED_BROWN_CONRADY:
        align_depth_to_other_sse<RS2_DISTORTION_MODIFIED_BROWN_CONRADY>(z_pixels, dest, depth, to, from_to_other);
        break;
    default:
        align_depth_to_other_sse(z_pixels, dest, depth, to, from_to_other);
        break;
    }
}

inline void image_transform::move_depth_to_other(const uint16_t* z_pixels, uint16_t* dest, const rs2_intrinsics& to,
    const std::vector<librealsense::int2>& pixel_top_left_int,
    const std::vector<librealsense::int2>& pixel_bottom_right_int)
{
    for (int y = 0; y < _depth.height; ++y)
    {
        for (int x = 0; x < _depth.width; ++x)
        {
            auto depth_pixel_index = y * _depth.width + x;
            // Skip over depth pixels with the value of zero, we have no depth data so we will not write anything into our aligned images
            if (z_pixels[depth_pixel_index])
            {
                for (int other_y = pixel_top_left_int[depth_pixel_index].y; other_y <= pixel_bottom_right_int[depth_pixel_index].y; ++other_y)
                {
                    for (int other_x = pixel_top_left_int[depth_pixel_index].x; other_x <= pixel_bottom_right_int[depth_pixel_index].x; ++other_x)
                    {
                        if (other_x < 0 || other_y < 0 || other_x >= to.width || other_y >= to.height)
                            continue;
                        auto other_ind = other_y * to.width + other_x;

                        dest[other_ind] = dest[other_ind] ? std::min(dest[other_ind], z_pixels[depth_pixel_index]) : z_pixels[depth_pixel_index];
                    }
                }
            }
        }
    }
}

void image_transform::align_other_to_depth(const uint16_t* z_pixels, const uint8_t * source, uint8_t * dest, int bpp, const rs2_intrinsics& to,
    const rs2_extrinsics& from_to_other)
{
    switch (to.model)
    {
    case RS2_DISTORTION_MODIFIED_BROWN_CONRADY:
    case RS2_DISTORTION_INVERSE_BROWN_CONRADY:
        align_other_to_depth_sse<RS2_DISTORTION_MODIFIED_BROWN_CONRADY>(z_pixels, source, dest, bpp, to, from_to_other);
        break;
    default:
        align_other_to_depth_sse(z_pixels, source, dest, bpp, to, from_to_other);
        break;
    }
}


template<rs2_distortion dist>
inline void image_transform::align_depth_to_other_sse(const uint16_t * z_pixels, uint16_t * dest, const rs2_intrinsics& depth, const rs2_intrinsics& to,
    const rs2_extrinsics& from_to_other)
{
    get_texture_map_sse<dist>(z_pixels, _depth_scale, _depth.height*_depth.width, _pre_compute_map_x_top_left.data(),
        _pre_compute_map_y_top_left.data(), (uint8_t *)_pixel_top_left_int.data(), to, from_to_other);

    float fov[2];
    rs2_fov(&depth, fov);
    float2 pixels_per_angle_depth = { (float)depth.width / fov[0], (float)depth.height / fov[1] };

    rs2_fov(&to, fov);
    float2 pixels_per_angle_target = { (float)to.width / fov[0], (float)to.height / fov[1] };

    if (pixels_per_angle_depth.x < pixels_per_angle_target.x || pixels_per_angle_depth.y < pixels_per_angle_target.y || is_special_resolution(depth, to))
    {
        get_texture_map_sse<dist>(z_pixels, _depth_scale, _depth.height*_depth.width, _pre_compute_map_x_bottom_right.data(),
            _pre_compute_map_y_bottom_right.data(), (uint8_t *)_pixel_bottom_right_int.data(), to, from_to_other);

        move_depth_to_other(z_pixels, dest, to, _pixel_top_left_int, _pixel_bottom_right_int);
    }
    else
    {
        move_depth_to_other(z_pixels, dest, to, _pixel_top_left_int, _pixel_top_left_int);
    }

}

template<rs2_distortion dist>
inline void image_transform::align_other_to_depth_sse(const uint16_t * z_pixels, const uint8_t * source, uint8_t * dest, int bpp, const rs2_intrinsics& to,
    const rs2_extrinsics& from_to_other)
{
    get_texture_map_sse<dist>(z_pixels, _depth_scale, _depth.height*_depth.width, _pre_compute_map_x_top_left.data(),
        _pre_compute_map_y_top_left.data(), (uint8_t *)_pixel_top_left_int.data(), to, from_to_other);

    std::vector<int2>& bottom_right = _pixel_top_left_int;
    if (to.height < _depth.height && to.width < _depth.width)
    {
        get_texture_map_sse<dist>(z_pixels, _depth_scale, _depth.height*_depth.width, _pre_compute_map_x_bottom_right.data(),
            _pre_compute_map_y_bottom_right.data(), (uint8_t *)_pixel_bottom_right_int.data(), to, from_to_other);

        bottom_right = _pixel_bottom_right_int;
    }

    switch (bpp)
    {
    case 1:
        move_other_to_depth(z_pixels, reinterpret_cast<const bytes<1>*>(source), reinterpret_cast<bytes<1>*>(dest), to,
            _pixel_top_left_int, bottom_right);
        break;
    case 2:
        move_other_to_depth(z_pixels, reinterpret_cast<const bytes<2>*>(source), reinterpret_cast<bytes<2>*>(dest), to,
            _pixel_top_left_int, bottom_right);
        break;
    case 3:
        move_other_to_depth(z_pixels, reinterpret_cast<const bytes<3>*>(source), reinterpret_cast<bytes<3>*>(dest), to,
            _pixel_top_left_int, bottom_right);
        break;
    case 4:
        move_other_to_depth(z_pixels, reinterpret_cast<const bytes<4>*>(source), reinterpret_cast<bytes<4>*>(dest), to,
            _pixel_top_left_int, bottom_right);
        break;
    default:
        break;
    }
}

template<class T >
void image_transform::move_other_to_depth(const uint16_t* z_pixels,
    const T* source,
    T* dest, const rs2_intrinsics& to,
    const std::vector<librealsense::int2>& pixel_top_left_int,
    const std::vector<librealsense::int2>& pixel_bottom_right_int)
{
    // Iterate over the pixels of the depth image
    for (int y = 0; y < _depth.height; ++y)
    {
        for (int x = 0; x < _depth.width; ++x)
        {
            auto depth_pixel_index = y * _depth.width + x;
            // Skip over depth pixels with the value of zero, we have no depth data so we will not write anything into our aligned images
            if (z_pixels[depth_pixel_index])
            {
                for (int other_y = pixel_top_left_int[depth_pixel_index].y; other_y <= pixel_bottom_right_int[depth_pixel_index].y; ++other_y)
                {
                    for (int other_x = pixel_top_left_int[depth_pixel_index].x; other_x <= pixel_bottom_right_int[depth_pixel_index].x; ++other_x)
                    {
                        if (other_x < 0 || other_y < 0 || other_x >= to.width || other_y >= to.height)
                            continue;
                        auto other_ind = other_y * to.width + other_x;

                        dest[depth_pixel_index] = source[other_ind];
                    }
                }
            }
        }
    }
}

void align_sse::reset_cache(rs2_stream from, rs2_stream to)
{
    _stream_transform = nullptr;
}

void align_sse::align_z_to_other(rs2::video_frame& aligned, const rs2::video_frame& depth, const rs2::video_stream_profile& other_profile, float z_scale)
{
    uint8_t * aligned_data = reinterpret_cast<uint8_t *>(const_cast<void*>(aligned.get_data()));
    auto aligned_profile = aligned.get_profile().as<rs2::video_stream_profile>();
    memset(aligned_data, 0, aligned_profile.height() * aligned_profile.width() * aligned.get_bytes_per_pixel());

    auto depth_profile = depth.get_profile().as<rs2::video_stream_profile>();

    auto z_intrin = depth_profile.get_intrinsics();
    auto other_intrin = other_profile.get_intrinsics();
    auto z_to_other = depth_profile.get_extrinsics_to(other_profile);

    auto z_pixels = reinterpret_cast<const uint16_t*>(depth.get_data());

    if (_stream_transform == nullptr)
    {
        _stream_transform = std::make_shared<image_transform>(z_intrin, z_scale);
        _stream_transform->pre_compute_x_y_map_corners();
    }
    _stream_transform->align_depth_to_other(z_pixels, reinterpret_cast<uint16_t*>(aligned_data), 2, z_intrin, other_intrin, z_to_other);
}

void align_sse::align_other_to_z(rs2::video_frame& aligned, const rs2::video_frame& depth, const rs2::video_frame& other, float z_scale)
{
    uint8_t * aligned_data = reinterpret_cast<uint8_t *>(const_cast<void*>(aligned.get_data()));
    auto aligned_profile = aligned.get_profile().as<rs2::video_stream_profile>();
    memset(aligned_data, 0, aligned_profile.height() * aligned_profile.width() * aligned.get_bytes_per_pixel());

    auto depth_profile = depth.get_profile().as<rs2::video_stream_profile>();
    auto other_profile = other.get_profile().as<rs2::video_stream_profile>();

    auto z_intrin = depth_profile.get_intrinsics();
    auto other_intrin = other_profile.get_intrinsics();
    auto z_to_other = depth_profile.get_extrinsics_to(other_profile);

    auto z_pixels = reinterpret_cast<const uint16_t*>(depth.get_data());
    auto other_pixels = reinterpret_cast<const uint8_t *>(other.get_data());

    if (_stream_transform == nullptr)
    {
        _stream_transform = std::make_shared<image_transform>(z_intrin, z_scale);
        _stream_transform->pre_compute_x_y_map_corners();
    }

    _stream_transform->align_other_to_depth(z_pixels, other_pixels, aligned_data, other.get_bytes_per_pixel(), other_intrin, z_to_other);
}
#endif
