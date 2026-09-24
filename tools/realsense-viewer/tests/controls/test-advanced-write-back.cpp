// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"

#include <librealsense2/rs_advanced_mode.hpp>


// An advanced-mode control is edited the way a user does it: the pencil swaps the slider for a text
// box, the typed value is entered, and the section writes its whole group to the camera once the
// controls inside it have drawn. The pencil's state has to survive from one frame to the next, and
// only that section has to write.
VIEWER_TEST( "controls", "advanced_write_back" )
{
    auto & model = test.find_first_device_or_exit();
    if( ! model.dev.is< rs400::advanced_mode >()
        || ! model.dev.as< rs400::advanced_mode >().is_enabled() )
        return;   // nothing to write back on a device that has no advanced mode

    auto advanced = model.dev.as< rs400::advanced_mode >();

    // IM_CHECK returns from the test on failure, so the restore at the very end may never run -
    // put the group back on the way out regardless, or every later test inherits the probe value
    struct depth_control_restore
    {
        rs400::advanced_mode advanced;
        STDepthControlGroup saved;
        ~depth_control_restore() { try { advanced.set_depth_control( saved ); } catch( ... ) {} }
    } restore{ advanced, advanced.get_depth_control( 0 ) };

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
    // the slider and the text box that replaces it share the "##<name>" label under the section
    // they were registered in; the pencil beside them is "<edit icon>##<name>"
    ImGuiID const widget = test.node_id( model, sub,
        { "Advanced Controls", "Depth Control", "##DS Median Threshold" } );
    std::string const edit_label = rsutils::string::from()
        << rs2::textual_icons::edit << "##DS Median Threshold";
    ImGuiID const edit_button = test.node_id( model, sub,
        { "Advanced Controls", "Depth Control", edit_label } );
    IM_CHECK( test.wait_until( 10, 0.3f, [&] { return test.imgui->ItemExists( widget ); } ) );

    auto const original = advanced.get_depth_control( 0 ).deepSeaMedianThreshold;
    auto const minimum = advanced.get_depth_control( 1 ).deepSeaMedianThreshold;
    auto const target = ( original == minimum ) ? original + 100 : minimum;

    IM_CHECK( test.type_value( widget, edit_button, std::to_string( target ) ) );
    IM_CHECK( test.wait_until( 20, 0.25f, [&] {
        return advanced.get_depth_control( 0 ).deepSeaMedianThreshold == target; } ) );

    // and back, so the next test starts where this one found the camera
    IM_CHECK( test.type_value( widget, edit_button, std::to_string( original ) ) );
    IM_CHECK( test.wait_until( 20, 0.25f, [&] {
        return advanced.get_depth_control( 0 ).deepSeaMedianThreshold == original; } ) );
}
