// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include <unit-tests/catch.h>
#include <src/ds/d500/d500-stream-group-transaction.h>
#include <src/librealsense-exception.h>


namespace
{
    uint32_t fourcc( char a, char b, char c, char d )
    {
        return ( static_cast< uint32_t >( static_cast< uint8_t >( a ) ) << 24 )
            | ( static_cast< uint32_t >( static_cast< uint8_t >( b ) ) << 16 )
            | ( static_cast< uint32_t >( static_cast< uint8_t >( c ) ) << 8 )
            | static_cast< uint32_t >( static_cast< uint8_t >( d ) );
    }
}


TEST_CASE( "D500 stream-group PREPARE uses the documented wire format", "[d500]" )
{
    using namespace librealsense;
    std::vector< d500_stream_group_profile > profiles = {
        { d500_stream_group_branch::depth, fourcc( 'Z', '1', '6', ' ' ), 640, 480, 30 },
        { d500_stream_group_branch::color_left, fourcc( 'N', 'V', '1', '2' ), 1280, 720, 30 },
        { d500_stream_group_branch::color_right, fourcc( 'M', '4', '2', '0' ), 1280, 720, 30 }
    };

    auto payload = d500_stream_group_transaction::encode_prepare( 0x12345678, profiles );
    std::vector< uint8_t > expected = {
        1, 3, 0x0d, 0, 0x78, 0x56, 0x34, 0x12,
        0, 0, 0x80, 0x02, 0xe0, 0x01, 30, 0, 0x20, 0x36, 0x31, 0x5a,
        2, 0, 0x00, 0x05, 0xd0, 0x02, 30, 0, 0x32, 0x31, 0x56, 0x4e,
        3, 0, 0x00, 0x05, 0xd0, 0x02, 30, 0, 0x30, 0x32, 0x34, 0x4d
    };
    CHECK( payload == expected );
}

TEST_CASE( "D500 stream-group PREPARE rejects ambiguous manifests", "[d500]" )
{
    using namespace librealsense;
    auto z16 = fourcc( 'Z', '1', '6', ' ' );

    CHECK_THROWS_AS( d500_stream_group_transaction::encode_prepare( 0, {
                         { d500_stream_group_branch::depth, z16, 640, 480, 30 } } ),
                     invalid_value_exception );
    CHECK_THROWS_AS( d500_stream_group_transaction::encode_prepare( 1, {
                         { d500_stream_group_branch::color_left, fourcc( 'N', 'V', '1', '2' ), 640, 480, 30 },
                         { d500_stream_group_branch::depth, z16, 640, 480, 30 } } ),
                     invalid_value_exception );
    CHECK_THROWS_AS( d500_stream_group_transaction::encode_prepare( 1, {
                         { d500_stream_group_branch::depth, fourcc( 'N', 'V', '1', '2' ), 640, 480, 30 } } ),
                     invalid_value_exception );
}

TEST_CASE( "D500 stream-group QUERY decodes status and rejects malformed masks", "[d500]" )
{
    using namespace librealsense;
    auto status = d500_stream_group_transaction::decode_status(
        { 0x78, 0x56, 0x34, 0x12, 0x0d, 0x05, 0x01, 2 } );
    CHECK( status.transaction_id == 0x12345678 );
    CHECK( status.expected_mask == 0x0d );
    CHECK( status.started_mask == 0x05 );
    CHECK( status.built_mask == 0x01 );
    CHECK( status.state == 2 );

    CHECK_THROWS_AS( d500_stream_group_transaction::decode_status( { 0, 0, 0, 0, 0, 0, 0 } ),
                     invalid_value_exception );
    CHECK_THROWS_AS( d500_stream_group_transaction::decode_status( { 0, 0, 0, 0, 0x10, 0, 0, 0 } ),
                     invalid_value_exception );
}
