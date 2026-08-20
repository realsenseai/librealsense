// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include "labeled-points.h"
#include "core/video.h"
#include "core/video-frame.h"
#include "core/frame-holder.h"
#include "librealsense-exception.h"
#include <rsutils/string/from.h>
#include <cstring>

namespace librealsense {

static constexpr size_t LOW_RES_WIDTH = 320;
static constexpr size_t LOW_RES_HEIGHT = 180;

static constexpr size_t HIGH_RES_WIDTH = 640;
static constexpr size_t HIGH_RES_HEIGHT = 360;

// bytes per pixel = 1 vertex + 1 label
static constexpr size_t BYTES_PER_PIXEL = 3 * sizeof(float) + sizeof(uint8_t);

static constexpr size_t LOW_RES_BUFFER_SIZE = LOW_RES_WIDTH * LOW_RES_HEIGHT * BYTES_PER_PIXEL;   // 320 x 180 x 13
static constexpr size_t HIGH_RES_BUFFER_SIZE = HIGH_RES_WIDTH * HIGH_RES_HEIGHT * BYTES_PER_PIXEL; // 640 x 360 x 13

// MAP1 carriage. A D500 Mapping frame is self-describing: a 20-byte common header, then a
// 24-byte LPCL sub-header holding width/height/strides/vertex-count, then the contiguous XYZ
// and label arrays. The legacy Safety stream has no headers and is identified by its exact
// buffer size instead. Both forms reach this class, so the geometry is resolved once, here,
// rather than being inferred from data.size() at four separate call sites.
static constexpr uint32_t MAP1_MAGIC = 0x3150414DU;  // "MAP1"
static constexpr size_t   MAP1_HEADER_LEN = 20;
static constexpr size_t   MAP1_LPCL_SUBHEADER_LEN = 24;
static constexpr uint8_t  MAP1_DATA_TYPE_LPCL = 1;

namespace {

struct lpcl_layout
{
    size_t payload_offset = 0;  // first XYZ byte
    size_t vertex_count = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    bool valid = false;
};

lpcl_layout resolve_layout( const uint8_t * bytes, size_t size )
{
    lpcl_layout out;

    if( bytes && size >= MAP1_HEADER_LEN + MAP1_LPCL_SUBHEADER_LEN )
    {
        uint32_t magic = 0;
        std::memcpy( &magic, bytes, sizeof( magic ) );
        if( magic == MAP1_MAGIC && bytes[6] == MAP1_DATA_TYPE_LPCL )
        {
            uint16_t w = 0, h = 0;
            uint32_t count = 0;
            const uint8_t * sub = bytes + MAP1_HEADER_LEN;
            std::memcpy( &w, sub + 0, sizeof( w ) );
            std::memcpy( &h, sub + 2, sizeof( h ) );
            std::memcpy( &count, sub + 8, sizeof( count ) );

            out.payload_offset = MAP1_HEADER_LEN + MAP1_LPCL_SUBHEADER_LEN;
            out.vertex_count = count;
            out.width = w;
            out.height = h;
            out.valid = ( count == static_cast< uint32_t >( w ) * h )
                     && ( size - out.payload_offset >= count * BYTES_PER_PIXEL );
            return out;
        }
    }

    // Legacy Safety stream: headerless, geometry carried only by the buffer size.
    switch( size )
    {
    case LOW_RES_BUFFER_SIZE:
        out.width = LOW_RES_WIDTH;
        out.height = LOW_RES_HEIGHT;
        break;
    case HIGH_RES_BUFFER_SIZE:
        out.width = HIGH_RES_WIDTH;
        out.height = HIGH_RES_HEIGHT;
        break;
    default:
        return out;  // invalid
    }
    out.payload_offset = 0;
    out.vertex_count = size / BYTES_PER_PIXEL;
    out.valid = true;
    return out;
}

lpcl_layout require_layout( const uint8_t * bytes, size_t size )
{
    auto out = resolve_layout( bytes, size );
    if( ! out.valid )
        throw wrong_api_call_sequence_exception( rsutils::string::from()
                                                << "unsupported buffer size of " << size
                                                << " received for labeled point cloud" );
    return out;
}

}  // namespace

size_t labeled_points::get_vertex_count() const
{
    return require_layout( data.data(), data.size() ).vertex_count;
}

float3* labeled_points::get_vertices()
{
    get_frame_data();  // call GetData to ensure data is in main memory
    return (float3 *)( data.data() + require_layout( data.data(), data.size() ).payload_offset );
}

const uint8_t* labeled_points::get_labels() const
{
    get_frame_data();  // call GetData to ensure data is in main memory
    const auto l = require_layout( data.data(), data.size() );
    return data.data() + l.payload_offset + 3 * sizeof(float) * l.vertex_count;
}

unsigned int labeled_points::get_width() const
{
    return require_layout( data.data(), data.size() ).width;
}

unsigned int labeled_points::get_height() const
{
    return require_layout( data.data(), data.size() ).height;
}

size_t labeled_points::get_bpp() const
{
    return BYTES_PER_PIXEL * 8;
}
} // namespace librealsense
