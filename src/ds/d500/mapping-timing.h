// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include "d585s-md.h"
#include <cstring>
#include <src/frame.h>

namespace librealsense
{
// Both Mapping products carry the same capture identity in their UVC metadata.
inline bool get_mapping_capture_timing( const frame &f, uint32_t &counter, uint64_t &timestamp )
{
    auto const &data = f.additional_data;
    constexpr size_t offset = platform::uvc_header_size;
    constexpr uint32_t capture_flags
        = static_cast< uint32_t >( md_occupancy_attributes::frame_counter_attribute )
        | static_cast< uint32_t >( md_occupancy_attributes::frame_timestamp_attribute );
    if ( data.metadata_size >= offset + sizeof( md_occupancy ) &&
         data.metadata_size <= data.metadata_blob.size() )
    {
        md_occupancy md;
        std::memcpy( &md, data.metadata_blob.data() + offset, sizeof( md ) );
        if ( ( md.header.md_type_id == md_type::META_DATA_INTEL_OCCUPANCY_ID ||
               md.header.md_type_id == md_type::META_DATA_INTEL_POINT_CLOUD_ID ) &&
             md.header.md_size >= sizeof( md ) && md.header.md_size <= data.metadata_size - offset &&
             ( md.flags & capture_flags ) == capture_flags && md.frame_timestamp != 0 )
        {
            counter = md.frame_counter;
            timestamp = md.frame_timestamp;
            return true;
        }
    }

    return false;
}
} // namespace librealsense
