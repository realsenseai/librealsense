// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include "image-neon.h"

#ifndef ANDROID
    #if defined(__ARM_NEON) && defined(BUILD_WITH_NEON)
    #include <arm_neon.h>

    namespace librealsense
    {
        template<rs2_format FORMAT>
        void unpack_yuy2_neon(uint8_t * const d[], const uint8_t * s, int n)
        {
            assert(n % 16 == 0); // All currently supported color resolutions are multiples of 16 pixels. Could easily extend support to other resolutions by copying final n<16 pixels into a zero-padded buffer and recursively calling self for final iteration.

            for (int i = 0; i < n; i+=16)
            {
                // Load 16 pixels
                const uint8x8x4_t yuyv = vld4_u8(&s[i * 2]);
                // yuyv.val[0] = y0, yuyv.val[1] = u, yuyv.val[2] = y1, yuyv.val[3] = v

                if (FORMAT == RS2_FORMAT_Y8)
                {
                    const uint8x16_t y8_0_7 = vcombine_u8(yuyv.val[0], yuyv.val[0]);
                    const uint8x16_t y8_8_F = vcombine_u8(yuyv.val[2], yuyv.val[2]);
                    const uint8x16_t y8_0_F = vzip1q_u8(y8_0_7, y8_8_F);
                    vst1q_u8(&d[0][i], y8_0_F);
                    continue;
                }

                if (FORMAT == RS2_FORMAT_Y16)
                {
                    const uint8x16_t y8_0_7 = vcombine_u8(yuyv.val[0], yuyv.val[0]);
                    const uint8x16_t y8_8_F = vcombine_u8(yuyv.val[2], yuyv.val[2]);
                    const uint8x16_t y8_0_F = vzip1q_u8(y8_0_7, y8_8_F);
                    // y16 (little endian)
                    uint8x16x2_t y16;
                    y16.val[0] = vdupq_n_u8(0);
                    y16.val[1] = y8_0_F;
                    vst2q_u8(&d[0][i * 2], y16);
                    continue;
                }

                uint8x16_t r8, g8, b8;
                {
                    int16x8x2_t y16;
                    {
                        const uint8x16_t y8_0_F = vzip1q_u8(
                            vcombine_u8(yuyv.val[0], yuyv.val[0]),
                            vcombine_u8(yuyv.val[2], yuyv.val[2]));

                        y16.val[0] = (int16x8_t)vmovl_u8(vget_low_u8(y8_0_F));
                        y16.val[1] = (int16x8_t)vmovl_high_u8(y8_0_F);
                    }

                    int16x8x2_t u16;
                    {
                        const uint8x16_t tmp = vcombine_u8(yuyv.val[1], yuyv.val[1]);
                        const uint8x16_t u8_0_F = vzip1q_u8(tmp, tmp);
                        u16.val[0] = (int16x8_t)vmovl_u8(vget_low_u8(u8_0_F));
                        u16.val[1] = (int16x8_t)vmovl_high_u8(u8_0_F);
                    }

                    int16x8x2_t v16;
                    {
                        const uint8x16_t tmp = vcombine_u8(yuyv.val[3], yuyv.val[3]);
                        const uint8x16_t v8_0_F = vzip1q_u8(tmp, tmp);
                        v16.val[0] = (int16x8_t)vmovl_u8(vget_low_u8(v8_0_F));
                        v16.val[1] = (int16x8_t)vmovl_high_u8(v8_0_F);
                    }

                    const auto n0   = vdupq_n_s16(0);
                    const auto n16  = vdupq_n_s16(16);
                    const auto n128 = vdupq_n_s16(128);

                    // YUV to RGB
                    int16x8x2_t r16, g16, b16;
                    for (size_t j = 0; j < 2; ++j)
                    {
                        // ((x << 4) * (y << 4)) >> 16 == (x * y) >> 8
                        //                             == (x * (y - 256)) >> 8 + x
                        // R = (298 * (Y - 16)) >> 8 + (409 * (V - 128)) >> 8
                        //   = + (42 * (Y - 16)) >> 8 + (Y - 16)
                        //     + (153 * (V - 128)) >> 8 + (V - 128)
                        // G = (298 * (Y - 16)) >> 8 - (100 * (U - 128)) >> 8 - (208 * (V - 128)) >> 8
                        //   = + (42 * (Y - 16)) >> 8 + (Y - 16)
                        //     - (100 * (U - 128)) >> 8
                        //     - (208 * (V - 128)) >> 8
                        // B = (298 * (Y - 16)) >> 8 + (516 * (U - 128)) >> 8
                        //   = + (42 * (Y - 16)) >> 8 + (Y - 16)
                        //     + (260 * (U - 128)) >> 8 + (U - 128)

                        auto tmp0 = vsubq_s16(y16.val[j], n16);
                        tmp0 = vaddq_s16(
                                vshrq_n_s16(vmulq_s16(tmp0, vdupq_n_s16(298 - (1 << 8))), 8),
                                tmp0
                        );
                        const auto tmp1 = vsubq_s16(u16.val[j], n128);
                        const auto tmp2 = vsubq_s16(v16.val[j], n128);

                        r16.val[j] = vaddq_s16(
                            tmp0,
                            vaddq_s16(
                                vshrq_n_s16(vmulq_s16(tmp2, vdupq_n_s16(409 - (1 << 8))), 8),
                                tmp2
                            )
                        );
                        // clamp min value to 0
                        // vqmovn_u16 is clamp max value of uint8_t on overflow
                        // don't need to use vminq_s16(r16.val[j], vdupq_n_s16(255))
                        r16.val[j] = vmaxq_s16(n0, r16.val[j]);

                        g16.val[j] = vsubq_s16(
                            vsubq_s16(tmp0, vshrq_n_s16(vmulq_s16(tmp1, vdupq_n_s16(100)), 8)),
                            vshrq_n_s16(vmulq_s16(tmp2, vdupq_n_s16(208)), 8)
                        );
                        g16.val[j] = vmaxq_s16(n0, g16.val[j]);

                        b16.val[j] = vaddq_s16(
                            tmp0,
                            vaddq_s16(
                                vshrq_n_s16(vmulq_s16(tmp1, vdupq_n_s16(516 - (1 << 8))), 8),
                                tmp1
                            )
                        );
                        b16.val[j] = vmaxq_s16(n0, b16.val[j]);
                    }

                    // int16 -> uint8 and combine x4x4 to x16
                    r8 = vcombine_u8(
                        vqmovn_u16((uint16x8_t)r16.val[0]),
                        vqmovn_u16((uint16x8_t)r16.val[1])
                    );
                    g8 = vcombine_u8(
                        vqmovn_u16((uint16x8_t)g16.val[0]),
                        vqmovn_u16((uint16x8_t)g16.val[1])
                    );
                    b8 = vcombine_u8(
                        vqmovn_u16((uint16x8_t)b16.val[0]),
                        vqmovn_u16((uint16x8_t)b16.val[1])
                    );
                }

                if (FORMAT == RS2_FORMAT_RGBA8)
                {
                    uint8x16x4_t rgba;
                    rgba.val[0] = r8;
                    rgba.val[1] = g8;
                    rgba.val[2] = b8;
                    rgba.val[3] = vdupq_n_u8(255);
                    vst4q_u8(&d[0][i * 4], rgba);
                    continue;
                }
                if (FORMAT == RS2_FORMAT_RGB8)
                {
                    uint8x16x3_t rgb;
                    rgb.val[0] = r8;
                    rgb.val[1] = g8;
                    rgb.val[2] = b8;
                    vst3q_u8(&d[0][i * 3], rgb);
                    continue;
                }
                if (FORMAT == RS2_FORMAT_BGRA8)
                {
                    uint8x16x4_t bgra;
                    bgra.val[0] = b8;
                    bgra.val[1] = g8;
                    bgra.val[2] = r8;
                    bgra.val[3] = vdupq_n_u8(255);
                    vst4q_u8(&d[0][i * 4], bgra);
                    continue;
                }
                if (FORMAT == RS2_FORMAT_BGR8)
                {
                    uint8x16x3_t bgr;
                    bgr.val[0] = b8;
                    bgr.val[1] = g8;
                    bgr.val[2] = r8;
                    vst3q_u8(&d[0][i * 3], bgr);
                    continue;
                }
            }
        }

        void unpack_yuy2_neon_y8(uint8_t * const d[], const uint8_t * s, int n)
        {
            unpack_yuy2_neon<RS2_FORMAT_Y8>(d, s, n);
        }
        void unpack_yuy2_neon_y16(uint8_t * const d[], const uint8_t * s, int n)
        {
            unpack_yuy2_neon<RS2_FORMAT_Y16>(d, s, n);
        }
        void unpack_yuy2_neon_rgb8(uint8_t * const d[], const uint8_t * s, int n)
        {
            unpack_yuy2_neon<RS2_FORMAT_RGB8>(d, s, n);
        }
        void unpack_yuy2_neon_rgba8(uint8_t * const d[], const uint8_t * s, int n)
        {
            unpack_yuy2_neon<RS2_FORMAT_RGBA8>(d, s, n);
        }
        void unpack_yuy2_neon_bgr8(uint8_t * const d[], const uint8_t * s, int n)
        {
            unpack_yuy2_neon<RS2_FORMAT_BGR8>(d, s, n);
        }
        void unpack_yuy2_neon_bgra8(uint8_t * const d[], const uint8_t * s, int n)
        {
            unpack_yuy2_neon<RS2_FORMAT_BGRA8>(d, s, n);
        }

        static inline uint8x8_t nv12_channel(int16x8_t c, int16x8_t d, int16x8_t e,
                                            int16_t u_coefficient, int16_t v_coefficient)
        {
            auto low = vmull_n_s16(vget_low_s16(c), 298);
            auto high = vmull_n_s16(vget_high_s16(c), 298);
            low = vmlal_n_s16(low, vget_low_s16(d), u_coefficient);
            high = vmlal_n_s16(high, vget_high_s16(d), u_coefficient);
            low = vmlal_n_s16(low, vget_low_s16(e), v_coefficient);
            high = vmlal_n_s16(high, vget_high_s16(e), v_coefficient);
            low = vaddq_s32(low, vdupq_n_s32(128));
            high = vaddq_s32(high, vdupq_n_s32(128));

            return vqmovun_s16(vcombine_s16(vshrn_n_s32(low, 8), vshrn_n_s32(high, 8)));
        }

        template<rs2_format FORMAT>
        static inline void unpack_nv12_neon_pixels(uint8_t * output, uint8x8_t y,
                                                    uint8x8_t u, uint8x8_t v)
        {
            const auto c = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(y)), vdupq_n_s16(16));
            const auto d = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(u)), vdupq_n_s16(128));
            const auto e = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(v)), vdupq_n_s16(128));
            const auto r = nv12_channel(c, d, e, 0, 409);
            const auto g = nv12_channel(c, d, e, -100, -208);
            const auto b = nv12_channel(c, d, e, 516, 0);

            if (FORMAT == RS2_FORMAT_RGB8 || FORMAT == RS2_FORMAT_BGR8)
            {
                uint8x8x3_t rgb;
                rgb.val[0] = FORMAT == RS2_FORMAT_RGB8 ? r : b;
                rgb.val[1] = g;
                rgb.val[2] = FORMAT == RS2_FORMAT_RGB8 ? b : r;
                vst3_u8(output, rgb);
            }
            else
            {
                uint8x8x4_t rgba;
                rgba.val[0] = FORMAT == RS2_FORMAT_RGBA8 ? r : b;
                rgba.val[1] = g;
                rgba.val[2] = FORMAT == RS2_FORMAT_RGBA8 ? b : r;
                rgba.val[3] = vdup_n_u8(255);
                vst4_u8(output, rgba);
            }
        }

        template<rs2_format FORMAT>
        static void unpack_nv12_neon_format(uint8_t * const d[], const uint8_t * s, int width, int height)
        {
            const int bpp = FORMAT == RS2_FORMAT_RGB8 || FORMAT == RS2_FORMAT_BGR8 ? 3 : 4;
            const auto y_plane = s;
            const auto uv_plane = s + width * height;
            for (int row = 0; row < height; ++row)
            {
                const auto y = y_plane + row * width;
                const auto uv = uv_plane + (row / 2) * width;
                auto output = d[0] + row * width * bpp;
                for (int column = 0; column < width; column += 16)
                {
                    const auto y_values = vld1q_u8(y + column);
                    const auto uv_values = vld2_u8(uv + column);
                    const auto u = vzip_u8(uv_values.val[0], uv_values.val[0]);
                    const auto v = vzip_u8(uv_values.val[1], uv_values.val[1]);

                    unpack_nv12_neon_pixels<FORMAT>(output + column * bpp,
                                                    vget_low_u8(y_values), u.val[0], v.val[0]);
                    unpack_nv12_neon_pixels<FORMAT>(output + (column + 8) * bpp,
                                                    vget_high_u8(y_values), u.val[1], v.val[1]);
                }
            }
        }

        void unpack_nv12_neon(rs2_format format, uint8_t * const d[], const uint8_t * s,
                              int width, int height)
        {
            switch (format)
            {
            case RS2_FORMAT_RGB8: unpack_nv12_neon_format<RS2_FORMAT_RGB8>(d, s, width, height); break;
            case RS2_FORMAT_RGBA8: unpack_nv12_neon_format<RS2_FORMAT_RGBA8>(d, s, width, height); break;
            case RS2_FORMAT_BGR8: unpack_nv12_neon_format<RS2_FORMAT_BGR8>(d, s, width, height); break;
            case RS2_FORMAT_BGRA8: unpack_nv12_neon_format<RS2_FORMAT_BGRA8>(d, s, width, height); break;
            default: assert(false); break;
            }
        }
    }
    #endif
#endif
