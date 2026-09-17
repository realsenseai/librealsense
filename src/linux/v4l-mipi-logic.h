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
            // The families reuse selector numbers for different controls, so `is_d5xx` picks the right table.
            uint32_t xu_to_cid( const extension_unit & xu, uint8_t control, bool is_d5xx );

            // D500 DPP composite XU descriptor for the MIPI backend's payload translation and range
            // synthesis. The driver's compound U32 array carries only the active param slots (no
            // dpp_header, no reserved-slot padding); this struct captures the header fields the
            // backend synthesizes on read and the per-slot ranges V4L2 can't publish for compound
            // controls with heterogeneous slots. Values mirror the driver's per-slot arrays.

            // LibRS wire layout is dpp_header + 8 int32 slots = 38 bytes (see rs_dpp_header.h /
            // rs_hdrd_control.h). Defined here so backend-v4l2 does not need to include the public
            // rs_dpp_header.h just for the byte offset.
            static constexpr size_t dpp_header_bytes = 6;
            static constexpr size_t dpp_wire_size = 38;

            struct d500_dpp_info
            {
                uint16_t ctl_id;         // dpp_header.ctl_id on read
                uint8_t  param_count;    // active int32 slots this control uses (7/2/4)
                uint8_t  param_type;     // dpp_header.param_type on read
                const int32_t * min;     // param_count entries each; nullptr on unknown
                const int32_t * max;
                const int32_t * step;
                const int32_t * def;
            };

            // Non-null only for RS_CAMERA_CID_MINZ / _DECIMATION_FILTER_DPP / _TEMPORAL_FILTER_DPP.
            const d500_dpp_info * d500_dpp_info_for_cid( uint32_t cid );

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
