// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <cstdint>

namespace librealsense
{
    namespace platform
    {
        // Resolves a device's USB-style VID/PID.
        class camera_identifier
        {
        public:
            virtual ~camera_identifier() = default;

            virtual uint16_t get_vid() const = 0;
            virtual uint16_t get_pid() const = 0;
        };
    }  // namespace platform
}  // namespace librealsense
