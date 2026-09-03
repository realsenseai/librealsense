// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"

#include <librealsense2/rs_advanced_mode.hpp>


// The search box sits at the sensor level, so one box reaches every control group the sensor draws:
// the Controls list, the advanced-mode sections, the post-processing filters and the embedded ones.
// A group with nothing matching is left out entirely rather than standing there empty, and the button
// beside the box brings everything back.
VIEWER_TEST( "controls", "global_search" )
{
    auto & model = test.find_first_device_or_exit();

    // the depth sensor is the one carrying every group
    std::shared_ptr< rs2::subdevice_model > sub;
    std::shared_ptr< rs2::processing_block_model > pb;
    for( auto && s : model.subdevices )
        if( ( pb = test.find_post_processing_filter( s, RS2_OPTION_FILTER_MAGNITUDE ) ) )
        {
            sub = s;
            break;
        }
    if( ! pb )
    {
        IM_ERRORF( "%s", "no sensor with post-processing filters" );
        throw test_exit();
    }

    controls_filter_reset const reset{ sub };

    test.expand_sensor_panel( model, sub );
    test.expand_controls( model, sub );
    test.expand_post_processing( model, sub );
    test.enable_post_processing( model, sub );
    test.enable_post_processing_filter( model, sub, pb );
    test.expand_post_processing_filter( model, sub, pb );

    // the filter's enable toggle is deferred to the panel's right edge, ~42px in - drawing it from
    // a dangling context would put it at the left instead
    float const inset = test.post_processing_toggle_inset( model, sub, pb );
    IM_CHECK( inset > 20.f );
    IM_CHECK( inset < 80.f );

    // with no search every group shows what it always shows
    IM_CHECK( ! test.controls_options( model, sub ).empty() );
    IM_CHECK( test.wait_until( 10, 0.3f, [&] {
        return test.post_processing_option_visible( model, sub, pb, RS2_OPTION_FILTER_MAGNITUDE ); } ) );

    // a post-processing option is reached by name, and stays reachable without a click; the Controls
    // list has nothing by that name, so it shows nothing at all
    test.set_controls_filter( model, sub, "magnitude" );
    IM_CHECK( test.wait_until( 10, 0.3f, [&] {
        return test.post_processing_option_visible( model, sub, pb, RS2_OPTION_FILTER_MAGNITUDE ); } ) );
    IM_CHECK( test.controls_options( model, sub ).empty() );

    // the advanced controls name themselves at their draw sites rather than coming from an option
    // list: a control name reaches its control, and a section name brings the whole section
    if( model.dev.is< rs400::advanced_mode >() && model.dev.as< rs400::advanced_mode >().is_enabled() )
    {
        test.set_controls_filter( model, sub, "ds median" );
        IM_CHECK( test.wait_until( 10, 0.3f, [&] {
            return test.node_shown( model, sub, { "Advanced Controls", "Depth Control", "##DS Median Threshold" } ); } ) );
        IM_CHECK( ! test.node_shown( model, sub, { "Advanced Controls", "Rsm" } ) );

        test.set_controls_filter( model, sub, "rsm" );
        IM_CHECK( test.wait_until( 10, 0.3f, [&] {
            return test.node_shown( model, sub, { "Advanced Controls", "Rsm" } ); } ) );
        IM_CHECK( test.node_shown( model, sub, { "Advanced Controls", "Rsm", "##Remove Threshold" } ) );
        IM_CHECK( ! test.node_shown( model, sub, { "Advanced Controls", "Depth Control" } ) );

        // "Advanced Controls" is the container, not a control: its own name names nothing the
        // user can set, so it must bring back nothing rather than an open, empty heading
        test.set_controls_filter( model, sub, "advanced" );
        IM_CHECK( test.wait_until( 10, 0.3f, [&] {
            return ! test.node_shown( model, sub, { "Advanced Controls" } ); } ) );
    }

    // the embedded filters are gated the same way as the post-processing blocks
    for( auto && ef : sub->embedded_filters )
    {
        auto const path = std::vector< std::string >{ test.embedded_filters_label( model ),
                                                      test.filter_label( model, ef->get_name() ) };
        test.set_controls_filter( model, sub, ef->get_name() );
        IM_CHECK( test.wait_until( 10, 0.3f, [&] { return test.node_shown( model, sub, path ); } ) );

        // the filter's enable option is its toggle, drawn beside the name and never under it, so it
        // must not keep the filter in the list on its own
        test.set_controls_filter( model, sub, "embedded filter enabled" );
        IM_CHECK( test.wait_until( 10, 0.3f, [&] { return ! test.node_shown( model, sub, path ); } ) );
        break;
    }

    // the options above the search box are not part of the Controls tree, so a name only they carry
    // must not leave the tree standing there empty
    if( test.has_option( sub, RS2_OPTION_VISUAL_PRESET ) )
    {
        test.set_controls_filter( model, sub, "visual preset" );
        IM_CHECK( test.wait_until( 10, 0.3f, [&] {
            return ! test.node_shown( model, sub, { test.controls_label( model, sub ) } ); } ) );
    }

    // a short query must not drag a filter in through an option the viewer never draws: every
    // post-processing block carries a hidden "Stream Index Filter", and "ex" is inside "Index"
    auto const pb_path = std::vector< std::string >{ test.post_processing_label( model ),
                                                     test.filter_label( model, pb->get_name() ) };
    test.set_controls_filter( model, sub, "ex" );
    IM_CHECK( test.wait_until( 10, 0.3f, [&] { return ! test.node_shown( model, sub, pb_path ); } ) );

    // nothing matching anywhere takes every group away, headings included
    test.set_controls_filter( model, sub, "zzzznosuchcontrol" );
    IM_CHECK( test.wait_until( 10, 0.3f, [&] {
        return test.controls_options( model, sub ).empty(); } ) );
    IM_CHECK( ! test.node_shown( model, sub, { test.post_processing_label( model ) } ) );
    IM_CHECK( ! test.node_shown( model, sub, { test.embedded_filters_label( model ) } ) );
    IM_CHECK( ! test.node_shown( model, sub, { "Advanced Controls" } ) );

    // the button beside the box clears the search, and every group comes back
    test.click_controls_filter_clear( model, sub );
    IM_CHECK( test.wait_until( 10, 0.3f, [&] {
        return ! test.controls_options( model, sub ).empty(); } ) );
    IM_CHECK( test.post_processing_option_visible( model, sub, pb, RS2_OPTION_FILTER_MAGNITUDE ) );

    test.collapse_controls( model, sub );
    test.collapse_sensor_panel( model, sub );
}
