// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"
#include "imgui_te_context.h"


VIEWER_TEST( "device", "device_detected" )
{
    IM_CHECK( !test.device_models.empty() );
}


VIEWER_TEST( "device", "hardware_reset" )
{
    IM_CHECK( !test.device_models.empty() );

    test.click_device_menu_item( *test.device_models[0], "Hardware Reset" );

    // Disconnect can be brief — poll at 50ms to catch it; allow up to 10s
    IM_CHECK( test.wait_until( 200, 0.05f, [&] { return test.device_models.empty(); } ) );
    // Reconnect takes several seconds; allow up to 20s
    IM_CHECK( test.wait_until( 40, 0.5f, [&] { return !test.device_models.empty(); } ) );
}
