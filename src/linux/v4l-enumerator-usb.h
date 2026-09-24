// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "v4l-node-info.h"

#include <string>
#include <vector>

namespace librealsense
{
    namespace platform
    {
        // The USB half of V4L2 enumeration: sysfs bus topology and USB descriptor walking.
        namespace v4l_enum
        {
            uvc_device_info get_info_from_usb_device_path( const std::string & video_path,
                                                           const std::string & dev_name,
                                                           const std::string & name );

            // Reorder the nodes of each UVC function into VideoStreaming interface order. MIPI nodes are skipped.
            void sort_nodes_by_streaming_interface( std::vector<node_info> & nodes );
        }  // namespace v4l_enum
    }  // namespace platform
}  // namespace librealsense
