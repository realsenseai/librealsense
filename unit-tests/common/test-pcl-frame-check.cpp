// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:dependencies realsense2 rsutils

#include <unit-tests/catch.h>

#include <src/ds/d500/mapping-pcl-check.h>
#include <rsutils/number/crc32.h>

#include <cstring>
#include <vector>

using namespace librealsense;

namespace {

constexpr uint32_t W = 4;
constexpr uint32_t H = 3;

pcl_expected expected_xyz()
{
    pcl_expected e;
    e.width = W;
    e.height = H;
    e.profile_id = 0x0001;
    e.point_stride = 12;
    e.point_format = 0;
    return e;
}

std::vector< uint8_t > make_payload()
{
    std::vector< uint8_t > p( W * H * 12 );
    for( size_t i = 0; i < p.size(); ++i )
        p[i] = static_cast< uint8_t >( i * 7 + 3 );
    return p;
}

uint32_t bit( md_point_cloud_attributes a )
{
    return static_cast< uint32_t >( a );
}

// Metadata as current FW sends it (version 0x00020001, counters, timestamp, CRC), optionally
// extended with the 0x00020002 fields.
md_point_cloud make_md( std::vector< uint8_t > const & payload, bool extended )
{
    md_point_cloud m;
    std::memset( &m, 0, sizeof( m ) );
    m.header.md_type_id = md_type::META_DATA_INTEL_POINT_CLOUD_ID;
    m.header.md_size = sizeof( m );
    m.version = extended ? 0x00020002 : 0x00020001;
    m.flags = bit( md_point_cloud_attributes::frame_counter_attribute )
            | bit( md_point_cloud_attributes::depth_frame_counter_attribute )
            | bit( md_point_cloud_attributes::frame_timestamp_attribute )
            | bit( md_point_cloud_attributes::payload_crc32_attribute );
    m.payload_crc32 = rsutils::number::calc_crc32( payload.data(), payload.size() );
    if( extended )
    {
        m.flags |= bit( md_point_cloud_attributes::number_of_3d_vertices_attribute )
                 | bit( md_point_cloud_attributes::profile_id_attribute )
                 | bit( md_point_cloud_attributes::stream_generation_attribute )
                 | bit( md_point_cloud_attributes::point_stride_attribute )
                 | bit( md_point_cloud_attributes::point_format_attribute );
        m.number_of_3d_vertices = W * H;
        m.profile_id = 0x0001;
        m.stream_generation = 7;
        m.point_stride = 12;
        m.point_format = 0;
    }
    return m;
}

pcl_frame_check check( std::vector< uint8_t > const & payload, md_point_cloud const * m, pcl_session & s )
{
    return check_pcl_frame( payload.data(),
                            payload.size(),
                            reinterpret_cast< const uint8_t * >( m ),
                            m ? sizeof( *m ) : 0,
                            expected_xyz(),
                            s );
}

}  // namespace


TEST_CASE( "pcl frame check: payload size" )
{
    pcl_session s;
    auto payload = make_payload();
    CHECK( check( payload, nullptr, s ) == pcl_frame_check::ok );  // no metadata: size only

    payload.pop_back();
    CHECK( check( payload, nullptr, s ) == pcl_frame_check::size_mismatch );
}

TEST_CASE( "pcl frame check: current FW metadata" )
{
    pcl_session s;
    auto payload = make_payload();
    auto m = make_md( payload, false );
    CHECK( check( payload, &m, s ) == pcl_frame_check::ok );

    payload[5] ^= 0x01;
    CHECK( check( payload, &m, s ) == pcl_frame_check::crc_mismatch );

    m.flags &= ~bit( md_point_cloud_attributes::payload_crc32_attribute );  // no CRC bit: not checked
    CHECK( check( payload, &m, s ) == pcl_frame_check::ok );
}

