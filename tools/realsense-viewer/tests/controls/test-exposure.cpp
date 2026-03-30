// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"


// Set exposure manually, verify auto-exposure disables, toggle it back on, and confirm frames keep arriving
VIEWER_TEST( "controls", "set_exposure" )
{
    auto & model = test.find_first_device_or_exit();

    for( auto && sub : model.subdevices )
    {
        if( !test.has_option( sub, RS2_OPTION_EXPOSURE ) )
            continue;

        test.click_stream_toggle_on( model, sub );
        test.sleep( 2.0f );

        test.expand_sensor_panel( model, sub );
        test.expand_controls( model, sub );
        test.set_control_value( model, sub, RS2_OPTION_EXPOSURE, "100" );

        // Verify frames still arriving after exposure change
        IM_CHECK( test.all_streams_alive() );

        // setting exposure manually is expected to disable auto-exposure
        if( test.has_option( sub, RS2_OPTION_ENABLE_AUTO_EXPOSURE ) )
        {
            IM_CHECK( test.wait_until( 10, 0.5f, [&] {
                return test.get_control_value( model, sub, RS2_OPTION_ENABLE_AUTO_EXPOSURE ) == "0";
            } ) );

            // toggle it back on
            test.set_control_value( model, sub, RS2_OPTION_ENABLE_AUTO_EXPOSURE, "1" );
        }

        test.collapse_controls( model, sub );
        test.collapse_sensor_panel( model, sub );
        test.click_stream_toggle_off( model, sub );
        test.sleep( 1.0f );
    }

    IM_CHECK( !model.is_streaming() );
}
