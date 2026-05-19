// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "image.h"

#pragma pack(push, 1) // All structs in this file are assumed to be byte-packed
namespace librealsense
{

    ////////////////////////////
    // Image size computation //
    ////////////////////////////

    size_t get_image_size(int width, int height, rs2_format format)
    {
        if (format == RS2_FORMAT_YUYV || (format == RS2_FORMAT_UYVY)) assert(width % 2 == 0);
        if (format == RS2_FORMAT_RAW10) assert(width % 4 == 0);
        return width * height * get_image_bpp(format) / 8;
    }

    int get_image_bpp(rs2_format format)
    {
        switch (format)
        {
        case RS2_FORMAT_Z16: return  16;
        case RS2_FORMAT_DISPARITY16: return 16;
        case RS2_FORMAT_DISPARITY32: return 32;
        case RS2_FORMAT_XYZ32F: return 12 * 8;
        case RS2_FORMAT_YUYV:  return 16;
        case RS2_FORMAT_M420:  return 12; // 16 pixels are represented with 24 bytes (16 of Y and 8 of Cr, Cb) - 24 / 16 * 8 = 12
        case RS2_FORMAT_NV12:  return 12; // Semi-planar YUV 4:2:0: Y plane (W*H) + interleaved UV plane (W*H/2) = 12 bpp
        case RS2_FORMAT_RGB8: return 24;
        case RS2_FORMAT_BGR8: return 24;
        case RS2_FORMAT_RGBA8: return 32;
        case RS2_FORMAT_BGRA8: return 32;
        case RS2_FORMAT_Y8: return 8;
        case RS2_FORMAT_Y16: return 16;
        case RS2_FORMAT_RAW10: return 16; // return 16 bits instead of 10 bits in order to prevent allocated frame buffer size < actual data size.
        case RS2_FORMAT_Y10BPACK: return 16;
        case RS2_FORMAT_RAW16: return 16;
        case RS2_FORMAT_RAW8: return 8;
        case RS2_FORMAT_UYVY: return 16;
        case RS2_FORMAT_GPIO_RAW: return 1;
        case RS2_FORMAT_MOTION_RAW: return 1;
        case RS2_FORMAT_MOTION_XYZ32F: return 1;
        case RS2_FORMAT_6DOF: return 1;
        case RS2_FORMAT_MJPEG: return 8;
        case RS2_FORMAT_Y8I: return 16;
        case RS2_FORMAT_Y12I: return 32;  // bpp for Y12i format was 24, but since D457 GMSL SerDes required 32, it had been increased to 32 for both
        case RS2_FORMAT_INZI: return 32;
        case RS2_FORMAT_INVI: return 16;
        case RS2_FORMAT_W10: return 32;
        case RS2_FORMAT_Z16H: return 16; // DEPRECATED!
        case RS2_FORMAT_FG: return 16;
        case RS2_FORMAT_Y411: return 12;
        case RS2_FORMAT_Y16I: return 32;
        default: assert(false); return 0;
        }
    }

}

#pragma pack(pop)
