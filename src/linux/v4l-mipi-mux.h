// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/platform/uvc-device.h>
#include <src/platform/uvc-device-info.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace librealsense
{
    namespace platform
    {
        // GMSL Perception MUX: logical streams time-share the IR node in the RSVL format and are told apart by their
        // metadata. Each logical stream is presented as its own node, the way the USB device exposes it.
        namespace v4l_mipi_mux
        {
            // Append a device per logical stream of every multiplexing node, numbered with the stream's USB interface
            // so the device code finds it as it does on USB.
            void add_mux_devices( std::vector< std::pair< uvc_device_info, std::string > > & nodes );

            // A device sharing the multiplexing node's capture, or nullptr if `info` is not part of a MUX.
            std::shared_ptr< uvc_device > create_device( const uvc_device_info & info );
        }  // namespace v4l_mipi_mux
    }  // namespace platform
}  // namespace librealsense
