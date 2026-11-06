// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <cstdint>

namespace librealsense
{
    // Per-SKU behaviour the V4L2 backend has to special-case. Not a transport concern:
    // these predicates hold on USB and on MIPI alike.

    // D5xx product line. The D400 and D500 families share this backend and the d4xx kernel driver,
    // but not their depth-XU selector tables - see v4l_mipi_logic::xu_to_cid().
    bool is_d5xx_product_line( uint16_t pid );

    // The UVC interface carrying the D5xx mapping streams (occupancy / labeled point cloud):
    // MI 13 on D585S, MI 11 on every other D5xx. Their payload is a self-sized MAP1 frame rather
    // than an image, which both the fourcc split and the frame-size validation have to account for.
    bool is_d5xx_mapping_interface( uint16_t pid, uint16_t mi );
}  // namespace librealsense
