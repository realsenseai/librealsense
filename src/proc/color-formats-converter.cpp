// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#include "color-formats-converter.h"

#include "option.h"
#include "image-avx.h"
#include "image.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../third-party/stb_image.h"

#ifdef RS2_USE_CUDA
#include "cuda/cuda-conversion.cuh"
#include "rsutils/accelerators/gpu.h"
#endif
#ifdef __SSSE3__
#include <tmmintrin.h> // For SSSE3 intrinsics
#endif
#include "neon/image-neon.h"

#if defined (ANDROID) || (defined (__linux__) && !defined (__x86_64__)) || (defined (__APPLE__) && !defined (__x86_64__))

bool has_avx() { return false; }

#else

#ifdef _WIN32
#include <intrin.h>
#define cpuid(info, x)    __cpuidex(info, x, 0)
#else
#include <cpuid.h>
void cpuid(int info[4], int info_type) {
    __cpuid_count(info_type, 0, info[0], info[1], info[2], info[3]);
}
#endif

bool has_avx()
{
    int info[4];
    cpuid(info, 1);
    return (info[2] & ((int)1 << 28)) != 0;
}

#endif

// explanations for converting YUV values to RGB can be found in:
// https://en.wikipedia.org/wiki/YUV#Y%E2%80%B2UV444_to_RGB888_conversion

namespace librealsense 
{
    /////////////////////////////
    // YUY2 unpacking routines //
    /////////////////////////////
    // This templated function unpacks YUY2 into Y8/Y16/RGB8/RGBA8/BGR8/BGRA8, depending on the compile-time parameter FORMAT.
    // It is expected that all branching outside of the loop control variable will be removed due to constant-folding.
    template<rs2_format FORMAT> void unpack_yuy2( uint8_t * const d[], const uint8_t * s, int width, int height, int actual_size)
    {
        auto n = width * height;
        assert(n % 16 == 0); // All currently supported color resolutions are multiples of 16 pixels. Could easily extend support to other resolutions by copying final n<16 pixels into a zero-padded buffer and recursively calling self for final iteration.
#ifdef RS2_USE_CUDA
        if (rsutils::rs2_is_gpu_available())
        {
            rscuda::unpack_yuy2_cuda<FORMAT>(d, s, n);
            return;
        }
#endif
#if defined __SSSE3__ && ! defined ANDROID
        static bool do_avx = has_avx();
#ifdef __AVX2__

        if (do_avx)
        {
            if (FORMAT == RS2_FORMAT_Y8) unpack_yuy2_avx_y8(d, s, n);
            if (FORMAT == RS2_FORMAT_Y16) unpack_yuy2_avx_y16(d, s, n);
            if (FORMAT == RS2_FORMAT_RGB8) unpack_yuy2_avx_rgb8(d, s, n);
            if (FORMAT == RS2_FORMAT_RGBA8) unpack_yuy2_avx_rgba8(d, s, n);
            if (FORMAT == RS2_FORMAT_BGR8) unpack_yuy2_avx_bgr8(d, s, n);
            if (FORMAT == RS2_FORMAT_BGRA8) unpack_yuy2_avx_bgra8(d, s, n);
        }
        else
#endif
        {
            auto src = reinterpret_cast<const __m128i *>(s);
            auto dst = reinterpret_cast<__m128i *>(d[0]);

#pragma omp parallel for
            for (int i = 0; i < n / 16; i++)
            {
                const __m128i zero = _mm_set1_epi8(0);
                const __m128i n100 = _mm_set1_epi16(100 << 4);
                const __m128i n208 = _mm_set1_epi16(208 << 4);
                const __m128i n298 = _mm_set1_epi16(298 << 4);
                const __m128i n409 = _mm_set1_epi16(409 << 4);
                const __m128i n516 = _mm_set1_epi16(516 << 4);
                const __m128i evens_odds = _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15);

                // Load 8 YUY2 pixels each into two 16-byte registers
                __m128i s0 = _mm_loadu_si128(&src[i * 2]);
                __m128i s1 = _mm_loadu_si128(&src[i * 2 + 1]);

                if (FORMAT == RS2_FORMAT_Y8)
                {
                    const __m128i vmask = _mm_set1_epi16( 0x00ff );
                    s0 = _mm_and_si128( s0, vmask );  // mask unwanted bytes
                    s1 = _mm_and_si128( s1, vmask );
                    // Convert packed signed 16-bit integers from a and b to packed 8-bit integers using unsigned saturation
                    _mm_storeu_si128( &dst[i], _mm_packus_epi16( s0, s1 ) );
                    continue;
                }

                // Shuffle all Y components to the low order bytes of the register, and all U/V components to the high order bytes
                const __m128i evens_odd1s_odd3s = _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 1, 5, 9, 13, 3, 7, 11, 15); // to get yyyyyyyyuuuuvvvv
                __m128i yyyyyyyyuuuuvvvv0 = _mm_shuffle_epi8(s0, evens_odd1s_odd3s);
                __m128i yyyyyyyyuuuuvvvv8 = _mm_shuffle_epi8(s1, evens_odd1s_odd3s);

                // Retrieve all 16 Y components as 16-bit values (8 components per register))
                __m128i y16__0_7 = _mm_unpacklo_epi8(yyyyyyyyuuuuvvvv0, zero);         // convert to 16 bit
                __m128i y16__8_F = _mm_unpacklo_epi8(yyyyyyyyuuuuvvvv8, zero);         // convert to 16 bit

                if (FORMAT == RS2_FORMAT_Y16)
                {
                    // Output 16 pixels (32 bytes) at once
                    _mm_storeu_si128(&dst[i * 2], _mm_slli_epi16(y16__0_7, 8));
                    _mm_storeu_si128(&dst[i * 2 + 1], _mm_slli_epi16(y16__8_F, 8));
                    continue;
                }

                // Retrieve all 16 U and V components as 16-bit values (8 components per register)
                __m128i uv = _mm_unpackhi_epi32(yyyyyyyyuuuuvvvv0, yyyyyyyyuuuuvvvv8); // uuuuuuuuvvvvvvvv
                __m128i u = _mm_unpacklo_epi8(uv, uv);                                 //  uu uu uu uu uu uu uu uu  u's duplicated
                __m128i v = _mm_unpackhi_epi8(uv, uv);                                 //  vv vv vv vv vv vv vv vv
                __m128i u16__0_7 = _mm_unpacklo_epi8(u, zero);                         // convert to 16 bit
                __m128i u16__8_F = _mm_unpackhi_epi8(u, zero);                         // convert to 16 bit
                __m128i v16__0_7 = _mm_unpacklo_epi8(v, zero);                         // convert to 16 bit
                __m128i v16__8_F = _mm_unpackhi_epi8(v, zero);                         // convert to 16 bit

                                                                                       // Compute R, G, B values for first 8 pixels
                __m128i c16__0_7 = _mm_slli_epi16(_mm_subs_epi16(y16__0_7, _mm_set1_epi16(16)), 4);
                __m128i d16__0_7 = _mm_slli_epi16(_mm_subs_epi16(u16__0_7, _mm_set1_epi16(128)), 4); // perhaps could have done these u,v to d,e before the duplication
                __m128i e16__0_7 = _mm_slli_epi16(_mm_subs_epi16(v16__0_7, _mm_set1_epi16(128)), 4);
                __m128i r16__0_7 = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_add_epi16(_mm_mulhi_epi16(c16__0_7, n298), _mm_mulhi_epi16(e16__0_7, n409))))));                                                 // (298 * c + 409 * e + 128) ; //
                __m128i g16__0_7 = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_sub_epi16(_mm_sub_epi16(_mm_mulhi_epi16(c16__0_7, n298), _mm_mulhi_epi16(d16__0_7, n100)), _mm_mulhi_epi16(e16__0_7, n208)))))); // (298 * c - 100 * d - 208 * e + 128)
                __m128i b16__0_7 = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_add_epi16(_mm_mulhi_epi16(c16__0_7, n298), _mm_mulhi_epi16(d16__0_7, n516))))));                                                 // clampbyte((298 * c + 516 * d + 128) >> 8);

                                                                                                                                                                                                                                 // Compute R, G, B values for second 8 pixels
                __m128i c16__8_F = _mm_slli_epi16(_mm_subs_epi16(y16__8_F, _mm_set1_epi16(16)), 4);
                __m128i d16__8_F = _mm_slli_epi16(_mm_subs_epi16(u16__8_F, _mm_set1_epi16(128)), 4); // perhaps could have done these u,v to d,e before the duplication
                __m128i e16__8_F = _mm_slli_epi16(_mm_subs_epi16(v16__8_F, _mm_set1_epi16(128)), 4);
                __m128i r16__8_F = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_add_epi16(_mm_mulhi_epi16(c16__8_F, n298), _mm_mulhi_epi16(e16__8_F, n409))))));                                                 // (298 * c + 409 * e + 128) ; //
                __m128i g16__8_F = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_sub_epi16(_mm_sub_epi16(_mm_mulhi_epi16(c16__8_F, n298), _mm_mulhi_epi16(d16__8_F, n100)), _mm_mulhi_epi16(e16__8_F, n208)))))); // (298 * c - 100 * d - 208 * e + 128)
                __m128i b16__8_F = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_add_epi16(_mm_mulhi_epi16(c16__8_F, n298), _mm_mulhi_epi16(d16__8_F, n516))))));                                                 // clampbyte((298 * c + 516 * d + 128) >> 8);

                if (FORMAT == RS2_FORMAT_RGB8 || FORMAT == RS2_FORMAT_RGBA8)
                {
                    // Shuffle separate R, G, B values into four registers storing four pixels each in (R, G, B, A) order
                    __m128i rg8__0_7 = _mm_unpacklo_epi8(_mm_shuffle_epi8(r16__0_7, evens_odds), _mm_shuffle_epi8(g16__0_7, evens_odds)); // hi to take the odds which are the upper bytes we care about
                    __m128i ba8__0_7 = _mm_unpacklo_epi8(_mm_shuffle_epi8(b16__0_7, evens_odds), _mm_set1_epi8(-1));
                    __m128i rgba_0_3 = _mm_unpacklo_epi16(rg8__0_7, ba8__0_7);
                    __m128i rgba_4_7 = _mm_unpackhi_epi16(rg8__0_7, ba8__0_7);

                    __m128i rg8__8_F = _mm_unpacklo_epi8(_mm_shuffle_epi8(r16__8_F, evens_odds), _mm_shuffle_epi8(g16__8_F, evens_odds)); // hi to take the odds which are the upper bytes we care about
                    __m128i ba8__8_F = _mm_unpacklo_epi8(_mm_shuffle_epi8(b16__8_F, evens_odds), _mm_set1_epi8(-1));
                    __m128i rgba_8_B = _mm_unpacklo_epi16(rg8__8_F, ba8__8_F);
                    __m128i rgba_C_F = _mm_unpackhi_epi16(rg8__8_F, ba8__8_F);

                    if (FORMAT == RS2_FORMAT_RGBA8)
                    {
                        // Store 16 pixels (64 bytes) at once
                        _mm_storeu_si128(&dst[i * 4], rgba_0_3);
                        _mm_storeu_si128(&dst[i * 4 + 1], rgba_4_7);
                        _mm_storeu_si128(&dst[i * 4 + 2], rgba_8_B);
                        _mm_storeu_si128(&dst[i * 4 + 3], rgba_C_F);
                    }

                    if (FORMAT == RS2_FORMAT_RGB8)
                    {
                        // Shuffle rgb triples to the start and end of each register
                        __m128i rgb0 = _mm_shuffle_epi8(rgba_0_3, _mm_setr_epi8(3, 7, 11, 15, 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14));
                        __m128i rgb1 = _mm_shuffle_epi8(rgba_4_7, _mm_setr_epi8(0, 1, 2, 4, 3, 7, 11, 15, 5, 6, 8, 9, 10, 12, 13, 14));
                        __m128i rgb2 = _mm_shuffle_epi8(rgba_8_B, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 3, 7, 11, 15, 10, 12, 13, 14));
                        __m128i rgb3 = _mm_shuffle_epi8(rgba_C_F, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 3, 7, 11, 15));

                        // Align registers and store 16 pixels (48 bytes) at once
                        _mm_storeu_si128(&dst[i * 3], _mm_alignr_epi8(rgb1, rgb0, 4));
                        _mm_storeu_si128(&dst[i * 3 + 1], _mm_alignr_epi8(rgb2, rgb1, 8));
                        _mm_storeu_si128(&dst[i * 3 + 2], _mm_alignr_epi8(rgb3, rgb2, 12));
                    }
                }

                if (FORMAT == RS2_FORMAT_BGR8 || FORMAT == RS2_FORMAT_BGRA8)
                {
                    // Shuffle separate R, G, B values into four registers storing four pixels each in (B, G, R, A) order
                    __m128i bg8__0_7 = _mm_unpacklo_epi8(_mm_shuffle_epi8(b16__0_7, evens_odds), _mm_shuffle_epi8(g16__0_7, evens_odds)); // hi to take the odds which are the upper bytes we care about
                    __m128i ra8__0_7 = _mm_unpacklo_epi8(_mm_shuffle_epi8(r16__0_7, evens_odds), _mm_set1_epi8(-1));
                    __m128i bgra_0_3 = _mm_unpacklo_epi16(bg8__0_7, ra8__0_7);
                    __m128i bgra_4_7 = _mm_unpackhi_epi16(bg8__0_7, ra8__0_7);

                    __m128i bg8__8_F = _mm_unpacklo_epi8(_mm_shuffle_epi8(b16__8_F, evens_odds), _mm_shuffle_epi8(g16__8_F, evens_odds)); // hi to take the odds which are the upper bytes we care about
                    __m128i ra8__8_F = _mm_unpacklo_epi8(_mm_shuffle_epi8(r16__8_F, evens_odds), _mm_set1_epi8(-1));
                    __m128i bgra_8_B = _mm_unpacklo_epi16(bg8__8_F, ra8__8_F);
                    __m128i bgra_C_F = _mm_unpackhi_epi16(bg8__8_F, ra8__8_F);

                    if (FORMAT == RS2_FORMAT_BGRA8)
                    {
                        // Store 16 pixels (64 bytes) at once
                        _mm_storeu_si128(&dst[i * 4], bgra_0_3);
                        _mm_storeu_si128(&dst[i * 4 + 1], bgra_4_7);
                        _mm_storeu_si128(&dst[i * 4 + 2], bgra_8_B);
                        _mm_storeu_si128(&dst[i * 4 + 3], bgra_C_F);
                    }

                    if (FORMAT == RS2_FORMAT_BGR8)
                    {
                        // Shuffle rgb triples to the start and end of each register
                        __m128i bgr0 = _mm_shuffle_epi8(bgra_0_3, _mm_setr_epi8(3, 7, 11, 15, 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14));
                        __m128i bgr1 = _mm_shuffle_epi8(bgra_4_7, _mm_setr_epi8(0, 1, 2, 4, 3, 7, 11, 15, 5, 6, 8, 9, 10, 12, 13, 14));
                        __m128i bgr2 = _mm_shuffle_epi8(bgra_8_B, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 3, 7, 11, 15, 10, 12, 13, 14));
                        __m128i bgr3 = _mm_shuffle_epi8(bgra_C_F, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 3, 7, 11, 15));

                        // Align registers and store 16 pixels (48 bytes) at once
                        _mm_storeu_si128(&dst[i * 3], _mm_alignr_epi8(bgr1, bgr0, 4));
                        _mm_storeu_si128(&dst[i * 3 + 1], _mm_alignr_epi8(bgr2, bgr1, 8));
                        _mm_storeu_si128(&dst[i * 3 + 2], _mm_alignr_epi8(bgr3, bgr2, 12));
                    }
                }
            }
        }

