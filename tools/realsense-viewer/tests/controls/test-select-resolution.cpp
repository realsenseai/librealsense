// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"


// Change resolution via the combo box for each sensor (if applicable), start streaming, and verify frames arrive
VIEWER_TEST( "controls", "select_resolution_and_stream" )
{
    auto & model = test.find_first_device_or_exit();

    for( auto && sub : model.subdevices )
    {
        if( sub->resolutions.empty() || sub->get_selected_profiles().empty() )
            continue;

        std::string target_res = "1280 x 720";
        if( std::find( sub->resolutions.begin(), sub->resolutions.end(), target_res ) == sub->resolutions.end() )
            target_res = sub->resolutions[0]; // if HD is not in the resolutions list, just take the first one

        test.expand_sensor_panel( model, sub );
        test.select_resolution( model, sub, target_res );

        test.collapse_sensor_panel( model, sub );

        test.click_stream_toggle_on( model, sub );
        IM_CHECK( test.all_streams_alive() );

        test.sleep( 3.0f );

        test.click_stream_toggle_off( model, sub );
        test.sleep( 2.0f );
    }

    IM_CHECK( !model.is_streaming() );
}
