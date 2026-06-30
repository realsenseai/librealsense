// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"
#include "imgui_te_context.h"


// ---------------------------------------------------------------------------
// viewer_test method implementations
//
// The imgui test engine locates UI elements by their label strings.
// The label/id helpers below must produce strings identical to what the viewer renders.
// SetRef("Control Panel") scopes subsequent item lookups to the viewer's left-side panel;
// most helpers call it first so that ItemClick/ItemOpen resolve within the correct window.
// Sleep() is skipped in --auto (fast) mode; use SleepNoSkip(seconds, framestep) to wait
// real wall-clock time.
// ---------------------------------------------------------------------------

rs2::device_model & viewer_test::find_first_device_or_exit()
{
    if( device_models.empty() )
    {
        IM_ERRORF( "%s", "no device connected" );
        throw test_exit();
    }
    return *device_models[0];
}

std::string viewer_test::sensor_label( rs2::device_model & model,
                                       std::shared_ptr< rs2::subdevice_model > sub )
{
    return rsutils::string::from()
        << sub->s->get_info( RS2_CAMERA_INFO_NAME ) << "##" << model.id;
}

std::string viewer_test::controls_label( rs2::device_model & model,
                                         std::shared_ptr< rs2::subdevice_model > sub )
{
    return rsutils::string::from()
        << "Controls ##" << sub->s->get_info( RS2_CAMERA_INFO_NAME ) << "," << model.id;
}

ImGuiID viewer_test::sensor_id_seed( rs2::device_model & model,
                                     std::shared_ptr< rs2::subdevice_model > sub )
{
    ImGuiWindow * cp = ImGui::FindWindowByName( "Control Panel" );
    if( !cp )
        return 0;
    return ImHashStr( sensor_label( model, sub ).c_str(), 0, cp->ID );
}

ImGuiID viewer_test::controls_id_seed( rs2::device_model & model,
                                       std::shared_ptr< rs2::subdevice_model > sub )
{
    return ImHashStr( controls_label( model, sub ).c_str(), 0, sensor_id_seed( model, sub ) );
}

void viewer_test::expand_sensor_panel( rs2::device_model & model,
                                       std::shared_ptr< rs2::subdevice_model > sub )
{
    imgui->SetRef( "Control Panel" );
    imgui->ItemOpen( sensor_label( model, sub ).c_str() );
    imgui->SleepNoSkip( 0.3f, 0.1f );
}

void viewer_test::collapse_sensor_panel( rs2::device_model & model,
                                         std::shared_ptr< rs2::subdevice_model > sub )
{
    imgui->SetRef( "Control Panel" );
    imgui->ItemClose( sensor_label( model, sub ).c_str() );
    imgui->SleepNoSkip( 0.3f, 0.1f );
}

void viewer_test::expand_controls( rs2::device_model & model,
                                   std::shared_ptr< rs2::subdevice_model > sub )
{
    if( !sub->num_supported_non_default_options() )
        return; // no options to show — controls section doesn't exist in the UI
    imgui->SetRef( "Control Panel" );
    std::string path = sensor_label( model, sub ) + "/" + controls_label( model, sub );
    imgui->ItemOpen( path.c_str() );
    imgui->SleepNoSkip( 0.3f, 0.1f );
}

void viewer_test::collapse_controls( rs2::device_model & model,
                                     std::shared_ptr< rs2::subdevice_model > sub )
{
    if( !sub->num_supported_non_default_options() )
        return; // no options to show — controls section doesn't exist in the UI
    imgui->SetRef( "Control Panel" );
    std::string path = sensor_label( model, sub ) + "/" + controls_label( model, sub );
    imgui->ItemClose( path.c_str() );
    imgui->SleepNoSkip( 0.3f, 0.1f );
}

void viewer_test::click_stream_toggle_on( rs2::device_model & model,
                                          std::shared_ptr< rs2::subdevice_model > sub )
{
    if( sub->streaming )
        throw std::runtime_error( "click_stream_toggle_on: sensor is already streaming" );
    imgui->SetRef( "Control Panel" );
    std::string label = rsutils::string::from()
        << rs2::textual_icons::toggle_off << "   off " << model.id << ", "
        << sub->s->get_info( RS2_CAMERA_INFO_NAME );
    imgui->ItemClick( label.c_str() );
}

void viewer_test::click_stream_toggle_off( rs2::device_model & model,
                                           std::shared_ptr< rs2::subdevice_model > sub )
{
    if( !sub->streaming )
        throw std::runtime_error( "click_stream_toggle_off: sensor is not streaming" );
    imgui->SetRef( "Control Panel" );
    std::string label = rsutils::string::from()
        << rs2::textual_icons::toggle_on << "   on  " << model.id << ","
        << sub->s->get_info( RS2_CAMERA_INFO_NAME );
    imgui->ItemClick( label.c_str() );
}

