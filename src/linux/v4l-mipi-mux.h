// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/platform/uvc-device.h>
#include <src/platform/uvc-device-info.h>

#include <memory>

namespace librealsense
{
    namespace platform
    {
        // GMSL MUX: streams time-share one node's capture in the RSVL format and are told apart by their metadata.
        // The node's device hands out a device per muxed stream through uvc_device::create_muxed_stream_device().
        namespace v4l_mipi_mux
        {
            // The device of a multiplexing node, or nullptr if `info` does not offer the carrier format
            std::shared_ptr< uvc_device > create_device( const uvc_device_info & info );
        }  // namespace v4l_mipi_mux
    }  // namespace platform
}  // namespace librealsense
