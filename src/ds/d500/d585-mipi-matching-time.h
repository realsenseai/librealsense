// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/core/stream-profile-interface.h>
#include <src/frame.h>
#include <src/platform/uvc-device.h>
#include <cstring>

namespace librealsense {
namespace ds {

// Express the exposure time in the frame's SDK clock domain. Firmware may put a
// different time in the CSI header. Its counter and capture time wrap at 32 bits.
inline double d585_mipi_matching_time( frame_interface const & f, int depth_mode )
{
    auto const timestamp = f.get_frame_timestamp();
    auto const profile = f.get_stream();
    auto const type = profile->get_stream_type();
    if( ( type != RS2_STREAM_COLOR && type != RS2_STREAM_DEPTH && type != RS2_STREAM_INFRARED )
        || depth_mode == RS2_PASSIVE_DEPTH_MODE_COUNT
        || f.get_frame_timestamp_domain() == RS2_TIMESTAMP_DOMAIN_SYSTEM_TIME )
        return timestamp;

    auto const video = dynamic_cast< frame const * >( &f );
    rs2_metadata_type capture_us;
    if( ! video || video->additional_data.metadata_size < platform::uvc_header_mipi_size
        || ! f.find_metadata( RS2_FRAME_METADATA_FRAME_TIMESTAMP, &capture_us ) )
        return timestamp;

    platform::uvc_header_mipi header;
    std::memcpy( &header, video->additional_data.metadata_blob.data(), sizeof( header ) );
    uint32_t const delta = static_cast< uint32_t >( capture_us ) - header.header.timestamp;
    int64_t const delta_us = delta < 0x80000000u ? int64_t( delta ) : int64_t( delta ) - 0x100000000ll;
    // In active-only mode, both colors expose halfway between two depth exposures.
    auto const phase_ms = type == RS2_STREAM_COLOR && depth_mode == RS2_PASSIVE_DEPTH_MODE_DISABLED
                            ? 500. / profile->get_framerate() : 0.;
    return timestamp + delta_us / 1000. - phase_ms;
}

}  // namespace ds
}  // namespace librealsense