void viewer_test::click_device_menu_item( rs2::device_model & model, const std::string & item )
{
    // Construct the label matching the viewer's hamburger menu button — the test engine
    // locates UI elements by their ImGui label, so this must match what the viewer renders
    std::string bars_btn = rsutils::string::from()
        << rs2::textual_icons::bars << "##" << model.id;

    imgui->SetRef( "Control Panel" );
    imgui->ItemClick( bars_btn.c_str() );
    imgui->SleepNoSkip( 0.5f, 0.1f );

    IM_CHECK_SILENT( imgui->UiContext->NavWindow != nullptr );
    imgui->SetRef( imgui->UiContext->NavWindow );
    imgui->ItemClick( item.c_str() );
}

static rs2::option_model & find_option( std::shared_ptr< rs2::subdevice_model > sub,
                                        rs2_option option )
{
    auto it = sub->options_metadata.find( option );
    if( it == sub->options_metadata.end() )
        throw std::runtime_error( rsutils::string::from()
            << "option " << rs2_option_to_string( option ) << " not found on sensor" );
    return it->second;
}

void viewer_test::set_control_value( rs2::device_model & model,
                                     std::shared_ptr< rs2::subdevice_model > sub,
                                     rs2_option option, const std::string & value )
{
    auto & opt = find_option( sub, option );

    // Options can be at sensor level or inside the Controls section — try controls first
    ImGuiID seed = controls_id_seed( model, sub );
    if( !imgui->ItemExists( ImHashStr( opt.id.c_str(), 0, seed ) ) )
        seed = sensor_id_seed( model, sub );

    if( opt.is_checkbox() )
    {
        ImGuiID id = ImHashStr( opt.label.c_str(), 0, seed );
        bool desired = ( value != "0" );
        if( imgui->ItemIsChecked( id ) != desired )
            imgui->ItemClick( id );
    }
    else if( opt.is_enum() )
    {
        select_combo_item( ImHashStr( opt.id.c_str(), 0, seed ), value );
    }
    else
    {
        std::string edit_btn = rsutils::string::from()
            << rs2::textual_icons::edit << "##" << opt.id;
        imgui->ItemClick( ImHashStr( edit_btn.c_str(), 0, seed ) );
        imgui->ItemInput( ImHashStr( opt.id.c_str(), 0, seed ) );
        imgui->KeyCharsReplaceEnter( value.c_str() );
    }
}

std::string viewer_test::get_control_value( rs2::device_model & model,
                                            std::shared_ptr< rs2::subdevice_model > sub,
                                            rs2_option option )
{
    auto & opt = find_option( sub, option );

    ImGuiID seed = controls_id_seed( model, sub );
    if( !imgui->ItemExists( ImHashStr( opt.id.c_str(), 0, seed ) ) )
        seed = sensor_id_seed( model, sub );

    if( opt.is_checkbox() )
    {
        return imgui->ItemIsChecked( ImHashStr( opt.label.c_str(), 0, seed ) ) ? "1" : "0";
    }
    else if( opt.is_enum() )
    {
        int idx;
        imgui->ItemSelectAndReadValue( ImHashStr( opt.id.c_str(), 0, seed ), &idx );
        return opt.get_combo_labels()[idx];
    }
    else
    {
        float val;
        imgui->ItemSelectAndReadValue( ImHashStr( opt.id.c_str(), 0, seed ), &val );
        return opt.is_all_integers() ? std::to_string( (int)val ) : std::to_string( val );
    }
}

void viewer_test::select_combo_item( ImGuiID combo_id, const std::string & item )
{
    imgui->ItemClick( combo_id );
    imgui->SetRef( "//$FOCUSED" ); // scope lookups to the combobox that just opened
    imgui->ItemClick( item.c_str() );
}

void viewer_test::select_resolution( rs2::device_model & model,
                                     std::shared_ptr< rs2::subdevice_model > sub,
                                     const std::string & resolution )
{
    std::string label = rsutils::string::from()
        << "##" << sub->dev.get_info( RS2_CAMERA_INFO_NAME )
        << sub->s->get_info( RS2_CAMERA_INFO_NAME ) << " resolution";
    select_combo_item( ImHashStr( label.c_str(), 0, sensor_id_seed( model, sub ) ), resolution );
}

void viewer_test::select_fps( rs2::device_model & model,
                               std::shared_ptr< rs2::subdevice_model > sub,
                               const std::string & fps )
{
    std::string label = rsutils::string::from()
        << "##" << sub->dev.get_info( RS2_CAMERA_INFO_NAME )
        << sub->s->get_info( RS2_CAMERA_INFO_NAME ) << " fps";
    select_combo_item( ImHashStr( label.c_str(), 0, sensor_id_seed( model, sub ) ), fps );
}

bool viewer_test::has_option( std::shared_ptr< rs2::subdevice_model > sub, rs2_option option )
{
    try {
        auto & opt = find_option( sub, option );
        return opt.supported && !opt.read_only;
    }
    catch( ... ) { return false; }
}

bool viewer_test::all_streams_alive( int max_attempts, float interval )
{
    auto all_alive = [&]()
    {
        std::lock_guard< std::mutex > lock( viewer_model.streams_mutex );
        return !viewer_model.streams.empty()
            && std::all_of( viewer_model.streams.begin(), viewer_model.streams.end(),
                []( std::pair< const int, rs2::stream_model > & kv ) {
                    auto & stream = kv.second;
                    return stream.is_stream_alive();
                } );
    };
    return wait_until( max_attempts, interval, all_alive );
}
