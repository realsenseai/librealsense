// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/platform/uvc-device-info.h>

#include <string>
#include <utility>

namespace librealsense
{
    namespace platform
    {
        // V4L2 node enumeration: turning /sys and /dev entries into uvc_device_info.
        namespace v4l_enum
        {
            // A V4L2 enumeration node: the resolved device info plus its /dev/video* path.
            typedef std::pair< uvc_device_info, std::string > node_info;

            struct identifier
            {
                unsigned int major;
                unsigned int minor;

                bool operator== (const identifier& other) const { return major == other.major && minor == other.minor;}
            };

            struct path_and_identifier
            {
                std::string path;
                identifier key;
            };
        }  // namespace v4l_enum
    }  // namespace platform
}  // namespace librealsense
