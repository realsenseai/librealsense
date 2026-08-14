// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include <unit-tests/catch.h>

#include <src/object-detection-frame.h>
#include <rsutils/number/crc32.h>

#include <cstring>

using librealsense::object_detection_frame;

namespace
{
template< class Entry >
object_detection_frame make_frame( uint16_t version, Entry const & entry, size_t trailing_padding = 0 )
{
    using frame_header = object_detection_frame::object_detection_frame_header;
    using payload_header = object_detection_frame::object_detection_payload_header;

    object_detection_frame frame;
    frame.data.resize( sizeof( frame_header ) + sizeof( payload_header ) + sizeof( Entry ) + trailing_padding );

    frame_header header{};
    header.magic_number = object_detection_frame::MAGIC_NUMBER;
    header.version = version;
    header.data_type = 0;
    header.size = static_cast< uint32_t >( sizeof( payload_header ) + sizeof( Entry ) );

    payload_header payload{};
    payload.timestamp_ms = 1234.5;
    payload.frame_id = 17;
    payload.number_of_detections = 1;
    payload.source = 0;
    payload.source_frame_id = 16;

    auto * dst = frame.data.data();
    memcpy( dst, &header, sizeof( header ) );
    memcpy( dst + sizeof( header ), &payload, sizeof( payload ) );
    memcpy( dst + sizeof( header ) + sizeof( payload ), &entry, sizeof( entry ) );

    auto * stored_header = reinterpret_cast< frame_header * >( dst );
    stored_header->crc32 = rsutils::number::calc_crc32(
        dst + sizeof( frame_header ), stored_header->size );
    return frame;
}
}

TEST_CASE( "Object Detection v2 frame matches the firmware ABI", "[object-detection][frame]" )
{
    STATIC_REQUIRE( sizeof( object_detection_frame::object_detection_frame_header ) == 20 );
    STATIC_REQUIRE( sizeof( object_detection_frame::object_detection_payload_header ) == 23 );
    STATIC_REQUIRE( sizeof( object_detection_frame::object_detection_entry_v2 ) == 16 );

    object_detection_frame::object_detection_entry_v2 entry{};
    entry.detection_id = 100;
    entry.detection_type = 3;
    entry.confidence = 90;
    entry.top_left_x = 10;
    entry.top_left_y = 20;
    entry.bottom_right_x = 110;
    entry.bottom_right_y = 220;
    entry.distance = 1.25f;

    auto frame = make_frame( object_detection_frame::VERSION_V2, entry );
    REQUIRE( frame.get_detection_count() == 1 );
    auto const detection = frame.get_detection( 0 );
    CHECK( detection.detection_id == 100 );
    CHECK( detection.detection_type == 3 );
    CHECK( detection.confidence == 90 );
    CHECK( detection.bottom_right_x == 110 );
    CHECK( detection.distance == Catch::Approx( 1.25f ) );
    CHECK_FALSE( detection.com_valid );
    CHECK( detection.world_position.z == 0.f );
}

TEST_CASE( "Object Detection v3 frame exposes COM", "[object-detection][frame]" )
{
    STATIC_REQUIRE( sizeof( object_detection_frame::object_detection_entry_v3 ) == 36 );

    object_detection_frame::object_detection_entry_v3 entry{};
    entry.detection.detection_id = 7;
    entry.detection.detection_type = 0;
    entry.detection.confidence = 92;
    entry.detection.top_left_x = 50;
    entry.detection.top_left_y = 60;
    entry.detection.bottom_right_x = 200;
    entry.detection.bottom_right_y = 300;
    entry.detection.distance = 2.291f;
    entry.world_x = 1.f;
    entry.world_y = -0.5f;
    entry.world_z = 2.f;
    entry.image_x = 321.5f;
    entry.image_y = 181.25f;

    // USB UVC frames are fixed-size and can contain padding after the valid payload.
    auto frame = make_frame( object_detection_frame::VERSION_V3, entry, 128 );
    REQUIRE( frame.get_detection_count() == 1 );
    auto const detection = frame.get_detection( 0 );
    CHECK( detection.com_valid );
    CHECK( detection.distance == Catch::Approx( 2.291f ) );
    CHECK( detection.world_position.x == Catch::Approx( 1.f ) );
    CHECK( detection.world_position.y == Catch::Approx( -0.5f ) );
    CHECK( detection.world_position.z == Catch::Approx( 2.f ) );
    CHECK( detection.image_x == Catch::Approx( 321.5f ) );
    CHECK( detection.image_y == Catch::Approx( 181.25f ) );
}

TEST_CASE( "Object Detection frame rejects malformed headers", "[object-detection][frame]" )
{
    object_detection_frame::object_detection_entry_v2 entry{};

    SECTION( "CRC mismatch" )
    {
        auto frame = make_frame( object_detection_frame::VERSION_V2, entry );
        auto * header = reinterpret_cast< object_detection_frame::object_detection_frame_header * >(
            frame.data.data() );
        ++header->crc32;
        CHECK( frame.get_detection_count() == 0 );
        CHECK_THROWS_AS( frame.get_detection( 0 ), std::out_of_range );
    }

    SECTION( "unknown version" )
    {
        auto frame = make_frame( 0x0400, entry );
        CHECK( frame.get_detection_count() == 0 );
    }

    SECTION( "size mismatch" )
    {
        auto frame = make_frame( object_detection_frame::VERSION_V2, entry );
        auto * header = reinterpret_cast< object_detection_frame::object_detection_frame_header * >(
            frame.data.data() );
        ++header->size;
        CHECK( frame.get_detection_count() == 0 );
    }
}