#elif defined(__ARM_NEON)  && ! defined ANDROID

        if (FORMAT == RS2_FORMAT_Y8) unpack_yuy2_neon_y8(d, s, n);
        if (FORMAT == RS2_FORMAT_Y16) unpack_yuy2_neon_y16(d, s, n);
        if (FORMAT == RS2_FORMAT_RGB8) unpack_yuy2_neon_rgb8(d, s, n);
        if (FORMAT == RS2_FORMAT_RGBA8) unpack_yuy2_neon_rgba8(d, s, n);
        if (FORMAT == RS2_FORMAT_BGR8) unpack_yuy2_neon_bgr8(d, s, n);
        if (FORMAT == RS2_FORMAT_BGRA8) unpack_yuy2_neon_bgra8(d, s, n);

#else  // Generic code for when SSSE3 is not available.
        auto src = reinterpret_cast<const uint8_t *>(s);
        auto dst = reinterpret_cast<uint8_t *>(d[0]);
        for (; n; n -= 16, src += 32)
        {
            if (FORMAT == RS2_FORMAT_Y8)
            {
                uint8_t out[16] = {
                    src[0], src[2], src[4], src[6],
                    src[8], src[10], src[12], src[14],
                    src[16], src[18], src[20], src[22],
                    src[24], src[26], src[28], src[30],
                };
                std::memcpy( dst, out, sizeof out );
                dst += sizeof out;
                continue;
            }

            if (FORMAT == RS2_FORMAT_Y16)
            {
                // Y16 is little-endian.  We output Y << 8.
                uint8_t out[32] = {
                    0, src[0], 0, src[2], 0, src[4], 0, src[6],
                    0, src[8], 0, src[10], 0, src[12], 0, src[14],
                    0, src[16], 0, src[18], 0, src[20], 0, src[22],
                    0, src[24], 0, src[26], 0, src[28], 0, src[30],
                };
                std::memcpy(dst, out, sizeof out);
                dst += sizeof out;
                continue;
            }

            int16_t y[16] = {
                src[0], src[2], src[4], src[6],
                src[8], src[10], src[12], src[14],
                src[16], src[18], src[20], src[22],
                src[24], src[26], src[28], src[30],
            }, u[16] = {
                src[1], src[1], src[5], src[5],
                src[9], src[9], src[13], src[13],
                src[17], src[17], src[21], src[21],
                src[25], src[25], src[29], src[29],
            }, v[16] = {
                src[3], src[3], src[7], src[7],
                src[11], src[11], src[15], src[15],
                src[19], src[19], src[23], src[23],
                src[27], src[27], src[31], src[31],
            };

            uint8_t r[16], g[16], b[16];
            for (int i = 0; i < 16; i++)
            {
                int32_t c = y[i] - 16;
                int32_t d = u[i] - 128;
                int32_t e = v[i] - 128;

                int32_t t;
#define clamp(x)  ((t=(x)) > 255 ? 255 : t < 0 ? 0 : t)
                r[i] = clamp((298 * c + 409 * e + 128) >> 8);
                g[i] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
                b[i] = clamp((298 * c + 516 * d + 128) >> 8);
#undef clamp
            }

            if (FORMAT == RS2_FORMAT_RGB8)
            {
                uint8_t out[16 * 3] = {
                    r[0], g[0], b[0], r[1], g[1], b[1],
                    r[2], g[2], b[2], r[3], g[3], b[3],
                    r[4], g[4], b[4], r[5], g[5], b[5],
                    r[6], g[6], b[6], r[7], g[7], b[7],
                    r[8], g[8], b[8], r[9], g[9], b[9],
                    r[10], g[10], b[10], r[11], g[11], b[11],
                    r[12], g[12], b[12], r[13], g[13], b[13],
                    r[14], g[14], b[14], r[15], g[15], b[15],
                };
                std::memcpy( dst, out, sizeof out );
                dst += sizeof out;
                continue;
            }

            if (FORMAT == RS2_FORMAT_BGR8)
            {
                uint8_t out[16 * 3] = {
                    b[0], g[0], r[0], b[1], g[1], r[1],
                    b[2], g[2], r[2], b[3], g[3], r[3],
                    b[4], g[4], r[4], b[5], g[5], r[5],
                    b[6], g[6], r[6], b[7], g[7], r[7],
                    b[8], g[8], r[8], b[9], g[9], r[9],
                    b[10], g[10], r[10], b[11], g[11], r[11],
                    b[12], g[12], r[12], b[13], g[13], r[13],
                    b[14], g[14], r[14], b[15], g[15], r[15],
                };
                std::memcpy( dst, out, sizeof out );
                dst += sizeof out;
                continue;
            }

            if (FORMAT == RS2_FORMAT_RGBA8)
            {
                uint8_t out[16 * 4] = {
                    r[0], g[0], b[0], 255, r[1], g[1], b[1], 255,
                    r[2], g[2], b[2], 255, r[3], g[3], b[3], 255,
                    r[4], g[4], b[4], 255, r[5], g[5], b[5], 255,
                    r[6], g[6], b[6], 255, r[7], g[7], b[7], 255,
                    r[8], g[8], b[8], 255, r[9], g[9], b[9], 255,
                    r[10], g[10], b[10], 255, r[11], g[11], b[11], 255,
                    r[12], g[12], b[12], 255, r[13], g[13], b[13], 255,
                    r[14], g[14], b[14], 255, r[15], g[15], b[15], 255,
                };
                std::memcpy( dst, out, sizeof out );
                dst += sizeof out;
                continue;
            }

            if (FORMAT == RS2_FORMAT_BGRA8)
            {
                uint8_t out[16 * 4] = {
                    b[0], g[0], r[0], 255, b[1], g[1], r[1], 255,
                    b[2], g[2], r[2], 255, b[3], g[3], r[3], 255,
                    b[4], g[4], r[4], 255, b[5], g[5], r[5], 255,
                    b[6], g[6], r[6], 255, b[7], g[7], r[7], 255,
                    b[8], g[8], r[8], 255, b[9], g[9], r[9], 255,
                    b[10], g[10], r[10], 255, b[11], g[11], r[11], 255,
                    b[12], g[12], r[12], 255, b[13], g[13], r[13], 255,
                    b[14], g[14], r[14], 255, b[15], g[15], r[15], 255,
                };
                std::memcpy( dst, out, sizeof out );
                dst += sizeof out;
                continue;
            }
        }
