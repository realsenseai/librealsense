// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"


// Start and stop each sensor one at a time, verifying frames arrive for each
VIEWER_TEST( "streaming", "stream_each_sensor_individually" )
{
    auto & model = test.find_first_device_or_exit();

    for( auto && sub : model.subdevices )
    {
        if( sub->get_selected_profiles().empty() )
            continue;

        test.click_stream_toggle_on( model, sub );
        IM_CHECK( test.all_streams_alive() );
        test.sleep( 2.0f );
        test.click_stream_toggle_off( model, sub );
        test.sleep( 1.0f );
    }

    IM_CHECK( !model.is_streaming() );
}
