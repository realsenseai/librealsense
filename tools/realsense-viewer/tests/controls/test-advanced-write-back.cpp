// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"

#include <librealsense2/rs_advanced_mode.hpp>


// An advanced-mode section writes its whole group to the camera once, after the controls inside it
// have drawn: what the user dragged has to reach the camera, and only that section has to write.
VIEWER_TEST( "controls", "advanced_write_back" )
{
    auto & model = test.find_first_device_or_exit();
    if( ! model.dev.is< rs400::advanced_mode >()
        || ! model.dev.as< rs400::advanced_mode >().is_enabled() )
        return;   // nothing to write back on a device that has no advanced mode

    auto advanced = model.dev.as< rs400::advanced_mode >();

    std::shared_ptr< rs2::subdevice_model > sub;
    for( auto && s : model.subdevices )
        if( s->s->is< rs2::depth_sensor >() )
        {
            sub = s;
            break;
        }
    IM_CHECK( sub != nullptr );   // IM_CHECK returns on failure, so sub is non-null below

    test.expand_sensor_panel( model, sub );
    IM_CHECK( test.wait_until( 10, 0.3f, [&] {
        return test.node_shown( model, sub, { "Advanced Controls" } ); } ) );

    test.imgui->ItemOpen( test.node_id( model, sub, { "Advanced Controls" } ) );
    test.imgui->ItemOpen( test.node_id( model, sub, { "Advanced Controls", "Depth Control" } ) );
    // the slider labels its widget "##<name>", under the section it was registered in
    ImGuiID const slider = test.node_id( model, sub,
        { "Advanced Controls", "Depth Control", "##DS Median Threshold" } );
    IM_CHECK( test.wait_until( 10, 0.3f, [&] { return test.imgui->ItemExists( slider ); } ) );

    auto const original = advanced.get_depth_control( 0 ).deepSeaMedianThreshold;
    auto const minimum = advanced.get_depth_control( 1 ).deepSeaMedianThreshold;
    auto const target = ( original == minimum ) ? original + 100 : minimum;

    test.imgui->ItemInputValue( slider, (int)target );
    IM_CHECK( test.wait_until( 20, 0.25f, [&] {
        return advanced.get_depth_control( 0 ).deepSeaMedianThreshold == target; } ) );

    // and back, so the next test starts where this one found the camera
    test.imgui->ItemInputValue( slider, (int)original );
    IM_CHECK( test.wait_until( 20, 0.25f, [&] {
        return advanced.get_depth_control( 0 ).deepSeaMedianThreshold == original; } ) );
}
