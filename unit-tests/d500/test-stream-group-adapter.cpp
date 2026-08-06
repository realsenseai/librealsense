// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include <unit-tests/catch.h>
#include <src/ds/d500/d500-stream-group-adapter.h>
#include <src/librealsense-exception.h>

#include <algorithm>


namespace
{
    uint32_t fourcc( char a, char b, char c, char d )
    {
        return ( static_cast< uint32_t >( static_cast< uint8_t >( a ) ) << 24 )
            | ( static_cast< uint32_t >( static_cast< uint8_t >( b ) ) << 16 )
            | ( static_cast< uint32_t >( static_cast< uint8_t >( c ) ) << 8 )
            | static_cast< uint32_t >( static_cast< uint8_t >( d ) );
    }

    librealsense::platform::stream_profile profile(
        uint32_t format, uint32_t pin, uint32_t width = 1280,
        uint32_t height = 720, uint32_t fps = 30 )
    {
        librealsense::platform::stream_profile result = {};
        result.width = width;
        result.height = height;
        result.fps = fps;
        result.format = format;
        result.pin_index = pin;
        return result;
    }

    std::vector< librealsense::platform::stream_profile > advertised_profiles()
    {
        auto nv12 = fourcc( 'N', 'V', '1', '2' );
        auto yuy2 = fourcc( 'Y', 'U', 'Y', '2' );
        return {
            profile( fourcc( 'Z', '1', '6', ' ' ), 2 ),
            profile( fourcc( 'Y', '8', 'I', ' ' ), 5 ),
            // Colored-IR pin: native color without a YUY2 companion is not RGB.
            profile( nv12, 5 ),
            profile( nv12, 10 ), profile( yuy2, 10 ),
            profile( nv12, 20 ), profile( yuy2, 20 )
        };
    }
}


TEST_CASE( "D500 dual-color manifest is canonical regardless of selected profile order", "[d500]" )
{
    using namespace librealsense;
    auto nv12 = fourcc( 'N', 'V', '1', '2' );
    auto z16 = fourcc( 'Z', '1', '6', ' ' );
    std::vector< platform::stream_profile > selected = {
        profile( nv12, 10 ),  // lowest RGB pin -> firmware EP4 / left
        profile( nv12, 20 ),  // highest RGB pin -> firmware EP8 / right
        profile( z16, 2, 640, 480, 30 )
    };
    std::reverse( selected.begin(), selected.end() );

    auto manifest = d500_dual_color_stream_group_adapter::build_manifest(
        selected, advertised_profiles() );
    REQUIRE( manifest.size() == 3 );
    CHECK( manifest[0].branch == d500_stream_group_branch::depth );
    CHECK( manifest[0].fourcc == z16 );
    CHECK( manifest[0].width == 640 );
    CHECK( manifest[1].branch == d500_stream_group_branch::color_left );
    CHECK( manifest[1].fourcc == nv12 );
    CHECK( manifest[2].branch == d500_stream_group_branch::color_right );
}

TEST_CASE( "D500 dual-color manifest follows firmware endpoint branch mapping", "[d500]" )
{
    using namespace librealsense;
    auto nv12 = fourcc( 'N', 'V', '1', '2' );

    auto ep4 = d500_dual_color_stream_group_adapter::build_manifest(
        { profile( nv12, 10 ) }, advertised_profiles() );
    REQUIRE( ep4.size() == 1 );
    CHECK( ep4[0].branch == d500_stream_group_branch::color_left );

    auto ep8 = d500_dual_color_stream_group_adapter::build_manifest(
        { profile( nv12, 20 ) }, advertised_profiles() );
    REQUIRE( ep8.size() == 1 );
    CHECK( ep8[0].branch == d500_stream_group_branch::color_right );
}

TEST_CASE( "D500 dual-color manifest maps the physical stereo IR branch", "[d500]" )
{
    using namespace librealsense;
    auto y8i = fourcc( 'Y', '8', 'I', ' ' );
    auto manifest = d500_dual_color_stream_group_adapter::build_manifest(
        { profile( y8i, 5 ) }, advertised_profiles() );
    REQUIRE( manifest.size() == 1 );
    CHECK( manifest[0].branch == d500_stream_group_branch::infrared );
    CHECK( manifest[0].fourcc == y8i );
}

TEST_CASE( "D500 dual-color manifest maps single-channel IR to the physical IR branch", "[d500]" )
{
    using namespace librealsense;
    auto grey = fourcc( 'G', 'R', 'E', 'Y' );
    auto manifest = d500_dual_color_stream_group_adapter::build_manifest(
        { profile( grey, 5 ) }, advertised_profiles() );
    REQUIRE( manifest.size() == 1 );
    CHECK( manifest[0].branch == d500_stream_group_branch::infrared );
    CHECK( manifest[0].fourcc == grey );
}

TEST_CASE( "D500 dual-color manifest rejects ambiguous physical mappings", "[d500]" )
{
    using namespace librealsense;
    auto nv12 = fourcc( 'N', 'V', '1', '2' );

    CHECK_THROWS_AS( d500_dual_color_stream_group_adapter::build_manifest(
                         { profile( nv12, 5 ) }, advertised_profiles() ),
                     invalid_value_exception );
    CHECK_THROWS_AS( d500_dual_color_stream_group_adapter::build_manifest(
                         { profile( nv12, 20 ), profile( fourcc( 'Y', 'U', 'Y', '2' ), 20 ) },
                         advertised_profiles() ),
                     invalid_value_exception );
    CHECK_THROWS_AS( d500_dual_color_stream_group_adapter::build_manifest(
                         { profile( fourcc( 'M', 'J', 'P', 'G' ), 20 ) },
                         advertised_profiles() ),
                     invalid_value_exception );
}

TEST_CASE( "D500 dual-color manifest rejects values that do not fit V1", "[d500]" )
{
    using namespace librealsense;
    CHECK_THROWS_AS( d500_dual_color_stream_group_adapter::build_manifest(
                         { profile( fourcc( 'Z', '1', '6', ' ' ), 2, 65536, 480, 30 ) },
                         advertised_profiles() ),
                     invalid_value_exception );
}
