// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"

#include <string>


// Regression test: a post-processing filter control set to a non-default value must keep it.
// Software-filter writes used to revert in the UI (the effect applied, but the control snapped
// back) because the async write path never refreshed the cached value. We assert on the cached
// value (what the slider shows after the user-request mask) without re-reading through the UI.
VIEWER_TEST( "controls", "post_processing_value_persists" )
{
    auto & model = test.find_first_device_or_exit();

    bool tested = false;
    for( auto && sub : model.subdevices )
    {
        // Decimation (a software post-processing filter) lives on the depth sensor
        auto pb = test.find_post_processing_filter( sub, RS2_OPTION_FILTER_MAGNITUDE );
        if( !pb )
            continue;
        rs2::option_model * om = pb->get_option_model( RS2_OPTION_FILTER_MAGNITUDE );

        test.click_stream_toggle_on( model, sub );
        test.sleep( 2.0f );

        test.expand_sensor_panel( model, sub );
        test.expand_post_processing( model, sub );
        test.enable_post_processing( model, sub );
        test.enable_post_processing_filter( model, sub, pb );
        test.expand_post_processing_filter( model, sub, pb );

        // The viewer persists filter options to the config file, so the start value varies between
        // runs — pick a target that differs from it so a revert would be observable.
        const float cur = pb->get_block()->get_option( RS2_OPTION_FILTER_MAGNITUDE );
        const std::string target = ( cur >= 4.f ) ? "2" : "6";
        const float target_f = std::stof( target );

        test.set_post_processing_value( model, sub, pb, RS2_OPTION_FILTER_MAGNITUDE, target );

        // Software-filter writes are synchronous, so the value is already applied by the time
        // set_post_processing_value returns; this poll is just a defensive safety net.
        IM_CHECK( test.wait_until( 20, 0.25f, [&] {
            return pb->get_block()->get_option( RS2_OPTION_FILTER_MAGNITUDE ) == target_f;
        } ) );

        // The control model's cached value must track the applied value (this is what the slider
        // shows once the user-request mask expires). Before the fix it stayed at the stale snapshot
        // and the control reverted. Allow a few frames for draw_option to drain the write
        // completion and refresh the cached value.
        IM_CHECK( test.wait_until( 20, 0.25f, [&] {
            return om->value->as_float == target_f;
        } ) );

        test.click_stream_toggle_off( model, sub );
        test.sleep( 1.0f );
        tested = true;
        break;
    }

    // No depth/decimation device present — nothing to validate, but don't fail the suite
    if( !tested )
        IM_CHECK( true );
}
