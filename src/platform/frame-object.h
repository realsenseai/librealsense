// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/h/rs_types.h>  // rs2_time_t

#include <cstddef>  // size_t
#include <cstdint>  // uint8_t


namespace librealsense {
namespace platform {


struct frame_object
{
    size_t frame_size;
    uint8_t metadata_size;
    const void * pixels;
    const void * metadata;
    rs2_time_t backend_time;
    // The backend continuation retains the sample and keeps the pixels locked until release.
    bool can_retain_pixels = false;
};


}  // namespace platform
}  // namespace librealsense
