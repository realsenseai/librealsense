// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"

#include <algorithm>
#include <string>
#include <vector>


// Check that the viewer's device list is non-empty, i.e. at least one camera is connected and visible on the viewer
VIEWER_TEST( "device", "device_detected" )
{
    IM_CHECK( !test.device_models.empty() );
}


namespace {

std::vector< std::string > sensor_names( rs2::device_model const & model )
{
    std::vector< std::string > names;
    for( auto && sub : model.subdevices )
        if( sub->s && sub->s->supports( RS2_CAMERA_INFO_NAME ) )
            names.push_back( sub->s->get_info( RS2_CAMERA_INFO_NAME ) );
    std::sort( names.begin(), names.end() );
    return names;
}

}  // namespace


// Reset the device via the UI menu and verify it disconnects and reconnects whole
VIEWER_TEST( "device", "hardware_reset" )
{
    auto & model = test.find_first_device_or_exit();
    auto const expected_sensors = sensor_names( model );

    test.click_device_menu_item( model, "Hardware Reset" );

    // Disconnect can be brief — poll at 50ms to catch it; allow up to 10s
    IM_CHECK( test.wait_until( 200, 0.05f, [&] { return test.device_models.empty(); } ) );

    // Reconnect takes several seconds. Poll fast and latch the sensor set of the
    // FIRST device the viewer publishes: a device published before all of its
    // interfaces have enumerated is superseded by a repaired one a second or two
    // later, so a slow poll would only ever see the good one.
    std::vector< std::string > first_seen;
    IM_CHECK( test.wait_until( 400, 0.05f,
                               [&]
                               {
                                   if( test.device_models.empty() )
                                       return false;
                                   if( first_seen.empty() )
                                       first_seen = sensor_names( *test.device_models.front() );
                                   return true;
                               } ) );

    // The camera must come back whole. Missing sensors here - typically the Motion
    // Module - is the partial-enumeration race, and leaves the user with no IMU.
    IM_CHECK_EQ( first_seen, expected_sensors );
}