TEST_CASE( "pcl frame check: metadata ABI" )
{
    pcl_session s;
    auto payload = make_payload();

    auto m = make_md( payload, false );
    m.header.md_type_id = md_type::META_DATA_INTEL_OCCUPANCY_ID;
    CHECK( check( payload, &m, s ) == pcl_frame_check::abi_mismatch );

    m = make_md( payload, false );
    m.version = 0x00030000;  // unknown layout: metadata ignored, size checked only
    m.payload_crc32 ^= 1;    // not read either
    CHECK( ! s.unknown_metadata_layout );
    CHECK( check( payload, &m, s ) == pcl_frame_check::ok );
    CHECK( s.unknown_metadata_layout );

    m = make_md( payload, false );
    m.version = 0x0002FFFF;  // any revision of layout 0x0002
    CHECK( check( payload, &m, s ) == pcl_frame_check::ok );

    m = make_md( payload, false );
    CHECK( check_pcl_frame( payload.data(), payload.size(), reinterpret_cast< const uint8_t * >( &m ), 100,
                            expected_xyz(), s )
           == pcl_frame_check::abi_mismatch );  // truncated block
}

TEST_CASE( "pcl frame check: extended fields" )
{
    pcl_session s;
    auto payload = make_payload();
    auto m = make_md( payload, true );
    CHECK( check( payload, &m, s ) == pcl_frame_check::ok );

    auto bad = m;
    bad.number_of_3d_vertices = W * H - 1;
    CHECK( check( payload, &bad, s ) == pcl_frame_check::vertex_count_mismatch );

    bad = m;
    bad.point_format = 1;
    CHECK( check( payload, &bad, s ) == pcl_frame_check::format_mismatch );

    bad = m;
    bad.point_stride = 16;
    CHECK( check( payload, &bad, s ) == pcl_frame_check::format_mismatch );

    bad = m;
    bad.profile_id = 0x0002;
    CHECK( check( payload, &bad, s ) == pcl_frame_check::profile_mismatch );

    bad = m;
    bad.profile_id = 0x0002;
    bad.flags &= ~bit( md_point_cloud_attributes::profile_id_attribute );  // bit clear: not checked
    CHECK( check( payload, &bad, s ) == pcl_frame_check::ok );
}

TEST_CASE( "pcl frame check: stream generation" )
{
    pcl_session s;
    auto payload = make_payload();
    auto m = make_md( payload, true );
    CHECK( check( payload, &m, s ) == pcl_frame_check::ok );
    CHECK( check( payload, &m, s ) == pcl_frame_check::ok );

    // FW restarted the stream inside the session (e.g. host Depth opened): adopt the new generation
    m.stream_generation = 8;
    CHECK( s.restarts == 0 );
    CHECK( check( payload, &m, s ) == pcl_frame_check::ok );
    CHECK( s.restarts == 1 );
    CHECK( s.generation == 8 );
    CHECK( check( payload, &m, s ) == pcl_frame_check::ok );
    CHECK( s.restarts == 1 );
}


// ---- MAP1 in-band form (FW before the pure-payload contract): 20-byte common header and a
// 16-byte point-cloud sub-header in front of the vertices; no UVC metadata. ----