#endif
    }

    template<rs2_format FORMAT>
    void m420_parse_one_line(const uint8_t * y_one_line, const uint8_t * uv_one_line, uint8_t** dst, int width)
    {
        // building 16 pixels at each iteration 
        for (int y_pix = 0, uv_pix = 0; y_pix < width; y_pix += 16, uv_pix += 16)
        {
            // grabbing matching y,u,v values
            uint8_t y[16] = { 0 };
            std::memcpy( y, &y_one_line[y_pix], 16 );

            uint8_t u[16] = {
                uv_one_line[uv_pix + 0], uv_one_line[uv_pix + 0], uv_one_line[uv_pix + 2], uv_one_line[uv_pix + 2],
                uv_one_line[uv_pix + 4], uv_one_line[uv_pix + 4], uv_one_line[uv_pix + 6], uv_one_line[uv_pix + 6],
                uv_one_line[uv_pix + 8], uv_one_line[uv_pix + 8], uv_one_line[uv_pix + 10], uv_one_line[uv_pix + 10],
                uv_one_line[uv_pix + 12], uv_one_line[uv_pix + 12], uv_one_line[uv_pix + 14], uv_one_line[uv_pix + 14]
            };

            uint8_t v[16] = {
                uv_one_line[uv_pix + 1], uv_one_line[uv_pix + 1], uv_one_line[uv_pix + 3], uv_one_line[uv_pix + 3],
                uv_one_line[uv_pix + 5], uv_one_line[uv_pix + 5], uv_one_line[uv_pix + 7], uv_one_line[uv_pix + 7],
                uv_one_line[uv_pix + 9], uv_one_line[uv_pix + 9], uv_one_line[uv_pix + 11], uv_one_line[uv_pix + 11],
                uv_one_line[uv_pix + 13], uv_one_line[uv_pix + 13], uv_one_line[uv_pix + 15], uv_one_line[uv_pix + 15]
            };

            // converting y,u,v values to r,g,b values
            uint8_t r[16], g[16], b[16];
            for (int i = 0; i < 16; i++)
            {
                int32_t c = y[i] - 16;
                int32_t d = u[i] - 128;
                int32_t e = v[i] - 128;

                int32_t t;
#define clamp(x)  ((t=(x)) > 255 ? 255 : t < 0 ? 0 : t)
                r[i] = clamp((298 * c + 409 * e + 128) >> 8);
                g[i] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
                b[i] = clamp((298 * c + 516 * d + 128) >> 8);
#undef clamp
            }

            // outputting r,g,b values in the order needed for each format
            if (FORMAT == RS2_FORMAT_RGB8)
            {
                uint8_t out[16 * 3] = {
                    r[0],  g[0],  b[0],  r[1],  g[1],  b[1],
                    r[2],  g[2],  b[2],  r[3],  g[3],  b[3],
                    r[4],  g[4],  b[4],  r[5],  g[5],  b[5],
                    r[6],  g[6],  b[6],  r[7],  g[7],  b[7],
                    r[8],  g[8],  b[8],  r[9],  g[9],  b[9],
                    r[10], g[10], b[10], r[11], g[11], b[11],
                    r[12], g[12], b[12], r[13], g[13], b[13],
                    r[14], g[14], b[14], r[15], g[15], b[15]
                };
                std::memcpy( *dst, out, sizeof( out ) );
                *dst += sizeof out;
                continue;
            }

            if (FORMAT == RS2_FORMAT_BGR8)
            {
                uint8_t out[16 * 3] = {
                    b[0], g[0], r[0], b[1], g[1], r[1],
                    b[2], g[2], r[2], b[3], g[3], r[3],
                    b[4], g[4], r[4], b[5], g[5], r[5],
                    b[6], g[6], r[6], b[7], g[7], r[7],
                    b[8], g[8], r[8], b[9], g[9], r[9],
                    b[10], g[10], r[10], b[11], g[11], r[11],
                    b[12], g[12], r[12], b[13], g[13], r[13],
                    b[14], g[14], r[14], b[15], g[15], r[15],
                };
                std::memcpy( *dst, out, sizeof out );
                *dst += sizeof out;
                continue;
            }

            if (FORMAT == RS2_FORMAT_RGBA8)
            {
                uint8_t out[16 * 4] = {
                    r[0], g[0], b[0], 255, r[1], g[1], b[1], 255,
                    r[2], g[2], b[2], 255, r[3], g[3], b[3], 255,
                    r[4], g[4], b[4], 255, r[5], g[5], b[5], 255,
                    r[6], g[6], b[6], 255, r[7], g[7], b[7], 255,
                    r[8], g[8], b[8], 255, r[9], g[9], b[9], 255,
                    r[10], g[10], b[10], 255, r[11], g[11], b[11], 255,
                    r[12], g[12], b[12], 255, r[13], g[13], b[13], 255,
                    r[14], g[14], b[14], 255, r[15], g[15], b[15], 255,
                };
                std::memcpy( *dst, out, sizeof out );
                *dst += sizeof out;
                continue;
            }

            if (FORMAT == RS2_FORMAT_BGRA8)
            {
                uint8_t out[16 * 4] = {
                    b[0], g[0], r[0], 255, b[1], g[1], r[1], 255,
                    b[2], g[2], r[2], 255, b[3], g[3], r[3], 255,
                    b[4], g[4], r[4], 255, b[5], g[5], r[5], 255,
                    b[6], g[6], r[6], 255, b[7], g[7], r[7], 255,
                    b[8], g[8], r[8], 255, b[9], g[9], r[9], 255,
                    b[10], g[10], r[10], 255, b[11], g[11], r[11], 255,
                    b[12], g[12], r[12], 255, b[13], g[13], r[13], 255,
                    b[14], g[14], r[14], 255, b[15], g[15], r[15], 255,
                };
                std::memcpy( *dst, out, sizeof out );
                *dst += sizeof out;
                continue;
            }
        }
    }

