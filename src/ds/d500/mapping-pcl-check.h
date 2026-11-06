// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include "d585s-md.h"
#include <rsutils/number/crc32.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace librealsense
{
// Host validation of a D5xx EP12 device point-cloud (PCL XYZ) frame: the bare W x H float32 x,y,z payload
// (no in-band header, RSDEV-15012); identity and integrity in md_point_cloud, checked when their validity bit is set.

enum class pcl_frame_check
{
    ok,
    size_mismatch,          // payload size != W * H * stride
    abi_mismatch,           // wrong block type, block too short, or unknown layout version
    crc_mismatch,           // payload_crc32 differs from the received bytes
    vertex_count_mismatch,  // number_of_3d_vertices != W * H
    format_mismatch,        // point_format / point_stride differ from the committed profile
    profile_mismatch,       // profile_id differs from the committed profile
};

inline const char * to_string( pcl_frame_check c )
{
    switch( c )
    {
    case pcl_frame_check::ok: return "ok";
    case pcl_frame_check::size_mismatch: return "payload size mismatch";
    case pcl_frame_check::abi_mismatch: return "metadata ABI mismatch";
    case pcl_frame_check::crc_mismatch: return "payload CRC mismatch";
    case pcl_frame_check::vertex_count_mismatch: return "vertex count mismatch";
    case pcl_frame_check::format_mismatch: return "point format mismatch";
    case pcl_frame_check::profile_mismatch: return "profile id mismatch";
    }
    return "unknown";
}

struct pcl_expected
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint16_t profile_id = 0;
    uint8_t point_stride = 12;  // XYZ
    uint8_t point_format = 0;   // 0 = XYZ (bits 0-3); bits 4-7 = encoding, 0 = raw
};

// Per stream session; reset when the stream starts.
struct pcl_session
{
    bool has_generation = false;
    uint16_t generation = 0;
    unsigned restarts = 0;  // generation changes inside the session (FW restarted the stream)
    bool unknown_metadata_layout = false;  // metadata of another layout seen: ignored, size check only
};

static constexpr uint32_t pcl_metadata_layout = 0x0002;  // upper 16 bits of frame_attributes.version

inline bool pcl_has( uint32_t flags, md_point_cloud_attributes a )
{
    return ( flags & static_cast< uint32_t >( a ) ) != 0;
}

// FW restarts the Mapping stream on its own when the pipeline is rebuilt (for example, host Depth
// opening at another profile): the generation changes and the source counters restart while the
// host session goes on. Follow the new generation instead of rejecting the rest of the session.
inline void track_pcl_generation( uint16_t generation, pcl_session & session )
{
    if( session.has_generation && session.generation != generation )
        ++session.restarts;
    session.has_generation = true;
    session.generation = generation;
}

// `md` / `md_size`: the metadata block after the UVC header, or nullptr / 0 when the frame has
// none (for example, Windows without the metadata registry keys). Without metadata only the
// payload size can be checked.
inline pcl_frame_check check_pcl_frame( const uint8_t * payload,
                                        size_t payload_size,
                                        const uint8_t * md,
                                        size_t md_size,
                                        const pcl_expected & expected,
                                        pcl_session & session )
{
    const size_t vertex_count = size_t( expected.width ) * expected.height;
    if( payload_size != vertex_count * expected.point_stride )
        return pcl_frame_check::size_mismatch;

    if( ! md || md_size == 0 )
        return pcl_frame_check::ok;

    if( md_size < sizeof( md_point_cloud ) )
        return pcl_frame_check::abi_mismatch;

    md_point_cloud m;
    std::memcpy( &m, md, sizeof( m ) );
    if( m.header.md_type_id != md_type::META_DATA_INTEL_POINT_CLOUD_ID || m.header.md_size < sizeof( m ) )
        return pcl_frame_check::abi_mismatch;

    // A layout this SDK does not know cannot be read field by field: validate the size only
    if( ( m.version >> 16 ) != pcl_metadata_layout )
    {
        session.unknown_metadata_layout = true;
        return pcl_frame_check::ok;
    }

    if( pcl_has( m.flags, md_point_cloud_attributes::payload_crc32_attribute )
        && rsutils::number::calc_crc32( payload, payload_size ) != m.payload_crc32 )
        return pcl_frame_check::crc_mismatch;

    if( pcl_has( m.flags, md_point_cloud_attributes::number_of_3d_vertices_attribute )
        && m.number_of_3d_vertices != vertex_count )
        return pcl_frame_check::vertex_count_mismatch;

    if( ( pcl_has( m.flags, md_point_cloud_attributes::point_format_attribute )
          && m.point_format != expected.point_format )
        || ( pcl_has( m.flags, md_point_cloud_attributes::point_stride_attribute )
             && m.point_stride != expected.point_stride ) )
        return pcl_frame_check::format_mismatch;

    if( pcl_has( m.flags, md_point_cloud_attributes::profile_id_attribute ) && m.profile_id != expected.profile_id )
        return pcl_frame_check::profile_mismatch;

    if( pcl_has( m.flags, md_point_cloud_attributes::stream_generation_attribute ) )
        track_pcl_generation( m.stream_generation, session );

    return pcl_frame_check::ok;
}

}  // namespace librealsense
