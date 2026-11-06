// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "image-avx.h"

#ifdef AVX2_SUPPORT
    // Keep this file free of C++ library code: it must not emit AVX2 instructions outside the functions below
    #include <librealsense2/h/rs_sensor.h>
    #include <cassert>
    #include <immintrin.h>

    #pragma pack(push, 1) // All structs in this file are assumed to be byte-packed
    namespace librealsense
    {
        template<rs2_format FORMAT> AVX2_TARGET void unpack_yuy2( uint8_t * const d[], const uint8_t * s, int n)
        {
            assert(n % 32 == 0); // the caller uses AVX2 only when n % 32 == 0

            auto src = reinterpret_cast<const __m256i *>(s);
            auto dst = reinterpret_cast<__m256i *>(d[0]);

            #pragma omp parallel for
            for (int i = 0; i < n / 32; i++)
            {
                const __m256i zero = _mm256_set1_epi8(0);
                const __m256i n100 = _mm256_set1_epi16(100 << 4);
                const __m256i n208 = _mm256_set1_epi16(208 << 4);
                const __m256i n298 = _mm256_set1_epi16(298 << 4);
                const __m256i n409 = _mm256_set1_epi16(409 << 4);
                const __m256i n516 = _mm256_set1_epi16(516 << 4);
                const __m256i evens_odds = _mm256_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
                    0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30);


                // Load 16 YUY2 pixels each into two 32-byte registers
                __m256i s0 = _mm256_loadu_si256(&src[i * 2]);
                __m256i s1 = _mm256_loadu_si256(&src[i * 2 + 1]);

                // Shuffle all Y components to the low order bytes of the register, and all U/V components to the high order bytes
                const __m256i evens_odd1s_odd3s = _mm256_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 1, 5, 9, 13, 3, 7, 11, 15,
                    0, 2, 4, 6, 8, 10, 12, 14, 1, 5, 9, 13, 3, 7, 11, 15); // to get yyyyyyyyuuuuvvvvyyyyyyyyuuuuvvvv
                __m256i yyyyyyyyuuuuvvvv0 = _mm256_shuffle_epi8(s0, evens_odd1s_odd3s);
                __m256i yyyyyyyyuuuuvvvv8 = _mm256_shuffle_epi8(s1, evens_odd1s_odd3s);

                // Retrieve all 32 Y components as 32-bit values (16 components per register))
                __m256i y16__0_7 = _mm256_unpacklo_epi8(yyyyyyyyuuuuvvvv0, zero);         // convert to 16 bit
                __m256i y16__8_F = _mm256_unpacklo_epi8(yyyyyyyyuuuuvvvv8, zero);         // convert to 16 bit

                // Retrieve all 16 U and V components as 32-bit values (16 components per register)
                __m256i uv = _mm256_unpackhi_epi32(yyyyyyyyuuuuvvvv0, yyyyyyyyuuuuvvvv8); // uuuuuuuuvvvvvvvvuuuuuuuuvvvvvvvv
                __m256i u = _mm256_unpacklo_epi8(uv, uv);                                 // u's duplicated: uu uu uu uu uu uu uu uu uu uu uu uu uu uu uu uu
                __m256i v = _mm256_unpackhi_epi8(uv, uv);                                 //  vv vv vv vv vv vv vv vv vv vv vv vv vv vv vv vv
                __m256i u16__0_7 = _mm256_unpacklo_epi8(u, zero);                         // convert to 16 bit
                __m256i u16__8_F = _mm256_unpackhi_epi8(u, zero);                         // convert to 16 bit
                __m256i v16__0_7 = _mm256_unpacklo_epi8(v, zero);                         // convert to 16 bit
                __m256i v16__8_F = _mm256_unpackhi_epi8(v, zero);                         // convert to 16 bit

                // Compute R, G, B values for first 16 pixels
                __m256i c16__0_7 = _mm256_slli_epi16(_mm256_subs_epi16(y16__0_7, _mm256_set1_epi16(16)), 4); // (y - 16) << 4
                __m256i d16__0_7 = _mm256_slli_epi16(_mm256_subs_epi16(u16__0_7, _mm256_set1_epi16(128)), 4); // (u - 128) << 4    perhaps could have done these u,v to d,e before the duplication
                __m256i e16__0_7 = _mm256_slli_epi16(_mm256_subs_epi16(v16__0_7, _mm256_set1_epi16(128)), 4); // (v - 128) << 4
                __m256i r16__0_7 = _mm256_min_epi16(_mm256_set1_epi16(255), _mm256_max_epi16(zero, ((_mm256_add_epi16(_mm256_mulhi_epi16(c16__0_7, n298), _mm256_mulhi_epi16(e16__0_7, n409))))));                                                 // (298 * c + 409 * e + 128) ; //
                __m256i g16__0_7 = _mm256_min_epi16(_mm256_set1_epi16(255), _mm256_max_epi16(zero, ((_mm256_sub_epi16(_mm256_sub_epi16(_mm256_mulhi_epi16(c16__0_7, n298), _mm256_mulhi_epi16(d16__0_7, n100)), _mm256_mulhi_epi16(e16__0_7, n208)))))); // (298 * c - 100 * d - 208 * e + 128)
                __m256i b16__0_7 = _mm256_min_epi16(_mm256_set1_epi16(255), _mm256_max_epi16(zero, ((_mm256_add_epi16(_mm256_mulhi_epi16(c16__0_7, n298), _mm256_mulhi_epi16(d16__0_7, n516))))));                                                 // clampbyte((298 * c + 516 * d + 128) >> 8);

                // Compute R, G, B values for second 8 pixels
                __m256i c16__8_F = _mm256_slli_epi16(_mm256_subs_epi16(y16__8_F, _mm256_set1_epi16(16)), 4); // (y - 16) << 4
                __m256i d16__8_F = _mm256_slli_epi16(_mm256_subs_epi16(u16__8_F, _mm256_set1_epi16(128)), 4); // (u - 128) << 4    perhaps could have done these u,v to d,e before the duplication
                __m256i e16__8_F = _mm256_slli_epi16(_mm256_subs_epi16(v16__8_F, _mm256_set1_epi16(128)), 4); // (v - 128) << 4
                __m256i r16__8_F = _mm256_min_epi16(_mm256_set1_epi16(255), _mm256_max_epi16(zero, ((_mm256_add_epi16(_mm256_mulhi_epi16(c16__8_F, n298), _mm256_mulhi_epi16(e16__8_F, n409))))));                                                 // (298 * c + 409 * e + 128) ; //
                __m256i g16__8_F = _mm256_min_epi16(_mm256_set1_epi16(255), _mm256_max_epi16(zero, ((_mm256_sub_epi16(_mm256_sub_epi16(_mm256_mulhi_epi16(c16__8_F, n298), _mm256_mulhi_epi16(d16__8_F, n100)), _mm256_mulhi_epi16(e16__8_F, n208)))))); // (298 * c - 100 * d - 208 * e + 128)
                __m256i b16__8_F = _mm256_min_epi16(_mm256_set1_epi16(255), _mm256_max_epi16(zero, ((_mm256_add_epi16(_mm256_mulhi_epi16(c16__8_F, n298), _mm256_mulhi_epi16(d16__8_F, n516))))));                                                 // clampbyte((298 * c + 516 * d + 128) >> 8);

                if (FORMAT == RS2_FORMAT_RGB8 || FORMAT == RS2_FORMAT_RGBA8)
                {
                    // Shuffle separate R, G, B values into four registers storing four pixels each in (R, G, B, A) order
                    __m256i rg8__0_7 = _mm256_unpacklo_epi8(_mm256_shuffle_epi8(r16__0_7, evens_odds), _mm256_shuffle_epi8(g16__0_7, evens_odds)); // hi to take the odds which are the upper bytes we care about
                    __m256i ba8__0_7 = _mm256_unpacklo_epi8(_mm256_shuffle_epi8(b16__0_7, evens_odds), _mm256_set1_epi8(-1));
                    __m256i rgba_0_3 = _mm256_unpacklo_epi16(rg8__0_7, ba8__0_7);
                    __m256i rgba_4_7 = _mm256_unpackhi_epi16(rg8__0_7, ba8__0_7);

                    __m128i ZW1 = _mm256_extracti128_si256(rgba_4_7, 0);
                    __m256i XYZW1 = _mm256_inserti128_si256(rgba_0_3, ZW1, 1);

                    __m128i UV1 = _mm256_extracti128_si256(rgba_0_3, 1);
                    __m256i UVST1 = _mm256_inserti128_si256(rgba_4_7, UV1, 0);

                    __m256i rg8__8_F = _mm256_unpacklo_epi8(_mm256_shuffle_epi8(r16__8_F, evens_odds), _mm256_shuffle_epi8(g16__8_F, evens_odds)); // hi to take the odds which are the upper bytes we care about
                    __m256i ba8__8_F = _mm256_unpacklo_epi8(_mm256_shuffle_epi8(b16__8_F, evens_odds), _mm256_set1_epi8(-1));
                    __m256i rgba_8_B = _mm256_unpacklo_epi16(rg8__8_F, ba8__8_F);
                    __m256i rgba_C_F = _mm256_unpackhi_epi16(rg8__8_F, ba8__8_F);

                    __m128i ZW2 = _mm256_extracti128_si256(rgba_C_F, 0);
                    __m256i XYZW2 = _mm256_inserti128_si256(rgba_8_B, ZW2, 1);

                    __m128i UV2 = _mm256_extracti128_si256(rgba_8_B, 1);
                    __m256i UVST2 = _mm256_inserti128_si256(rgba_C_F, UV2, 0);

                    if (FORMAT == RS2_FORMAT_RGBA8)
                    {
                        // Store 32 pixels (128 bytes) at once
                        _mm256_storeu_si256(&dst[i * 4], XYZW1);
                        _mm256_storeu_si256(&dst[i * 4 + 1], UVST1);
                        _mm256_storeu_si256(&dst[i * 4 + 2], XYZW2);
                        _mm256_storeu_si256(&dst[i * 4 + 3], UVST2);
                    }

                    if (FORMAT == RS2_FORMAT_RGB8)
                    {
                        __m128i rgba0 = _mm256_extracti128_si256(XYZW1, 0);
                        __m128i rgba1 = _mm256_extracti128_si256(XYZW1, 1);
                        __m128i rgba2 = _mm256_extracti128_si256(UVST1, 0);
                        __m128i rgba3 = _mm256_extracti128_si256(UVST1, 1);
                        __m128i rgba4 = _mm256_extracti128_si256(XYZW2, 0);
                        __m128i rgba5 = _mm256_extracti128_si256(XYZW2, 1);
                        __m128i rgba6 = _mm256_extracti128_si256(UVST2, 0);
                        __m128i rgba7 = _mm256_extracti128_si256(UVST2, 1);

                        // Shuffle rgb triples to the start and end of each register
                        __m128i rgb0 = _mm_shuffle_epi8(rgba0, _mm_setr_epi8(3, 7, 11, 15, 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14));
                        __m128i rgb1 = _mm_shuffle_epi8(rgba1, _mm_setr_epi8(0, 1, 2, 4, 3, 7, 11, 15, 5, 6, 8, 9, 10, 12, 13, 14));
                        __m128i rgb2 = _mm_shuffle_epi8(rgba2, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 3, 7, 11, 15, 10, 12, 13, 14));
                        __m128i rgb3 = _mm_shuffle_epi8(rgba3, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 3, 7, 11, 15));
                        __m128i rgb4 = _mm_shuffle_epi8(rgba4, _mm_setr_epi8(3, 7, 11, 15, 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14));
                        __m128i rgb5 = _mm_shuffle_epi8(rgba5, _mm_setr_epi8(0, 1, 2, 4, 3, 7, 11, 15, 5, 6, 8, 9, 10, 12, 13, 14));
                        __m128i rgb6 = _mm_shuffle_epi8(rgba6, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 3, 7, 11, 15, 10, 12, 13, 14));
                        __m128i rgb7 = _mm_shuffle_epi8(rgba7, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 3, 7, 11, 15));

                        __m128i a1 = _mm_alignr_epi8(rgb1, rgb0, 4);
                        __m128i a2 = _mm_alignr_epi8(rgb2, rgb1, 8);
                        __m128i a3 = _mm_alignr_epi8(rgb3, rgb2, 12);
                        __m128i a4 = _mm_alignr_epi8(rgb5, rgb4, 4);
                        __m128i a5 = _mm_alignr_epi8(rgb6, rgb5, 8);
                        __m128i a6 = _mm_alignr_epi8(rgb7, rgb6, 12);

                        __m256i a1_2 = _mm256_castsi128_si256(a1);
                        a1_2 = _mm256_inserti128_si256(a1_2, a2, 1);

                        __m256i a3_4 = _mm256_castsi128_si256(a3);
                        a3_4 = _mm256_inserti128_si256(a3_4, a4, 1);

                        __m256i a5_6 = _mm256_castsi128_si256(a5);
                        a5_6 = _mm256_inserti128_si256(a5_6, a6, 1);

                        // Align registers and store 32 pixels (96 bytes) at once
                        _mm256_storeu_si256(&dst[i * 3], a1_2);
                        _mm256_storeu_si256(&dst[i * 3 + 1], a3_4);
                        _mm256_storeu_si256(&dst[i * 3 + 2], a5_6);
                    }
                }

                if (FORMAT == RS2_FORMAT_BGR8 || FORMAT == RS2_FORMAT_BGRA8)
                {
                    // Shuffle separate R, G, B values into four registers storing four pixels each in (B, G, R, A) order
                    __m256i bg8__0_7 = _mm256_unpacklo_epi8(_mm256_shuffle_epi8(b16__0_7, evens_odds), _mm256_shuffle_epi8(g16__0_7, evens_odds)); // hi to take the odds which are the upper bytes we care about
                    __m256i ra8__0_7 = _mm256_unpacklo_epi8(_mm256_shuffle_epi8(r16__0_7, evens_odds), _mm256_set1_epi8(-1));
                    __m256i bgra_0_3 = _mm256_unpacklo_epi16(bg8__0_7, ra8__0_7);
                    __m256i bgra_4_7 = _mm256_unpackhi_epi16(bg8__0_7, ra8__0_7);

                    __m128i ZW1 = _mm256_extracti128_si256(bgra_4_7, 0);
                    __m256i XYZW1 = _mm256_inserti128_si256(bgra_0_3, ZW1, 1);

                    __m128i UV1 = _mm256_extracti128_si256(bgra_0_3, 1);
                    __m256i UVST1 = _mm256_inserti128_si256(bgra_4_7, UV1, 0);

                    __m256i bg8__8_F = _mm256_unpacklo_epi8(_mm256_shuffle_epi8(b16__8_F, evens_odds), _mm256_shuffle_epi8(g16__8_F, evens_odds)); // hi to take the odds which are the upper bytes we care about
                    __m256i ra8__8_F = _mm256_unpacklo_epi8(_mm256_shuffle_epi8(r16__8_F, evens_odds), _mm256_set1_epi8(-1));
                    __m256i bgra_8_B = _mm256_unpacklo_epi16(bg8__8_F, ra8__8_F);
                    __m256i bgra_C_F = _mm256_unpackhi_epi16(bg8__8_F, ra8__8_F);

                    __m128i ZW2 = _mm256_extracti128_si256(bgra_C_F, 0);
                    __m256i XYZW2 = _mm256_inserti128_si256(bgra_8_B, ZW2, 1);

                    __m128i UV2 = _mm256_extracti128_si256(bgra_8_B, 1);
                    __m256i UVST2 = _mm256_inserti128_si256(bgra_C_F, UV2, 0);

                    if (FORMAT == RS2_FORMAT_BGRA8)
                    {
                        // Store 32 pixels (128 bytes) at once
                        _mm256_storeu_si256(&dst[i * 4], XYZW1);
                        _mm256_storeu_si256(&dst[i * 4 + 1], UVST1);
                        _mm256_storeu_si256(&dst[i * 4 + 2], XYZW2);
                        _mm256_storeu_si256(&dst[i * 4 + 3], UVST2);
                    }

                    if (FORMAT == RS2_FORMAT_BGR8)
                    {
                        __m128i rgba0 = _mm256_extracti128_si256(XYZW1, 0);
                        __m128i rgba1 = _mm256_extracti128_si256(XYZW1, 1);
                        __m128i rgba2 = _mm256_extracti128_si256(UVST1, 0);
                        __m128i rgba3 = _mm256_extracti128_si256(UVST1, 1);
                        __m128i rgba4 = _mm256_extracti128_si256(XYZW2, 0);
                        __m128i rgba5 = _mm256_extracti128_si256(XYZW2, 1);
                        __m128i rgba6 = _mm256_extracti128_si256(UVST2, 0);
                        __m128i rgba7 = _mm256_extracti128_si256(UVST2, 1);

                        // Shuffle rgb triples to the start and end of each register
                        __m128i bgr0 = _mm_shuffle_epi8(rgba0, _mm_setr_epi8(3, 7, 11, 15, 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14));
                        __m128i bgr1 = _mm_shuffle_epi8(rgba1, _mm_setr_epi8(0, 1, 2, 4, 3, 7, 11, 15, 5, 6, 8, 9, 10, 12, 13, 14));
                        __m128i bgr2 = _mm_shuffle_epi8(rgba2, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 3, 7, 11, 15, 10, 12, 13, 14));
                        __m128i bgr3 = _mm_shuffle_epi8(rgba3, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 3, 7, 11, 15));
                        __m128i bgr4 = _mm_shuffle_epi8(rgba4, _mm_setr_epi8(3, 7, 11, 15, 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14));
                        __m128i bgr5 = _mm_shuffle_epi8(rgba5, _mm_setr_epi8(0, 1, 2, 4, 3, 7, 11, 15, 5, 6, 8, 9, 10, 12, 13, 14));
                        __m128i bgr6 = _mm_shuffle_epi8(rgba6, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 3, 7, 11, 15, 10, 12, 13, 14));
                        __m128i bgr7 = _mm_shuffle_epi8(rgba7, _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 3, 7, 11, 15));

                        __m128i a1 = _mm_alignr_epi8(bgr1, bgr0, 4);
                        __m128i a2 = _mm_alignr_epi8(bgr2, bgr1, 8);
                        __m128i a3 = _mm_alignr_epi8(bgr3, bgr2, 12);
                        __m128i a4 = _mm_alignr_epi8(bgr5, bgr4, 4);
                        __m128i a5 = _mm_alignr_epi8(bgr6, bgr5, 8);
                        __m128i a6 = _mm_alignr_epi8(bgr7, bgr6, 12);

                        __m256i a1_2 = _mm256_castsi128_si256(a1);
                        a1_2 = _mm256_inserti128_si256(a1_2, a2, 1);

                        __m256i a3_4 = _mm256_castsi128_si256(a3);
                        a3_4 = _mm256_inserti128_si256(a3_4, a4, 1);

                        __m256i a5_6 = _mm256_castsi128_si256(a5);
                        a5_6 = _mm256_inserti128_si256(a5_6, a6, 1);

                        // Align registers and store 32 pixels (96 bytes) at once
                        _mm256_storeu_si256(&dst[i * 3], a1_2);
                        _mm256_storeu_si256(&dst[i * 3 + 1], a3_4);
                        _mm256_storeu_si256(&dst[i * 3 + 2], a5_6);
                    }
                }
            }
        }

        AVX2_TARGET void unpack_yuy2_avx_rgb8( uint8_t * const d[], const uint8_t * s, int n)
        {
            unpack_yuy2<RS2_FORMAT_RGB8>(d, s, n);
        }
        AVX2_TARGET void unpack_yuy2_avx_rgba8( uint8_t * const d[], const uint8_t * s, int n)
        {
            unpack_yuy2<RS2_FORMAT_RGBA8>(d, s, n);
        }
        AVX2_TARGET void unpack_yuy2_avx_bgr8( uint8_t * const d[], const uint8_t * s, int n)
        {
            unpack_yuy2<RS2_FORMAT_BGR8>(d, s, n);
        }
        AVX2_TARGET void unpack_yuy2_avx_bgra8( uint8_t * const d[], const uint8_t * s, int n)
        {
            unpack_yuy2<RS2_FORMAT_BGRA8>(d, s, n);
        }
    }

    #pragma pack(pop)
#endif