#if defined __SSSE3__ && ! defined ANDROID
    // This method receives 1 line of y and one line of uv.
    // source_chunks_y  // yyyyyyyyyyyyyyyy
    // source_chunks_uv // uvuvuvuvuvuvuvuv
    // Each coupling is done as: 2 bytes of y coupled with 2 bytes of uv (one u, and one v)
    template<rs2_format FORMAT> 
    void m420_sse_parse_one_line(const __m128i* source_chunks_y, const __m128i* source_chunks_uv, __m128i* dst, int line_length)
    {
#pragma omp parallel for
        for (int i = 0; i < line_length; ++i)
        {
            const __m128i zero = _mm_set1_epi8(0);
            __m128i y16__0_7 = _mm_unpacklo_epi8(source_chunks_y[i], zero);
            __m128i y16__8_F = _mm_unpackhi_epi8(source_chunks_y[i], zero);

            const __m128i evens_odds = _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15);  // to get uuuuuuuuvvvvvvvv

            __m128i uuuuuuuuvvvvvvvv = _mm_shuffle_epi8(source_chunks_uv[i], evens_odds);
            __m128i u = _mm_unpacklo_epi8(uuuuuuuuvvvvvvvv, uuuuuuuuvvvvvvvv); // uu duplicated
            __m128i v = _mm_unpackhi_epi8(uuuuuuuuvvvvvvvv, uuuuuuuuvvvvvvvv); // vv duplicated

            __m128i u16__0_7 = _mm_unpacklo_epi8(u, zero);                         // convert to 16 bit
            __m128i u16__8_F = _mm_unpackhi_epi8(u, zero);                         // convert to 16 bit
            __m128i v16__0_7 = _mm_unpacklo_epi8(v, zero);                         // convert to 16 bit
            __m128i v16__8_F = _mm_unpackhi_epi8(v, zero);                         // convert to 16 bit
            const __m128i n100 = _mm_set1_epi16(100 << 4);
            const __m128i n208 = _mm_set1_epi16(208 << 4);
            const __m128i n298 = _mm_set1_epi16(298 << 4);
            const __m128i n409 = _mm_set1_epi16(409 << 4);
            const __m128i n516 = _mm_set1_epi16(516 << 4);
            __m128i c16__0_7 = _mm_slli_epi16(_mm_subs_epi16(y16__0_7, _mm_set1_epi16(16)), 4);
            __m128i d16__0_7 = _mm_slli_epi16(_mm_subs_epi16(u16__0_7, _mm_set1_epi16(128)), 4); // perhaps could have done these u,v to d,e before the duplication
            __m128i e16__0_7 = _mm_slli_epi16(_mm_subs_epi16(v16__0_7, _mm_set1_epi16(128)), 4);
            __m128i r16__0_7 = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_add_epi16(_mm_mulhi_epi16(c16__0_7, n298), _mm_mulhi_epi16(e16__0_7, n409))))));                                                 // (298 * c + 409 * e + 128) ; //
            __m128i g16__0_7 = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_sub_epi16(_mm_sub_epi16(_mm_mulhi_epi16(c16__0_7, n298), _mm_mulhi_epi16(d16__0_7, n100)), _mm_mulhi_epi16(e16__0_7, n208)))))); // (298 * c - 100 * d - 208 * e + 128)
            __m128i b16__0_7 = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_add_epi16(_mm_mulhi_epi16(c16__0_7, n298), _mm_mulhi_epi16(d16__0_7, n516))))));                                                 // clampbyte((298 * c + 516 * d + 128) >> 8);

            __m128i c16__8_F = _mm_slli_epi16(_mm_subs_epi16(y16__8_F, _mm_set1_epi16(16)), 4);
            __m128i d16__8_F = _mm_slli_epi16(_mm_subs_epi16(u16__8_F, _mm_set1_epi16(128)), 4); // perhaps could have done these u,v to d,e before the duplication
            __m128i e16__8_F = _mm_slli_epi16(_mm_subs_epi16(v16__8_F, _mm_set1_epi16(128)), 4);
            __m128i r16__8_F = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_add_epi16(_mm_mulhi_epi16(c16__8_F, n298), _mm_mulhi_epi16(e16__8_F, n409))))));                                                 // (298 * c + 409 * e + 128) ; //
            __m128i g16__8_F = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_sub_epi16(_mm_sub_epi16(_mm_mulhi_epi16(c16__8_F, n298), _mm_mulhi_epi16(d16__8_F, n100)), _mm_mulhi_epi16(e16__8_F, n208)))))); // (298 * c - 100 * d - 208 * e + 128)
            __m128i b16__8_F = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_add_epi16(_mm_mulhi_epi16(c16__8_F, n298), _mm_mulhi_epi16(d16__8_F, n516))))));                                                 // clampbyte((298 * c + 516 * d + 128) >> 8);


            if (FORMAT == RS2_FORMAT_RGB8 || FORMAT == RS2_FORMAT_RGBA8)
            {
                // Shuffle separate R, G, B values into four registers storing four pixels each in (R, G, B, A) order
                __m128i rg8__0_7 = _mm_unpacklo_epi8(_mm_shuffle_epi8(r16__0_7, evens_odds), _mm_shuffle_epi8(g16__0_7, evens_odds)); // hi to take the odds which are the upper bytes we care about
                __m128i ba8__0_7 = _mm_unpacklo_epi8(_mm_shuffle_epi8(b16__0_7, evens_odds), _mm_set1_epi8(-1));
                __m128i rgba_0_3 = _mm_unpacklo_epi16(rg8__0_7, ba8__0_7);
                __m128i rgba_4_7 = _mm_unpackhi_epi16(rg8__0_7, ba8__0_7);

                __m128i rg8__8_F = _mm_unpacklo_epi8(_mm_shuffle_epi8(r16__8_F, evens_odds), _mm_shuffle_epi8(g16__8_F, evens_odds)); // hi to take the odds which are the upper bytes we care about
                __m128i ba8__8_F = _mm_unpacklo_epi8(_mm_shuffle_epi8(b16__8_F, evens_odds), _mm_set1_epi8(-1));
                __m128i rgba_8_B = _mm_unpacklo_epi16(rg8__8_F, ba8__8_F);
                __m128i rgba_C_F = _mm_unpackhi_epi16(rg8__8_F, ba8__8_F);

                if (FORMAT == RS2_FORMAT_RGBA8)
                {
                    // Store 16 pixels (64 bytes) at once
                    _mm_storeu_si128(&dst[i * 4], rgba_0_3);
                    _mm_storeu_si128(&dst[i * 4 + 1], rgba_4_7);
                    _mm_storeu_si128(&dst[i * 4 + 2], rgba_8_B);
                    _mm_storeu_si128(&dst[i * 4 + 3], rgba_C_F);

                    continue;
                }

                if (FORMAT == RS2_FORMAT_RGB8)
                {
                    // Shuffle rgb triples to the start and end of each register
                    __m128i rgb0 = _mm_shuffle_epi8(rgba_0_3, _mm_setr_epi8(3, 7, 11, 15, 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14));
                    __m128i rgb1 = _mm_shuffle_epi8(rgba_4_7, _mm_setr_epi8(0, 1, 2, 4, 3, 7, 11, 15, 5, 6, 8, 9, 10, 12, 13, 14));
                    __m128i rgb2 = _mm_shuffle_epi8(rgba_8_B, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 3, 7, 11, 15, 10, 12, 13, 14));
                    __m128i rgb3 = _mm_shuffle_epi8(rgba_C_F, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 3, 7, 11, 15));

                    // Align registers and store 16 pixels (48 bytes) at once
                    _mm_storeu_si128(&dst[i * 3], _mm_alignr_epi8(rgb1, rgb0, 4));
                    _mm_storeu_si128(&dst[i * 3 + 1], _mm_alignr_epi8(rgb2, rgb1, 8));
                    _mm_storeu_si128(&dst[i * 3 + 2], _mm_alignr_epi8(rgb3, rgb2, 12));

                    continue;
                }
            }

            if (FORMAT == RS2_FORMAT_BGR8 || FORMAT == RS2_FORMAT_BGRA8)
            {
                // Shuffle separate R, G, B values into four registers storing four pixels each in (B, G, R, A) order
                __m128i bg8__0_7 = _mm_unpacklo_epi8(_mm_shuffle_epi8(b16__0_7, evens_odds), _mm_shuffle_epi8(g16__0_7, evens_odds)); // hi to take the odds which are the upper bytes we care about
                __m128i ra8__0_7 = _mm_unpacklo_epi8(_mm_shuffle_epi8(r16__0_7, evens_odds), _mm_set1_epi8(-1));
                __m128i bgra_0_3 = _mm_unpacklo_epi16(bg8__0_7, ra8__0_7);
                __m128i bgra_4_7 = _mm_unpackhi_epi16(bg8__0_7, ra8__0_7);

                __m128i bg8__8_F = _mm_unpacklo_epi8(_mm_shuffle_epi8(b16__8_F, evens_odds), _mm_shuffle_epi8(g16__8_F, evens_odds)); // hi to take the odds which are the upper bytes we care about
                __m128i ra8__8_F = _mm_unpacklo_epi8(_mm_shuffle_epi8(r16__8_F, evens_odds), _mm_set1_epi8(-1));
                __m128i bgra_8_B = _mm_unpacklo_epi16(bg8__8_F, ra8__8_F);
                __m128i bgra_C_F = _mm_unpackhi_epi16(bg8__8_F, ra8__8_F);

                if (FORMAT == RS2_FORMAT_BGRA8)
                {
                    // Store 16 pixels (64 bytes) at once
                    _mm_storeu_si128(&dst[i * 4], bgra_0_3);
                    _mm_storeu_si128(&dst[i * 4 + 1], bgra_4_7);
                    _mm_storeu_si128(&dst[i * 4 + 2], bgra_8_B);
                    _mm_storeu_si128(&dst[i * 4 + 3], bgra_C_F);

                    continue;
                }

                if (FORMAT == RS2_FORMAT_BGR8)
                {
                    // Shuffle rgb triples to the start and end of each register
                    __m128i bgr0 = _mm_shuffle_epi8(bgra_0_3, _mm_setr_epi8(3, 7, 11, 15, 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14));
                    __m128i bgr1 = _mm_shuffle_epi8(bgra_4_7, _mm_setr_epi8(0, 1, 2, 4, 3, 7, 11, 15, 5, 6, 8, 9, 10, 12, 13, 14));
                    __m128i bgr2 = _mm_shuffle_epi8(bgra_8_B, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 3, 7, 11, 15, 10, 12, 13, 14));
                    __m128i bgr3 = _mm_shuffle_epi8(bgra_C_F, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 3, 7, 11, 15));

                    // Align registers and store 16 pixels (48 bytes) at once
                    _mm_storeu_si128(&dst[i * 3], _mm_alignr_epi8(bgr1, bgr0, 4));
                    _mm_storeu_si128(&dst[i * 3 + 1], _mm_alignr_epi8(bgr2, bgr1, 8));
                    _mm_storeu_si128(&dst[i * 3 + 2], _mm_alignr_epi8(bgr3, bgr2, 12));

                    continue;
                }
            }
        }
    }
