// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"
#include "imgui_te_context.h"


// ---------------------------------------------------------------------------
// viewer_test method implementations
// ---------------------------------------------------------------------------

std::string viewer_test::sensor_label( std::shared_ptr< rs2::subdevice_model > sub,
                                               rs2::device_model & model )
{
    return rsutils::string::from()
        << sub->s->get_info( RS2_CAMERA_INFO_NAME ) << "##" << model.id;
}

std::string viewer_test::controls_label( std::shared_ptr< rs2::subdevice_model > sub,
                                                  rs2::device_model & model )
{
    return rsutils::string::from()
        << "Controls ##" << sub->s->get_info( RS2_CAMERA_INFO_NAME ) << "," << model.id;
}

ImGuiID viewer_test::sensor_id_seed( std::shared_ptr< rs2::subdevice_model > sub,
                                              rs2::device_model & model )
{
    ImGuiWindow * cp = ImGui::FindWindowByName( "Control Panel" );
    if( !cp )
        return 0;
    return ImHashStr( sensor_label( sub, model ).c_str(), 0, cp->ID );
}

ImGuiID viewer_test::controls_id_seed( std::shared_ptr< rs2::subdevice_model > sub,
                                                rs2::device_model & model )
{
    return ImHashStr( controls_label( sub, model ).c_str(), 0, sensor_id_seed( sub, model ) );
}

void viewer_test::expand_sensor_panel( std::shared_ptr< rs2::subdevice_model > sub,
                                                rs2::device_model & model,
                                                bool open_controls )
{
    imgui->SetRef( "Control Panel" );
    std::string sl = sensor_label( sub, model );
    imgui->ItemOpen( sl.c_str() );
    if( open_controls && sub->num_supported_non_default_options() )
    {
        std::string path = sl + "/" + controls_label( sub, model );
        imgui->ItemOpen( path.c_str() );
    }
    imgui->SleepNoSkip( 0.3f, 0.1f );
}

void viewer_test::collapse_sensor_panel( std::shared_ptr< rs2::subdevice_model > sub,
                                                  rs2::device_model & model,
                                                  bool close_controls )
{
    imgui->SetRef( "Control Panel" );
    std::string sl = sensor_label( sub, model );
    if( close_controls && sub->num_supported_non_default_options() )
    {
        std::string path = sl + "/" + controls_label( sub, model );
        imgui->ItemClose( path.c_str() );
    }
    imgui->ItemClose( sl.c_str() );
    imgui->SleepNoSkip( 0.3f, 0.1f );
}

void viewer_test::click_toggle_on( std::shared_ptr< rs2::subdevice_model > sub,
                                            rs2::device_model & model )
{
    if( sub->streaming )
        return;
    imgui->SetRef( "Control Panel" );
    std::string label = rsutils::string::from()
        << rs2::textual_icons::toggle_off << "   off " << model.id << ", "
        << sub->s->get_info( RS2_CAMERA_INFO_NAME );
    imgui->ItemClick( label.c_str() );
}

void viewer_test::click_toggle_off( std::shared_ptr< rs2::subdevice_model > sub,
                                             rs2::device_model & model )
{
    if( !sub->streaming )
        return;
    imgui->SetRef( "Control Panel" );
    std::string label = rsutils::string::from()
        << rs2::textual_icons::toggle_on << "   on  " << model.id << ","
        << sub->s->get_info( RS2_CAMERA_INFO_NAME );
    imgui->ItemClick( label.c_str() );
}

void viewer_test::click_device_menu_item( rs2::device_model & model, const char * item )
{
    std::string bars_btn = rsutils::string::from()
        << rs2::textual_icons::bars << "##" << model.id;

    imgui->SetRef( "Control Panel" );
    imgui->ItemClick( bars_btn.c_str() );
    imgui->SleepNoSkip( 0.5f, 0.1f );

    IM_CHECK_SILENT( imgui->UiContext->NavWindow != nullptr );
    imgui->SetRef( imgui->UiContext->NavWindow );
    imgui->ItemClick( item );
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

void viewer_test::set_option_value( std::shared_ptr< rs2::subdevice_model > sub,
                                    rs2::device_model & model,
                                    rs2_option option, const char * value )
{
    auto & opt = find_option( sub, option );
    if( opt.is_enum() || opt.is_checkbox() )
        throw std::runtime_error( rsutils::string::from()
            << rs2_option_to_string( option ) << " is not a slider" );

    ImGuiID seed = sub->num_supported_non_default_options()
                     ? controls_id_seed( sub, model )
                     : sensor_id_seed( sub, model );

    std::string edit_btn = rsutils::string::from()
        << rs2::textual_icons::edit << "##" << opt.id;
    imgui->ItemClick( ImHashStr( edit_btn.c_str(), 0, seed ) );

    imgui->ItemInput( ImHashStr( opt.id.c_str(), 0, seed ) );
    imgui->KeyCharsReplaceEnter( value );
}

void viewer_test::toggle_option( std::shared_ptr< rs2::subdevice_model > sub,
                                 rs2::device_model & model,
                                 rs2_option option )
{
    auto & opt = find_option( sub, option );
    if( !opt.is_checkbox() )
        throw std::runtime_error( rsutils::string::from()
            << rs2_option_to_string( option ) << " is not a checkbox, use set_option_value instead" );

    ImGuiID seed = sensor_id_seed( sub, model );
    imgui->ItemClick( ImHashStr( opt.label.c_str(), 0, seed ) );
}

bool viewer_test::is_option_checked( std::shared_ptr< rs2::subdevice_model > sub,
                                     rs2::device_model & model,
                                     rs2_option option )
{
    auto & opt = find_option( sub, option );
    if( !opt.is_checkbox() )
        throw std::runtime_error( rsutils::string::from()
            << rs2_option_to_string( option ) << " is not a checkbox, use set_option_value instead" );

    ImGuiID seed = sensor_id_seed( sub, model );
    return imgui->ItemIsChecked( ImHashStr( opt.label.c_str(), 0, seed ) );
}

void viewer_test::select_combo_item( std::shared_ptr< rs2::subdevice_model > sub,
                                     rs2::device_model & model,
                                     const char * combo_label, const char * item )
{
    ImGuiID combo_id = ImHashStr( combo_label, 0, sensor_id_seed( sub, model ) );
    imgui->ItemClick( combo_id );
    imgui->SetRef( "//$FOCUSED" );
    imgui->ItemClick( item );
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
