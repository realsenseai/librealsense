// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"


// Check that the viewer's device list is non-empty, i.e. at least one camera is connected and visible on the viewer
VIEWER_TEST( "device", "device_detected" )
{
    IM_CHECK( !test.device_models.empty() );
}


// Reset the device via the UI menu and verify it disconnects and reconnects
VIEWER_TEST( "device", "hardware_reset" )
{
    auto & model = test.find_first_device_or_exit();

    test.click_device_menu_item( model, "Hardware Reset" );

    // Disconnect can be brief — poll at 50ms to catch it; allow up to 10s
    IM_CHECK( test.wait_until( 200, 0.05f, [&] { return test.device_models.empty(); } ) );
    // Reconnect takes several seconds; allow up to 20s
    IM_CHECK( test.wait_until( 20, 1.0f, [&] { return !test.device_models.empty(); } ) );
}
