// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/h/rs_option.h>  // rs2_option

#include <cstdint>
#include <string>
#include <vector>

namespace librealsense
{
    namespace platform
    {
        struct extension_unit;

        // Low-level V4L2 MIPI/GMSL primitives: raw device queries and node naming conventions.
        namespace v4l_mipi_logic
        {
            // Translate a USB like XU (subdevice, selector) to its V4L2 control id. Throws on an unmapped selector.
            uint32_t xu_to_cid( const extension_unit & xu, uint8_t control );

            // Translate an rs2_option (processing-unit control) to its V4L2 control id. Throws on an unmapped option.
            uint32_t option_to_cid( rs2_option option );

            // Whether an XU selector is the auto-exposure control, which the backend maps to/from V4L2 enum values.
            bool is_auto_exposure_control( uint8_t control );

            // Read the device's raw GVD buffer. Validates the response opcode, retrying as needed. Throws on failure.
            std::vector< uint8_t > get_gvd( const std::string & dev_name );

            bool is_device_depth_node( const std::string & dev_name );
            bool is_format_supported_on_node( const std::string & dev_name, std::string v4l_4cc_fmt );

            void get_device_info( const std::string & dev_name, std::string & bus_info, std::string & card );

            // Trailing index of a video node name (e.g. "video2" -> 2). Throws if the name has no index.
            int parse_video_index( const std::string & name );

            // Map a video node index to its camera id and interface indicator (video/metadata/IMU).
            // Throws on an index that does not fit the per-camera node layout.
            void derive_mi_and_cam_id( int video_index, uint16_t & mi, int & cam_id );

            // rs-enum link naming conventions
            std::string rs_enum_video_node_name( const std::string & sensor, int cam_idx, bool metadata );
            std::string rs_enum_dfu_node_path( int cam_idx );
        }  // namespace v4l_mipi_logic
    }  // namespace platform
}  // namespace librealsense
