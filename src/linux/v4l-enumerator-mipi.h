// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "camera-identifier-v4l.h"
#include "v4l-node-info.h"

#include <src/platform/mipi-device-info.h>

#include <functional>
#include <string>
#include <vector>

namespace librealsense
{
    namespace platform
    {
        // The MIPI/GMSL half of V4L2 enumeration: /sys/class/d4xx-class DFU nodes and /dev/video-rs-* links.
        namespace v4l_enum
        {
            std::vector<std::string> get_mipi_dfu_paths();

            bool get_devname_from_mipi_dfu_path( const path_and_identifier & dfu_path, std::string & dev_name );

            // Walk MIPI devices sitting in DFU/recovery mode.
            void foreach_mipi_device( std::function<void(const mipi_device_info&, const std::string&)> action );

            uvc_device_info get_info_from_mipi_device_path( const std::string & video_path, const std::string & name,
                                                            camera_identifier_v4l_mipi & mipi_id );

            // Nodes published by the rs-enum udev rules as /dev/video-rs-<sensor>[-md]-<idx>.
            std::vector<node_info> get_mipi_rs_enum_nodes();
        }  // namespace v4l_enum
    }  // namespace platform
}  // namespace librealsense
