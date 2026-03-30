// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"


// Start all sensors simultaneously and verify all streams are alive
VIEWER_TEST( "streaming", "stream_all_sensors" )
{
    auto & model = test.find_first_device_or_exit();

    for( auto && sub : model.subdevices )
        test.click_stream_toggle_on( model, sub );

    test.sleep( 1.0f );
    IM_CHECK( test.all_streams_alive() );

    test.sleep( 2.0f );

    for( auto && sub : model.subdevices )
        test.click_stream_toggle_off( model, sub );

    IM_CHECK( !model.is_streaming() );
}
