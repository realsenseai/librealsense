// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#ifndef LIBREALSENSE_IMAGE_NEON_H
#define LIBREALSENSE_IMAGE_NEON_H

#include "types.h"

namespace librealsense
{
#ifndef ANDROID
    #if defined(__ARM_NEON) && defined(BUILD_WITH_NEON)
    void unpack_yuy2_neon_y8(uint8_t * const d[], const uint8_t * s, int n);
    void unpack_yuy2_neon_y16(uint8_t * const d[], const uint8_t * s, int n);
    void unpack_yuy2_neon_rgb8(uint8_t * const d[], const uint8_t * s, int n);
    void unpack_yuy2_neon_rgba8(uint8_t * const d[], const uint8_t * s, int n);
    void unpack_yuy2_neon_bgr8(uint8_t * const d[], const uint8_t * s, int n);
    void unpack_yuy2_neon_bgra8(uint8_t * const d[], const uint8_t * s, int n);
    #endif
#endif
}

#endif