#endif

    /////////////////////////////
    // M420 unpacking routines //
    /////////////////////////////
    // This templated function unpacks M420 into Y8/Y16/RGB8/RGBA8/BGR8/BGRA8, depending on the compile-time parameter FORMAT.
    // It is expected that all branching outside of the loop control variable will be removed due to constant-folding.
    // The M420 is a standard format - see: https://www.kernel.org/doc/html/v4.10/media/uapi/v4l/pixfmt-m420.html
    // Its configuration is as following: 2 lines of Y then one line of UV (line size is width)
    // There is one Y value for each pixel, and one pair of U,V values for 4 pixels. 
    // For example: for the first 3 lines of the frame:
    // Y0  Y1   Y2   Y3   .... Yw-1  (Yw:Ywidth)
    // Yw  Yw+1 Yw+2 Yw+3 .... Y2w-1
    // U0  V0   U1   V1
    // The first pixel is (Y0, U0, V0), second pixel is (Y1, U0, V0)
    // The first pixel in the second line is (Yw, U0, V0) second pixel in second line is (Yw+1, U0, V0)
    // The third pixel in second line is (Yw+2, U1, V1)
    template<rs2_format FORMAT> void unpack_m420( uint8_t * const d[], const uint8_t * s, int width, int height, int actual_size)
    {
        auto n = width * height;
        assert(n % 16 == 0); // All currently supported color resolutions are multiples of 16 pixels. Could easily extend support to other resolutions by copying final n<16 pixels into a zero-padded buffer and recursively calling self for final iteration.

#if defined __SSSE3__ && ! defined ANDROID
        static bool do_avx = has_avx();

        auto src = reinterpret_cast<const __m128i*>(s);
        auto dst = reinterpret_cast<__m128i*>(d[0]);

        __m128i* source_chunks_y = new __m128i[2 * width / 16];
        __m128i* source_chunks_uv = new __m128i[width / 16];

#pragma omp parallel for
        for (int j = 0; j < height / 2; ++j)
        {
#pragma omp parallel for
            for (int i = 0; i < 2 * width / 16; ++i)
            {
                auto offset_to_current_2_y_lines_for_src = (3 * width * j) / 16;
                source_chunks_y[i] = _mm_loadu_si128(&src[offset_to_current_2_y_lines_for_src + i]);

                if (FORMAT == RS2_FORMAT_Y8)
                {
                    auto offset_to_current_2_y_lines_for_dst = (2 * width * j) / 16;
                    // Align all Y components and output 2 lines of Y at once
                    _mm_storeu_si128(&dst[offset_to_current_2_y_lines_for_dst + i], source_chunks_y[i]);
                    continue;
                }

                if (FORMAT == RS2_FORMAT_Y16)
                {
                    auto bpp = 2;
                    auto offset_to_current_2_y_lines_for_dst = (2 * width * j) / 16 * bpp;
                    const __m128i zero = _mm_set1_epi8(0);
                    __m128i y16__0_7 = _mm_unpacklo_epi8(source_chunks_y[i], zero);
                    __m128i y16__8_F = _mm_unpackhi_epi8(source_chunks_y[i], zero);
                    __m128i y16_0_7_epi_16 = _mm_slli_epi16(y16__0_7, 8);
                    __m128i y16_8_F_epi_16 = _mm_slli_epi16(y16__8_F, 8);
                    // Align all Y components and output 2 _m128i of Y at once
                    _mm_storeu_si128(&dst[offset_to_current_2_y_lines_for_dst + i * 2], y16_0_7_epi_16);
                    _mm_storeu_si128(&dst[offset_to_current_2_y_lines_for_dst + i * 2 + 1], y16_8_F_epi_16);
                    continue;
                }

                auto offset_to_current_uv_line_for_src = offset_to_current_2_y_lines_for_src + 2 * width / 16;
                if ( i < width / 16)
                    source_chunks_uv[i] = _mm_load_si128(&src[offset_to_current_uv_line_for_src + i]);
            }

            if (FORMAT == RS2_FORMAT_RGB8 || FORMAT == RS2_FORMAT_RGBA8 || FORMAT == RS2_FORMAT_BGR8 || FORMAT == RS2_FORMAT_BGRA8)
            {
                int bpp = 3;
                if (FORMAT == RS2_FORMAT_RGBA8 || FORMAT == RS2_FORMAT_BGRA8)
                    bpp = 4;

                auto offset_to_current_first_line_for_dst = (2 * width * j) / 16 * bpp;
                auto offset_to_current_second_line_for_dst = offset_to_current_first_line_for_dst + width * bpp / 16;

                auto line_length = width / 16;
                auto first_line_y = source_chunks_y;
                auto second_line_y = source_chunks_y + line_length;

                m420_sse_parse_one_line<FORMAT>(first_line_y, source_chunks_uv, &dst[offset_to_current_first_line_for_dst], line_length);
                m420_sse_parse_one_line<FORMAT>(second_line_y, source_chunks_uv, &dst[offset_to_current_second_line_for_dst], line_length);
            }
        }

        delete[] source_chunks_y;
        delete[] source_chunks_uv;

#else
        auto src = reinterpret_cast<const uint8_t*>(s);
        auto dst = reinterpret_cast<uint8_t*>(d[0]);

        auto src_height = height * 12 >> 3;

        if (FORMAT == RS2_FORMAT_Y8)
        {
            for (int k = 0; k < src_height; k += 3)
            {
                // fill the destination with y values
                // while y is on 2 lines, and uv on the third line
                auto start_of_y = src + k * width;
                std::memcpy( dst, start_of_y, 2 * width );
                dst += 2 * width;
            }
            return;
        }
        if (FORMAT == RS2_FORMAT_Y16)
        {
            for (int k = 0; k < src_height; k += 3)
            {
                // fill the destination with y values
                // while y is on 2 lines, and uv on the third line
                auto start_of_y = src + k * width;

                for (int pix = 0; pix < 2 * width; pix += 16)
                {
                    uint16_t y[16];
                    for (int dst_idx = 0, src_idx = 0; dst_idx < 16; dst_idx += 1, ++src_idx)
                    {
                        y[dst_idx] = start_of_y[src_idx + pix] << 8;
                    }
                    std::memcpy( dst, y, sizeof y );
                    dst += sizeof y;
                }
            }
            return;
        }
        for (int k = 0; k < src_height; k += 3)
        {
            // fill the y_buffer and uv_buffer
            // while y is on 2 lines, and uv on the third line
            auto start_of_y = src + k * width;
            auto start_of_second_line = start_of_y + width;
            auto end_of_y = start_of_second_line + width;
            auto start_of_uv = end_of_y;
            auto end_of_uv = start_of_uv + width;

            m420_parse_one_line<FORMAT>(start_of_y, start_of_uv, &dst, width);
            m420_parse_one_line<FORMAT>(start_of_second_line, start_of_uv, &dst, width);
        }
        return;
#endif // __SSSE3__
    }

    void unpack_yuy2(rs2_format dst_format, rs2_stream dst_stream, uint8_t * const d[], const uint8_t * s, int w, int h, int actual_size)
    {
        switch (dst_format)
        {
        case RS2_FORMAT_RGB8:
            unpack_yuy2<RS2_FORMAT_RGB8>(d, s, w, h, actual_size);
            break;
        case RS2_FORMAT_Y8:
            unpack_yuy2<RS2_FORMAT_Y8>(d, s, w, h, actual_size);
            break;
        case RS2_FORMAT_RGBA8:
            unpack_yuy2<RS2_FORMAT_RGBA8>(d, s, w, h, actual_size);
            break;
        case RS2_FORMAT_BGR8:
            unpack_yuy2<RS2_FORMAT_BGR8>(d, s, w, h, actual_size);
            break;
        case RS2_FORMAT_BGRA8:
            unpack_yuy2<RS2_FORMAT_BGRA8>(d, s, w, h, actual_size);
            break;
        case RS2_FORMAT_Y16:
            unpack_yuy2<RS2_FORMAT_Y16>( d, s, w, h, actual_size );
            break;
        default:
            LOG_ERROR("Unsupported format for YUY2 conversion.");
            break;
        }
    }

    void unpack_m420(rs2_format dst_format, rs2_stream dst_stream, uint8_t * const d[], const uint8_t * s, int w, int h, int actual_size)
    {
        LOG_DEBUG("unpack m420 called with dst_format: " << rs2_format_to_string(dst_format));
        switch (dst_format)
        {
        case RS2_FORMAT_Y8:
            unpack_m420<RS2_FORMAT_Y8>(d, s, w, h, actual_size);
            break;
        case RS2_FORMAT_Y16:
            unpack_m420<RS2_FORMAT_Y16>(d, s, w, h, actual_size);
            break;
        case RS2_FORMAT_RGB8:
            unpack_m420<RS2_FORMAT_RGB8>(d, s, w, h, actual_size);
            break;
        case RS2_FORMAT_RGBA8:
            unpack_m420<RS2_FORMAT_RGBA8>(d, s, w, h, actual_size);
            break;
        case RS2_FORMAT_BGR8:
            unpack_m420<RS2_FORMAT_BGR8>(d, s, w, h, actual_size);
            break;
        case RS2_FORMAT_BGRA8:
            unpack_m420<RS2_FORMAT_BGRA8>(d, s, w, h, actual_size);
            break;
        default:
            LOG_ERROR("Unsupported format for M420 conversion.");
            break;
        }
    }

    /////////////////////////////
    // UYVY unpacking routines //
    /////////////////////////////
    // This templated function unpacks UYVY into RGB8/RGBA8/BGR8/BGRA8, depending on the compile-time parameter FORMAT.
    // It is expected that all branching outside of the loop control variable will be removed due to constant-folding.
    template<rs2_format FORMAT> void unpack_uyvy( uint8_t * const d[], const uint8_t * s, int width, int height, int actual_size)
    {
        auto n = width * height;
        assert(n % 16 == 0); // All currently supported color resolutions are multiples of 16 pixels. Could easily extend support to other resolutions by copying final n<16 pixels into a zero-padded buffer and recursively calling self for final iteration.
#ifdef __SSSE3__
        auto src = reinterpret_cast<const __m128i *>(s);
        auto dst = reinterpret_cast<__m128i *>(d[0]);
        for (; n; n -= 16)
        {
            const __m128i zero = _mm_set1_epi8(0);
            const __m128i n100 = _mm_set1_epi16(100 << 4);
            const __m128i n208 = _mm_set1_epi16(208 << 4);
            const __m128i n298 = _mm_set1_epi16(298 << 4);
            const __m128i n409 = _mm_set1_epi16(409 << 4);
            const __m128i n516 = _mm_set1_epi16(516 << 4);
            const __m128i evens_odds = _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15);

            // Load 8 UYVY pixels each into two 16-byte registers
            __m128i s0 = _mm_loadu_si128(src++);
            __m128i s1 = _mm_loadu_si128(src++);


            // Shuffle all Y components to the low order bytes of the register, and all U/V components to the high order bytes
            const __m128i evens_odd1s_odd3s = _mm_setr_epi8(1, 3, 5, 7, 9, 11, 13, 15, 0, 4, 8, 12, 2, 6, 10, 14); // to get yyyyyyyyuuuuvvvv
            __m128i yyyyyyyyuuuuvvvv0 = _mm_shuffle_epi8(s0, evens_odd1s_odd3s);
            __m128i yyyyyyyyuuuuvvvv8 = _mm_shuffle_epi8(s1, evens_odd1s_odd3s);

            // Retrieve all 16 Y components as 16-bit values (8 components per register))
            __m128i y16__0_7 = _mm_unpacklo_epi8(yyyyyyyyuuuuvvvv0, zero);         // convert to 16 bit
            __m128i y16__8_F = _mm_unpacklo_epi8(yyyyyyyyuuuuvvvv8, zero);         // convert to 16 bit


            // Retrieve all 16 U and V components as 16-bit values (8 components per register)
            __m128i uv = _mm_unpackhi_epi32(yyyyyyyyuuuuvvvv0, yyyyyyyyuuuuvvvv8); // uuuuuuuuvvvvvvvv
            __m128i u = _mm_unpacklo_epi8(uv, uv);                                 //  uu uu uu uu uu uu uu uu  u's duplicated
            __m128i v = _mm_unpackhi_epi8(uv, uv);                                 //  vv vv vv vv vv vv vv vv
            __m128i u16__0_7 = _mm_unpacklo_epi8(u, zero);                         // convert to 16 bit
            __m128i u16__8_F = _mm_unpackhi_epi8(u, zero);                         // convert to 16 bit
            __m128i v16__0_7 = _mm_unpacklo_epi8(v, zero);                         // convert to 16 bit
            __m128i v16__8_F = _mm_unpackhi_epi8(v, zero);                         // convert to 16 bit

                                                                                   // Compute R, G, B values for first 8 pixels
            __m128i c16__0_7 = _mm_slli_epi16(_mm_subs_epi16(y16__0_7, _mm_set1_epi16(16)), 4);
            __m128i d16__0_7 = _mm_slli_epi16(_mm_subs_epi16(u16__0_7, _mm_set1_epi16(128)), 4); // perhaps could have done these u,v to d,e before the duplication
            __m128i e16__0_7 = _mm_slli_epi16(_mm_subs_epi16(v16__0_7, _mm_set1_epi16(128)), 4);
            __m128i r16__0_7 = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_add_epi16(_mm_mulhi_epi16(c16__0_7, n298), _mm_mulhi_epi16(e16__0_7, n409))))));                                                 // (298 * c + 409 * e + 128) ; //
            __m128i g16__0_7 = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_sub_epi16(_mm_sub_epi16(_mm_mulhi_epi16(c16__0_7, n298), _mm_mulhi_epi16(d16__0_7, n100)), _mm_mulhi_epi16(e16__0_7, n208)))))); // (298 * c - 100 * d - 208 * e + 128)
            __m128i b16__0_7 = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_add_epi16(_mm_mulhi_epi16(c16__0_7, n298), _mm_mulhi_epi16(d16__0_7, n516))))));                                                 // clampbyte((298 * c + 516 * d + 128) >> 8);

                                                                                                                                                                                                                             // Compute R, G, B values for second 8 pixels
            __m128i c16__8_F = _mm_slli_epi16(_mm_subs_epi16(y16__8_F, _mm_set1_epi16(16)), 4);
            __m128i d16__8_F = _mm_slli_epi16(_mm_subs_epi16(u16__8_F, _mm_set1_epi16(128)), 4); // perhaps could have done these u,v to d,e before the duplication
            __m128i e16__8_F = _mm_slli_epi16(_mm_subs_epi16(v16__8_F, _mm_set1_epi16(128)), 4);
            __m128i r16__8_F = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_add_epi16(_mm_mulhi_epi16(c16__8_F, n298), _mm_mulhi_epi16(e16__8_F, n409))))));                                                 // (298 * c + 409 * e + 128) ; //
            __m128i g16__8_F = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_sub_epi16(_mm_sub_epi16(_mm_mulhi_epi16(c16__8_F, n298), _mm_mulhi_epi16(d16__8_F, n100)), _mm_mulhi_epi16(e16__8_F, n208)))))); // (298 * c - 100 * d - 208 * e + 128)
            __m128i b16__8_F = _mm_min_epi16(_mm_set1_epi16(255), _mm_max_epi16(zero, ((_mm_add_epi16(_mm_mulhi_epi16(c16__8_F, n298), _mm_mulhi_epi16(d16__8_F, n516))))));                                                 // clampbyte((298 * c + 516 * d + 128) >> 8);

            if (FORMAT == RS2_FORMAT_RGB8 || FORMAT == RS2_FORMAT_RGBA8)
            {
                // Shuffle separate R, G, B values into four registers storing four pixels each in (R, G, B, A) order
                __m128i rg8__0_7 = _mm_unpacklo_epi8(_mm_shuffle_epi8(r16__0_7, evens_odds), _mm_shuffle_epi8(g16__0_7, evens_odds)); // hi to take the odds which are the upper bytes we care about
                __m128i ba8__0_7 = _mm_unpacklo_epi8(_mm_shuffle_epi8(b16__0_7, evens_odds), _mm_set1_epi8(-1));
                __m128i rgba_0_3 = _mm_unpacklo_epi16(rg8__0_7, ba8__0_7);
                __m128i rgba_4_7 = _mm_unpackhi_epi16(rg8__0_7, ba8__0_7);

                __m128i rg8__8_F = _mm_unpacklo_epi8(_mm_shuffle_epi8(r16__8_F, evens_odds), _mm_shuffle_epi8(g16__8_F, evens_odds)); // hi to take the odds which are the upper bytes we care about
                __m128i ba8__8_F = _mm_unpacklo_epi8(_mm_shuffle_epi8(b16__8_F, evens_odds), _mm_set1_epi8(-1));
                __m128i rgba_8_B = _mm_unpacklo_epi16(rg8__8_F, ba8__8_F);
                __m128i rgba_C_F = _mm_unpackhi_epi16(rg8__8_F, ba8__8_F);

                if (FORMAT == RS2_FORMAT_RGBA8)
                {
                    // Store 16 pixels (64 bytes) at once
                    _mm_storeu_si128(dst++, rgba_0_3);
                    _mm_storeu_si128(dst++, rgba_4_7);
                    _mm_storeu_si128(dst++, rgba_8_B);
                    _mm_storeu_si128(dst++, rgba_C_F);
                }

                if (FORMAT == RS2_FORMAT_RGB8)
                {
                    // Shuffle rgb triples to the start and end of each register
                    __m128i rgb0 = _mm_shuffle_epi8(rgba_0_3, _mm_setr_epi8(3, 7, 11, 15, 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14));
                    __m128i rgb1 = _mm_shuffle_epi8(rgba_4_7, _mm_setr_epi8(0, 1, 2, 4, 3, 7, 11, 15, 5, 6, 8, 9, 10, 12, 13, 14));
                    __m128i rgb2 = _mm_shuffle_epi8(rgba_8_B, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 3, 7, 11, 15, 10, 12, 13, 14));
                    __m128i rgb3 = _mm_shuffle_epi8(rgba_C_F, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 3, 7, 11, 15));

                    // Align registers and store 16 pixels (48 bytes) at once
                    _mm_storeu_si128(dst++, _mm_alignr_epi8(rgb1, rgb0, 4));
                    _mm_storeu_si128(dst++, _mm_alignr_epi8(rgb2, rgb1, 8));
                    _mm_storeu_si128(dst++, _mm_alignr_epi8(rgb3, rgb2, 12));
                }
            }

            if (FORMAT == RS2_FORMAT_BGR8 || FORMAT == RS2_FORMAT_BGRA8)
            {
                // Shuffle separate R, G, B values into four registers storing four pixels each in (B, G, R, A) order
                __m128i bg8__0_7 = _mm_unpacklo_epi8(_mm_shuffle_epi8(b16__0_7, evens_odds), _mm_shuffle_epi8(g16__0_7, evens_odds)); // hi to take the odds which are the upper bytes we care about
                __m128i ra8__0_7 = _mm_unpacklo_epi8(_mm_shuffle_epi8(r16__0_7, evens_odds), _mm_set1_epi8(-1));
                __m128i bgra_0_3 = _mm_unpacklo_epi16(bg8__0_7, ra8__0_7);
                __m128i bgra_4_7 = _mm_unpackhi_epi16(bg8__0_7, ra8__0_7);

                __m128i bg8__8_F = _mm_unpacklo_epi8(_mm_shuffle_epi8(b16__8_F, evens_odds), _mm_shuffle_epi8(g16__8_F, evens_odds)); // hi to take the odds which are the upper bytes we care about
                __m128i ra8__8_F = _mm_unpacklo_epi8(_mm_shuffle_epi8(r16__8_F, evens_odds), _mm_set1_epi8(-1));
                __m128i bgra_8_B = _mm_unpacklo_epi16(bg8__8_F, ra8__8_F);
                __m128i bgra_C_F = _mm_unpackhi_epi16(bg8__8_F, ra8__8_F);

                if (FORMAT == RS2_FORMAT_BGRA8)
                {
                    // Store 16 pixels (64 bytes) at once
                    _mm_storeu_si128(dst++, bgra_0_3);
                    _mm_storeu_si128(dst++, bgra_4_7);
                    _mm_storeu_si128(dst++, bgra_8_B);
                    _mm_storeu_si128(dst++, bgra_C_F);
                }

                if (FORMAT == RS2_FORMAT_BGR8)
                {
                    // Shuffle rgb triples to the start and end of each register
                    __m128i bgr0 = _mm_shuffle_epi8(bgra_0_3, _mm_setr_epi8(3, 7, 11, 15, 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14));
                    __m128i bgr1 = _mm_shuffle_epi8(bgra_4_7, _mm_setr_epi8(0, 1, 2, 4, 3, 7, 11, 15, 5, 6, 8, 9, 10, 12, 13, 14));
                    __m128i bgr2 = _mm_shuffle_epi8(bgra_8_B, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 3, 7, 11, 15, 10, 12, 13, 14));
                    __m128i bgr3 = _mm_shuffle_epi8(bgra_C_F, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 3, 7, 11, 15));

                    // Align registers and store 16 pixels (48 bytes) at once
                    _mm_storeu_si128(dst++, _mm_alignr_epi8(bgr1, bgr0, 4));
                    _mm_storeu_si128(dst++, _mm_alignr_epi8(bgr2, bgr1, 8));
                    _mm_storeu_si128(dst++, _mm_alignr_epi8(bgr3, bgr2, 12));
                }
            }
        }
