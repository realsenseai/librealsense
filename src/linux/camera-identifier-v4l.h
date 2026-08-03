// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/platform/camera-identifier.h>

#include <cstdint>
#include <string>

namespace librealsense
{
    namespace platform
    {
        // Holds the USB-style VID/PID for a V4L2 device.
        class camera_identifier_v4l : public camera_identifier
        {
        public:
            uint16_t get_vid() const override { return _vid; }
            uint16_t get_pid() const override { return _pid; }

        protected:
            uint16_t _vid = 0;
            uint16_t _pid = 0;
        };

        // Identifier for a MIPI/GMSL camera.
        class camera_identifier_v4l_mipi : public camera_identifier_v4l
        {
        public:
            camera_identifier_v4l_mipi() { _vid = 0x8086; }  // Legacy Intel VID by default; Current models overrides to RealSense VID

            // Read the device's GVD and set PID/VID from it. dev_name must be the depth video node. Throws on failure.
            void resolve( const std::string & dev_name );

            // Clear a previously resolved PID/VID.
            void reset() { _pid = 0; _vid = 0x8086; }
        };

        // Identifier for a USB camera.
        class camera_identifier_v4l_usb : public camera_identifier_v4l
        {
        public:
            // Read the node's modalias and set VID/PID from it. name is the video4linux node (e.g. "video0"). Throws on failure.
            void resolve( const std::string & name );
        };
    }  // namespace platform
}  // namespace librealsense