namespace {

std::vector< uint8_t > make_map1_frame( uint16_t generation = 5 )
{
    auto vertices = make_payload();

    map1_point_cloud_header sub{};
    sub.width = W;
    sub.height = H;
    sub.point_stride = 12;
    sub.vertex_count = W * H;
    sub.source_frame_id = 42;

    std::vector< uint8_t > frame( sizeof( map1_frame_header ) + sizeof( sub ) + vertices.size() );
    std::memcpy( frame.data() + sizeof( map1_frame_header ), &sub, sizeof( sub ) );
    std::memcpy( frame.data() + sizeof( map1_frame_header ) + sizeof( sub ), vertices.data(), vertices.size() );

    map1_frame_header h{};
    h.magic = map1_magic;
    h.version = 0x0100;
    h.data_type = map1_type_point_cloud;
    h.flags = map1_flag_crc32;
    h.payload_size = static_cast< uint32_t >( frame.size() - sizeof( h ) );
    h.profile_id = 0x0001;
    h.stream_generation = generation;
    h.crc32 = rsutils::number::calc_crc32( frame.data() + sizeof( h ), h.payload_size );
    std::memcpy( frame.data(), &h, sizeof( h ) );
    return frame;
}

pcl_frame_check check_map1( std::vector< uint8_t > const & frame, pcl_session & s )
{
    return check_pcl_frame( frame.data(), frame.size(), nullptr, 0, expected_xyz(), s );
}

void restamp_crc( std::vector< uint8_t > & frame )
{
    map1_frame_header h;
    std::memcpy( &h, frame.data(), sizeof( h ) );
    h.crc32 = rsutils::number::calc_crc32( frame.data() + sizeof( h ), h.payload_size );
    std::memcpy( frame.data(), &h, sizeof( h ) );
}

}  // namespace


TEST_CASE( "pcl layout: MAP1 and pure payload" )
{
    auto frame = make_map1_frame();
    auto l = resolve_pcl_layout( frame.data(), frame.size() );
    CHECK( l.map1 );
    CHECK( l.vertex_offset == 36 );
    CHECK( l.vertex_count == W * H );

    auto pure = make_payload();
    l = resolve_pcl_layout( pure.data(), pure.size() );
    CHECK( ! l.map1 );
    CHECK( l.vertex_offset == 0 );
    CHECK( l.vertex_count == W * H );
}

TEST_CASE( "pcl frame check: MAP1 frame" )
{
    pcl_session s;
    auto frame = make_map1_frame();
    CHECK( check_map1( frame, s ) == pcl_frame_check::ok );

    auto bad = frame;
    bad.back() ^= 0x01;  // vertex byte
    CHECK( check_map1( bad, s ) == pcl_frame_check::crc_mismatch );

    bad = frame;
    bad.pop_back();  // truncated
    CHECK( check_map1( bad, s ) == pcl_frame_check::size_mismatch );

    bad = frame;
    bad[20] = W + 1;  // sub-header width
    restamp_crc( bad );
    CHECK( check_map1( bad, s ) == pcl_frame_check::vertex_count_mismatch );

    bad = frame;
    bad[24] = 16;  // sub-header point stride
    restamp_crc( bad );
    CHECK( check_map1( bad, s ) == pcl_frame_check::format_mismatch );

    bad = frame;
    bad[12] = 0x02;  // common header profile id
    CHECK( check_map1( bad, s ) == pcl_frame_check::profile_mismatch );

    bad = frame;
    bad[5] = 0x02;  // version 0x0200: unknown MAP1 layout
    CHECK( check_map1( bad, s ) == pcl_frame_check::abi_mismatch );
}

TEST_CASE( "pcl frame check: MAP1 stream generation" )
{
    pcl_session s;
    CHECK( check_map1( make_map1_frame( 5 ), s ) == pcl_frame_check::ok );
    CHECK( check_map1( make_map1_frame( 5 ), s ) == pcl_frame_check::ok );
    CHECK( check_map1( make_map1_frame( 6 ), s ) == pcl_frame_check::ok );  // FW restart: resync
    CHECK( s.restarts == 1 );
    CHECK( check_map1( make_map1_frame( 6 ), s ) == pcl_frame_check::ok );
    CHECK( s.restarts == 1 );
}

TEST_CASE( "pcl MAP1 source frame id" )
{
    uint32_t id = 0;
    auto frame = make_map1_frame();
    CHECK( map1_pcl_source_frame_id( frame.data(), frame.size(), id ) );
    CHECK( id == 42 );

    auto pure = make_payload();
    CHECK( ! map1_pcl_source_frame_id( pure.data(), pure.size(), id ) );
}