#else  // Generic code for when SSSE3 is not available.
        auto src = reinterpret_cast<const uint8_t *>(s);
        auto dst = reinterpret_cast<uint8_t *>(d[0]);
        for (; n; n -= 16, src += 32)
        {
            int16_t y[16] = {
                src[1], src[3], src[5], src[7],
                src[9], src[11], src[13], src[15],
                src[17], src[19], src[21], src[23],
                src[25], src[27], src[29], src[31],
            }, u[16] = {
                src[0], src[0], src[4], src[4],
                src[8], src[8], src[12], src[12],
                src[16], src[16], src[20], src[20],
                src[24], src[24], src[28], src[28],
            }, v[16] = {
                src[2], src[2], src[6], src[6],
                src[10], src[10], src[14], src[14],
                src[18], src[18], src[22], src[22],
                src[26], src[26], src[30], src[30],
            };

            uint8_t r[16], g[16], b[16];
            for (int i = 0; i < 16; i++)
            {
                int32_t c = y[i] - 16;
                int32_t d = u[i] - 128;
                int32_t e = v[i] - 128;

                int32_t t;
#define clamp(x)  ((t=(x)) > 255 ? 255 : t < 0 ? 0 : t)
                r[i] = clamp((298 * c + 409 * e + 128) >> 8);
                g[i] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
                b[i] = clamp((298 * c + 516 * d + 128) >> 8);
#undef clamp
            }

            if (FORMAT == RS2_FORMAT_RGB8)
            {
                uint8_t out[16 * 3] = {
                    r[0], g[0], b[0], r[1], g[1], b[1],
                    r[2], g[2], b[2], r[3], g[3], b[3],
                    r[4], g[4], b[4], r[5], g[5], b[5],
                    r[6], g[6], b[6], r[7], g[7], b[7],
                    r[8], g[8], b[8], r[9], g[9], b[9],
                    r[10], g[10], b[10], r[11], g[11], b[11],
                    r[12], g[12], b[12], r[13], g[13], b[13],
                    r[14], g[14], b[14], r[15], g[15], b[15],
                };
                std::memcpy( dst, out, sizeof out );
                dst += sizeof out;
                continue;
            }

            if (FORMAT == RS2_FORMAT_BGR8)
            {
                uint8_t out[16 * 3] = {
                    b[0], g[0], r[0], b[1], g[1], r[1],
                    b[2], g[2], r[2], b[3], g[3], r[3],
                    b[4], g[4], r[4], b[5], g[5], r[5],
                    b[6], g[6], r[6], b[7], g[7], r[7],
                    b[8], g[8], r[8], b[9], g[9], r[9],
                    b[10], g[10], r[10], b[11], g[11], r[11],
                    b[12], g[12], r[12], b[13], g[13], r[13],
                    b[14], g[14], r[14], b[15], g[15], r[15],
                };
                std::memcpy( dst, out, sizeof out );
                dst += sizeof out;
                continue;
            }

            if (FORMAT == RS2_FORMAT_RGBA8)
            {
                uint8_t out[16 * 4] = {
                    r[0], g[0], b[0], 255, r[1], g[1], b[1], 255,
                    r[2], g[2], b[2], 255, r[3], g[3], b[3], 255,
                    r[4], g[4], b[4], 255, r[5], g[5], b[5], 255,
                    r[6], g[6], b[6], 255, r[7], g[7], b[7], 255,
                    r[8], g[8], b[8], 255, r[9], g[9], b[9], 255,
                    r[10], g[10], b[10], 255, r[11], g[11], b[11], 255,
                    r[12], g[12], b[12], 255, r[13], g[13], b[13], 255,
                    r[14], g[14], b[14], 255, r[15], g[15], b[15], 255,
                };
                std::memcpy( dst, out, sizeof out );
                dst += sizeof out;
                continue;
            }

            if (FORMAT == RS2_FORMAT_BGRA8)
            {
                uint8_t out[16 * 4] = {
                    b[0], g[0], r[0], 255, b[1], g[1], r[1], 255,
                    b[2], g[2], r[2], 255, b[3], g[3], r[3], 255,
                    b[4], g[4], r[4], 255, b[5], g[5], r[5], 255,
                    b[6], g[6], r[6], 255, b[7], g[7], r[7], 255,
                    b[8], g[8], r[8], 255, b[9], g[9], r[9], 255,
                    b[10], g[10], r[10], 255, b[11], g[11], r[11], 255,
                    b[12], g[12], r[12], 255, b[13], g[13], r[13], 255,
                    b[14], g[14], r[14], 255, b[15], g[15], r[15], 255,
                };
                std::memcpy( dst, out, sizeof out );
                dst += sizeof out;
                continue;
            }
        }
