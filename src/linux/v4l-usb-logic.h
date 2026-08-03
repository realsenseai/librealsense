// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/usb/usb-types.h>  // usb_spec

#include <cstdint>
#include <string>

namespace librealsense
{
    namespace platform
    {
        // Low-level V4L2 USB primitives: sysfs queries and USB node path conventions.
        namespace v4l_usb_logic
        {
            bool is_usb_device_path( const std::string & video_path );

            // Locate the node's busnum/devnum/devpath in sysfs. Returns false if not found.
            bool is_usb_path_valid( const std::string & usb_video_path, const std::string & dev_name,
                                    std::string & busnum, std::string & devnum, std::string & devpath );

            // Raw modalias string of a video4linux node (e.g. "video0"). Throws if unreadable.
            std::string read_modalias( const std::string & name );

            // bInterfaceNumber of a video4linux node. Throws if unreadable.
            uint16_t read_interface_number( const std::string & name );

            // Find USB connection type (USB2/3) for UVC device. Note - input parameter is passed by value.
            usb_spec get_usb_connection_type( std::string path );
        }  // namespace v4l_usb_logic
    }  // namespace platform
}  // namespace librealsense
