// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake: static!

#include <src/ds/d500/d585-mipi-matching-time.h>
#include <src/stream.h>
#include <unit-tests/catch.h>
#include <cstring>
#include <cmath>

using namespace librealsense;

namespace {
struct mipi_frame : frame
{
    bool find_metadata( rs2_frame_metadata_value key, rs2_metadata_type * value ) const override
    {
        if( ! has_capture_time || key != RS2_FRAME_METADATA_FRAME_TIMESTAMP )
            return false;
        *value = capture_us;
        return true;
    }

    void set_header_time( uint32_t us )
    {
        platform::uvc_header_mipi header{};
        header.header.timestamp = us;
        additional_data.metadata_size = sizeof( header );
        std::memcpy( additional_data.metadata_blob.data(), &header, sizeof( header ) );
    }

    uint32_t capture_us = 0;
    bool has_capture_time = true;
};
}

TEST_CASE( "D585 MIPI uses the exposure slot only for internal timestamp matching" )
{
    mipi_frame f;
    auto profile = std::make_shared< video_stream_profile >();
    profile->set_framerate( 30 );
    profile->set_stream_type( RS2_STREAM_COLOR );
    f.set_stream( profile );
    f.set_header_time( 0xfffffff0u );
    f.capture_us = 0x10u;  // The capture counter crossed zero 32 microseconds after the CSI header.

    for( auto domain : { RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK, RS2_TIMESTAMP_DOMAIN_GLOBAL_TIME } )
    {
        f.additional_data.timestamp_domain = domain;
        double const sdk_ms = domain == RS2_TIMESTAMP_DOMAIN_GLOBAL_TIME ? 1700000000000. : 12000.;
        f.additional_data.timestamp = sdk_ms;
        for( int eye : { 1, 2 } )
        {
            profile->set_stream_index( eye );
            const auto active = ds::d585_mipi_matching_time( f, RS2_PASSIVE_DEPTH_MODE_DISABLED );
            CHECK( std::abs( active - ( sdk_ms + .032 - 500. / 30 ) ) < .001 );
            for( auto mode : { RS2_PASSIVE_DEPTH_MODE_ALTERNATING, RS2_PASSIVE_DEPTH_MODE_FULL } )
                CHECK( std::abs( ds::d585_mipi_matching_time( f, mode ) - ( sdk_ms + .032 ) ) < .001 );
        }
        CHECK( f.get_frame_timestamp() == sdk_ms );
    }

    f.additional_data.timestamp = 1000.;
    f.set_header_time( 0x10u );
    f.capture_us = 0xfffffff0u;
    for( auto type : { RS2_STREAM_DEPTH, RS2_STREAM_INFRARED } )
    {
        profile->set_stream_type( type );
        CHECK( std::abs( ds::d585_mipi_matching_time( f, RS2_PASSIVE_DEPTH_MODE_DISABLED ) - 999.968 ) < .001 );
    }
    CHECK( ds::d585_mipi_matching_time( f, RS2_PASSIVE_DEPTH_MODE_COUNT ) == 1000. );
    f.additional_data.timestamp_domain = RS2_TIMESTAMP_DOMAIN_SYSTEM_TIME;
    CHECK( ds::d585_mipi_matching_time( f, RS2_PASSIVE_DEPTH_MODE_DISABLED ) == 1000. );
    f.additional_data.timestamp_domain = RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK;
    f.has_capture_time = false;
    CHECK( ds::d585_mipi_matching_time( f, RS2_PASSIVE_DEPTH_MODE_DISABLED ) == 1000. );
    f.has_capture_time = true;
    f.additional_data.metadata_size = 0;
    CHECK( ds::d585_mipi_matching_time( f, RS2_PASSIVE_DEPTH_MODE_DISABLED ) == 1000. );
}