#endif
    }

    void unpack_uyvyc(rs2_format dst_format, rs2_stream dst_stream, uint8_t * const d[], const uint8_t * s, int w, int h, int actual_size)
    {
        switch (dst_format)
        {
        case RS2_FORMAT_RGB8:
            unpack_uyvy<RS2_FORMAT_RGB8>(d, s, w, h, actual_size);
            break;
        case RS2_FORMAT_RGBA8:
            unpack_uyvy<RS2_FORMAT_RGBA8>(d, s, w, h, actual_size);
            break;
        case RS2_FORMAT_BGR8:
            unpack_uyvy<RS2_FORMAT_BGR8>(d, s, w, h, actual_size);
            break;
        case RS2_FORMAT_BGRA8:
            unpack_uyvy<RS2_FORMAT_BGRA8>(d, s, w, h, actual_size);
            break;
        default:
            LOG_ERROR("Unsupported format for UYVY conversion.");
            break;
        }
    }

    /////////////////////////////
    // MJPEG unpacking routines //
    /////////////////////////////
    void unpack_mjpeg( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size, int input_size)
    {
        int w, h, bpp;
        auto uncompressed_rgb = stbi_load_from_memory(source, actual_size, &w, &h, &bpp, false);
        if (uncompressed_rgb)
        {
            auto uncompressed_size = w * h * bpp;
            if (uncompressed_size != actual_size)
            {
                LOG_WARNING( "MJPEG decode size mismatch. Expected " << actual_size << ", got "
                                                                     << uncompressed_size );
                uncompressed_size = std::min( uncompressed_size, actual_size );
            }
            std::memcpy( dest[0], uncompressed_rgb, uncompressed_size );
            stbi_image_free(uncompressed_rgb);
        }
        else
            LOG_ERROR("mjpeg decode failed");
    }

    /////////////////////////////
    // BGR unpacking routines //
    /////////////////////////////
    void unpack_rgb_from_bgr( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size)
    {
        auto count = width * height;
        auto in = reinterpret_cast<const uint8_t *>(source);
        auto out = reinterpret_cast<uint8_t *>(dest[0]);

        std::memcpy( out, in, count * 3 );
        for (auto i = 0; i < count; i++)
        {
            std::swap(out[i * 3], out[i * 3 + 2]);
        }
    }

    void yuy2_converter::process_function( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size, int input_size)
    {
        unpack_yuy2(_target_format, _target_stream, dest, source, width, height, actual_size);
    }

    void uyvy_converter::process_function( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size, int input_size)
    {
        unpack_uyvyc(_target_format, _target_stream, dest, source, width, height, actual_size);
    }

    void mjpeg_converter::process_function( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size, int input_size)
    {
        unpack_mjpeg(dest, source, width, height, actual_size, input_size);
    }

    void bgr_to_rgb::process_function( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size, int input_size)
    {
        unpack_rgb_from_bgr(dest, source, width, height, actual_size);
    }

    void m420_converter::process_function( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size, int input_size)
    {
        unpack_m420(_target_format, _target_stream, dest, source, width, height, actual_size);
    }

    void uyvy_to_yuyv::process_function( uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size, int input_size)
    {
        auto in = reinterpret_cast< const uint16_t * >( source );
        auto out = reinterpret_cast< uint16_t * >( dest[0] );

        // Time measurments on Jetson yielded better results for the non-CUDA version
    //#ifdef RS2_USE_CUDA
        //if( rsutils::rs2_is_gpu_available() )
        //    rscuda::uyvy_to_yuyv_cuda_helper( in, out, width * height );
    //#else
        for( size_t i = 0; i < width * height; ++i )
            out[i] = ( ( in[i] >> 8 ) & 0x00FF ) | ( ( in[i] << 8 ) & 0xFF00 );
    //#endif
    }
}
