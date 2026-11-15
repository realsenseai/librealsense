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
