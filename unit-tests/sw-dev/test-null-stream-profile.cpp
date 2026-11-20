// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "../catch.h"
#include <librealsense2/rs.hpp>
#include <librealsense2/hpp/rs_internal.hpp>

TEST_CASE( "sensor open rejects null stream profiles", "[software-device]" )
{
    rs2::software_device device;
    auto sensor = device.add_sensor( "software_sensor" );
    rs2_intrinsics intrinsics{};
    auto profile = sensor.add_video_stream(
        { RS2_STREAM_DEPTH, 0, 1, 640, 360, 30, 2, RS2_FORMAT_Z16, intrinsics } );

    // A bad entry must be rejected before the sensor receives any profiles,
    // regardless of whether the null entry appears first or after a valid one.
    REQUIRE_THROWS_AS(
        sensor.open( std::vector<rs2::stream_profile>{ rs2::stream_profile{} } ), rs2::error );
    REQUIRE_THROWS_AS(
        sensor.open( std::vector<rs2::stream_profile>{ profile, rs2::stream_profile{} } ),
        rs2::error );
    REQUIRE_NOTHROW( sensor.open( profile ) );
    REQUIRE_NOTHROW( sensor.close() );
}
