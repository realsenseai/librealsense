// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once
#ifndef LIBREALSENSE_IMAGE_AVX_H
#define LIBREALSENSE_IMAGE_AVX_H

#include <cstdint>

// AVX2 image kernels are built on x86-64 only; callers select them at runtime via rsutils::os::cpu_supports_avx2().
// They are compiled with the baseline flags plus a per-function AVX2 target, so nothing else emits AVX2.
#if defined( __SSSE3__ ) && ! defined( ANDROID ) && ( defined( __x86_64__ ) || defined( _M_X64 ) )
#define AVX2_SUPPORT
#ifdef _MSC_VER
#define AVX2_TARGET  // MSVC allows AVX2 intrinsics without /arch:AVX2
#else
#define AVX2_TARGET __attribute__( ( target( "avx2" ) ) )
#endif
#endif

namespace librealsense
{
#ifdef AVX2_SUPPORT
    // n must be a multiple of 32 pixels
    void unpack_yuy2_avx_rgb8(uint8_t * const d[], const uint8_t * s, int n);
    void unpack_yuy2_avx_rgba8(uint8_t * const d[], const uint8_t * s, int n);
    void unpack_yuy2_avx_bgr8(uint8_t * const d[], const uint8_t * s, int n);
    void unpack_yuy2_avx_bgra8(uint8_t * const d[], const uint8_t * s, int n);
#endif
}

#endif
