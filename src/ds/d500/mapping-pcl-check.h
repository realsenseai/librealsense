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
// Host validation of a D5xx EP12 device point-cloud (PCL XYZ) frame. Two wire forms exist:
// - Pure payload (design contract): the bare W x H float32 x,y,z array; geometry, identity and
//   integrity travel in the point-cloud UVC metadata block (md_point_cloud). Checks on fields that
//   older FW does not send run only when the field's validity bit is set.
// - MAP1 (FW before the pure-payload contract): a 20-byte common header and a 16-byte point-cloud
//   sub-header in front of the vertices carry the same identity and integrity; no UVC metadata.

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

// MAP1 in-band headers (FW MappingEp12FrameHeader / MappingEp12PointCloudHeader)
static constexpr uint32_t map1_magic = 0x3150414DU;  // "MAP1"
static constexpr uint8_t map1_version_major = 0x01;  // version 0x01xx
static constexpr uint8_t map1_type_point_cloud = 0;
static constexpr uint8_t map1_flag_crc32 = 1;

#pragma pack( push, 1 )
struct map1_frame_header
{
    uint32_t magic;
    uint16_t version;
    uint8_t data_type;
    uint8_t flags;
    uint32_t payload_size;  // bytes after this header: sub-header + vertices
    uint16_t profile_id;
    uint16_t stream_generation;
    uint32_t crc32;  // over the payload_size bytes after this header
};
struct map1_point_cloud_header
{
    uint16_t width;
    uint16_t height;
    uint16_t point_stride;
    uint16_t reserved;
    uint32_t vertex_count;
    uint32_t source_frame_id;
};
#pragma pack( pop )
static_assert( sizeof( map1_frame_header ) == 20, "MAP1 frame header ABI" );
static_assert( sizeof( map1_point_cloud_header ) == 16, "MAP1 point-cloud header ABI" );

static constexpr size_t map1_pcl_vertex_offset = sizeof( map1_frame_header ) + sizeof( map1_point_cloud_header );

inline bool parse_map1_pcl( const uint8_t * data, size_t size, map1_frame_header & h, map1_point_cloud_header & sub )
{
    if( ! data || size < map1_pcl_vertex_offset )
        return false;
    std::memcpy( &h, data, sizeof( h ) );
    if( h.magic != map1_magic || h.data_type != map1_type_point_cloud )
        return false;
    std::memcpy( &sub, data + sizeof( h ), sizeof( sub ) );
    return true;
}

// MAP1 frames carry no UVC metadata; the sub-header's source Depth frame ID stands in for the
// frame counter.
inline bool map1_pcl_source_frame_id( const uint8_t * data, size_t size, uint32_t & id )
{
    map1_frame_header h;
    map1_point_cloud_header sub;
    if( ! parse_map1_pcl( data, size, h, sub ) )
        return false;
    id = sub.source_frame_id;
    return true;
}

// Where the vertices start in a received buffer, and how many there are.
struct pcl_layout
{
    size_t vertex_offset = 0;
    size_t vertex_count = 0;
    bool map1 = false;
};

inline pcl_layout resolve_pcl_layout( const uint8_t * data, size_t size )
{
    pcl_layout l;
    map1_frame_header h;
    map1_point_cloud_header sub;
    if( parse_map1_pcl( data, size, h, sub ) )
    {
        l.map1 = true;
        l.vertex_offset = map1_pcl_vertex_offset;
        l.vertex_count = sub.vertex_count;
        const size_t fits = ( size - map1_pcl_vertex_offset ) / ( 3 * sizeof( float ) );
        if( l.vertex_count > fits )
            l.vertex_count = fits;
        return l;
    }
    l.vertex_count = data ? size / ( 3 * sizeof( float ) ) : 0;
    return l;
}

inline bool pcl_has( uint32_t flags, md_point_cloud_attributes a )
{
    return ( flags & static_cast< uint32_t >( a ) ) != 0;
}

// FW restarts the Mapping stream on its own when the pipeline is rebuilt (for example, host Depth
// opening at another profile): the generation changes and the source counters restart while the
// host session goes on. Follow the new generation instead of rejecting the rest of the session.
inline pcl_frame_check check_pcl_generation( uint16_t generation, pcl_session & session )
{
    if( session.has_generation && session.generation != generation )
        ++session.restarts;
    session.has_generation = true;
    session.generation = generation;
    return pcl_frame_check::ok;
}

inline pcl_frame_check check_map1_pcl_frame( const uint8_t * data,
                                             size_t size,
                                             const map1_frame_header & h,
                                             const map1_point_cloud_header & sub,
                                             const pcl_expected & expected,
                                             pcl_session & session )
{
    if( ( h.version >> 8 ) != map1_version_major )
        return pcl_frame_check::abi_mismatch;

    const size_t vertex_count = size_t( expected.width ) * expected.height;
    if( h.payload_size != size - sizeof( h )
        || size != map1_pcl_vertex_offset + vertex_count * expected.point_stride )
        return pcl_frame_check::size_mismatch;

    if( ( h.flags & map1_flag_crc32 )
        && rsutils::number::calc_crc32( data + sizeof( h ), h.payload_size ) != h.crc32 )
        return pcl_frame_check::crc_mismatch;

    if( sub.point_stride != expected.point_stride )
        return pcl_frame_check::format_mismatch;

    if( sub.width != expected.width || sub.height != expected.height || sub.vertex_count != vertex_count )
        return pcl_frame_check::vertex_count_mismatch;

    if( h.profile_id != expected.profile_id )
        return pcl_frame_check::profile_mismatch;

    return check_pcl_generation( h.stream_generation, session );
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
    map1_frame_header h;
    map1_point_cloud_header sub;
    if( parse_map1_pcl( payload, payload_size, h, sub ) )
        return check_map1_pcl_frame( payload, payload_size, h, sub, expected, session );

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
        return check_pcl_generation( m.stream_generation, session );

    return pcl_frame_check::ok;
}

}  // namespace librealsense
