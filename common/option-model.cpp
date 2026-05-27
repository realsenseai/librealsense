// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include "option-model.h"
#include <realsense_imgui.h>
#include <librealsense2/rs_advanced_mode.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include "device-model.h"
#include "subdevice-model.h"
#include <os.h>

#include <rsutils/easylogging/easyloggingpp.h>

#include <stdexcept>

namespace rs2
{
    option_model create_option_model( option_value const & opt,
        const std::string& opt_base_label,
        subdevice_model* model,
        std::shared_ptr<options> options,
        bool* options_invalidated,
        std::string& error_message)
    {
        option_model option = {};

        std::string const option_name = options->get_option_name( opt->id );
        option.id = rsutils::string::from() << opt_base_label << '/' << option_name;
        option.opt = opt->id;
        option.endpoint = options;
        option.label = rsutils::string::from() << option_name << "##" << option.id;
        option.invalidate_flag = options_invalidated;
        option.dev = model;
        option.value = opt;
        option.supported = opt->is_valid;  // i.e., supported-and-enabled!
        option.range = options->get_option_range( opt->id );
        option.read_only = options->is_option_read_only( opt->id );
        option.last_slider_hold_stopwatch.reset( {} ); // Avoids seeming as if a slider was dragged and just released.
        return option;
    }
}

using namespace rs2;

std::string option_model::adjust_description(const std::string& str_in, const std::string& to_be_replaced, const std::string& to_replace)
{
    std::string adjusted_string(str_in);
    auto pos = adjusted_string.find(to_be_replaced);
    adjusted_string.replace(pos, to_be_replaced.size(), to_replace);
    return adjusted_string;
}

bool option_model::draw( std::string & error_message,
                         notifications_model & model,
                         bool new_line,
                         bool use_option_name )
{
    auto res = false;
    if( endpoint->supports( opt ) )
    {
        std::string desc_str( endpoint->get_option_description( opt ) );

        // The option's rendering model supports an alternative option title derived from its
        // description rather than name. This is applied to the Holes Filling as its display must
        // conform with the names used by a 3rd-party tools for consistency.
        if (opt == RS2_OPTION_HOLES_FILL)
        {
            use_option_name = false;
            // Below change is instead of the long description provided with DDS
            // which is useful when user does not know what are the options' possible values
            desc_str = "Persistency mode"; 
        }

        // Device D405 is for short range, therefore, its units are in cm - for better UX
        bool use_cm_units = false;
        std::string device_pid = dev->dev.get_info( RS2_CAMERA_INFO_PRODUCT_ID );
        if( device_pid == "0B5B"
            && val_in_range(
                opt,
                { RS2_OPTION_MIN_DISTANCE, RS2_OPTION_MAX_DISTANCE, RS2_OPTION_DEPTH_UNITS } ) )
        {
            use_cm_units = true;
            desc_str = adjust_description( desc_str, "meters", "cm" );
        }

        auto desc = desc_str.c_str();

        // remain option to append to the current line
        if( ! new_line )
            ImGui::SameLine();

        if( is_enum() )
        {
            res = draw_combobox( model, error_message, desc, new_line, use_option_name );
        }
        else
        {
            if( is_checkbox() )
            {
                res = draw_checkbox( model, error_message, desc );
            }
            else
            {
                res = draw_slider( model, error_message, desc, use_cm_units );
            }
        }

        if( ! read_only && opt == RS2_OPTION_ENABLE_AUTO_EXPOSURE && dev->auto_exposure_enabled
            && dev->s->is< roi_sensor >() && dev->streaming )
        {
            ImGui::SameLine( 0, 10 );
            std::string button_label = label;
            auto index = label.find_last_of( '#' );
            if( index != std::string::npos )
            {
                button_label = label.substr( index + 1 );
            }

            ImGui::PushStyleColor( ImGuiCol_TextSelectedBg, { 1.f, 1.f, 1.f, 1.f } );
            if( ! dev->roi_checked )
            {
                std::string caption = rsutils::string::from() << "Set ROI##" << button_label;
                if( ImGui::Button( caption.c_str(), { 55, 0 } ) )
                {
                    dev->roi_checked = true;
                }
            }
            else
            {
                std::string caption = rsutils::string::from() << "Cancel##" << button_label;
                if( ImGui::Button( caption.c_str(), { 55, 0 } ) )
                {
                    dev->roi_checked = false;
                }
            }
            ImGui::PopStyleColor();

            if( ImGui::IsItemHovered() )
                RsImGui::CustomTooltip( "Select custom region of interest for the auto-exposure "
                                   "algorithm\nClick the button, then draw a rect on the frame" );
        }

        auto advanced = dev->dev.as< rs400::advanced_mode >();
        auto supports_auto_hdr = std::string(dev->dev.get_info(RS2_CAMERA_INFO_NAME)).find("D45") != std::string::npos; // auto HDR is only supported on D45*
        if( opt == RS2_OPTION_HDR_ENABLED && advanced && advanced.is_enabled() && supports_auto_hdr)
        {
            ImGui::SameLine( 0, 10 );

            std::string button_label = "HDR Config";
            std::string caption = rsutils::string::from() << "HDR Config##" << button_label;

            RsImGui::RsImButton(
                [&]()
                {
                    if( ImGui::Button( caption.c_str(), { 85, 0 } ) )
                    {
                        try
                        {
                            dev->dev_model->open_hdr_config_tool_window();
                        }
                        catch( const std::exception & e )
                        {
                            error_message = rsutils::string::from() << "Failed to open HDR configuration: " << e.what();
                        }
                    }
                });
        }
    
    }

    return res;
}

void option_model::update_supported( std::string & error_message )
{
    try
    {
        supported = endpoint->supports( opt );
    }
    catch( const error & e )
    {
        error_message = error_to_string( e );
    }
}

void option_model::update_read_only_status( std::string & error_message )
{
    try
    {
        read_only = endpoint->is_option_read_only( opt );
    }
    catch( const error & e )
    {
        error_message = error_to_string( e );
    }
}

void option_model::update_all_fields( std::string & error_message, notifications_model & model )
{
    try
    {
        // After slider was dragged value updated using set_option, don't update value again here
        if( last_slider_hold_stopwatch.get_elapsed_ms() < 500 )
            return;

        value = endpoint->get_option_value( opt );
        _has_user_request->store( false );
        supported = value->is_valid;
        if( supported )
        {
            range = endpoint->get_option_range( opt );
            read_only = endpoint->is_option_read_only( opt );
        }
    }
    catch( const error & e )
    {
        if( read_only )
        {
            model.add_notification( { rsutils::string::from()
                                          << "Could not refresh read-only option "
                                          << endpoint->get_option_name( opt ) << ": " << e.what(),
                                      RS2_LOG_SEVERITY_WARN,
                                      RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR } );
        }
        else
            error_message = error_to_string( e );
    }
}

bool option_model::is_all_integers() const
{
    return is_integer( range.min ) && is_integer( range.max ) && is_integer( range.def )
        && is_integer( range.step );
}

bool option_model::is_enum() const
{
    // We do not expect enum values to have a step that is smaller than 1,
    // and we don't want to compare a floating point value to an integer so 0.9 will do the work.
    if( range.step < 0.9f )
        return false;

    for( auto i = range.min; i <= range.max; i += range.step )
    {
        if( endpoint->get_option_value_description( opt, i ) == nullptr )
            return false;
    }
    return true;
}

std::vector< const char * > option_model::get_combo_labels( int * p_selected ) const
{
    int selected = 0, counter = 0;
    std::vector< const char * > labels;
    for( auto i = range.min; i <= range.max; i += range.step, counter++ )
    {
        auto label = endpoint->get_option_value_description( opt, i );

        switch( value->type )
        {
        case RS2_OPTION_TYPE_STRING:
            if( 0 == strcmp( label, value->as_string ) )
                selected = counter;
            break;

        default:
            if( std::fabs( i - value_as_float() ) < 0.001f )
                selected = counter;
            break;
        }

        labels.push_back( label );
    }
    if( p_selected )
        *p_selected = selected;
    return labels;
}

bool option_model::draw_combobox( notifications_model & model,
                                  std::string & error_message,
                                  const char * description,
                                  bool new_line,
                                  bool use_option_name )
{
    bool item_clicked = false;
    std::string txt = rsutils::string::from()
                   << ( use_option_name ? endpoint->get_option_name( opt ) : description ) << ":";

    float text_length = ImGui::CalcTextSize( txt.c_str() ).x;
    float combo_position_x = ImGui::GetCursorPosX() + text_length + 5;

    ImGui::Text( "%s", txt.c_str() );
    if( ImGui::IsItemHovered() && description )
    {
        RsImGui::CustomTooltip( "%s", description );
    }

    ImGui::SameLine();
    if( new_line )
        ImGui::SetCursorPosX( combo_position_x );

    ImGui::PushItemWidth( new_line ? ImGui::GetContentRegionAvail().x - 25 : 100.f );

    int selected;
    std::vector< const char * > labels = get_combo_labels( &selected );
    ImGui::PushStyleColor( ImGuiCol_TextSelectedBg, { 1, 1, 1, 1 } );

    try
    {
        if( RsImGui::CustomComboBox( id.c_str(), &selected, labels.data(), static_cast< int >( labels.size() ) ) )
        {
            float tmp_value = range.min + range.step * selected;
            model.add_log( rsutils::string::from()
                           << "Setting " << opt << " to " << tmp_value << " (" << labels[selected] << ")" );
            set_option( opt, tmp_value );
            if( invalidate_flag )
                *invalidate_flag = true;
            item_clicked = true;
        }
    }
    catch( const error & e )
    {
        error_message = error_to_string( e );
    }

    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    return item_clicked;
}


float option_model::value_as_float() const
{
    // While the user-requested value is fresh, prefer it over the stale cached `value`
    // so the slider doesn't visually snap back between dispatch and FW echo. Cap at 2 s
    // in case the FW echoes a different value (rejected/clamped) — beyond that we want
    // the user to see what was actually applied.
    if( _has_user_request->load() && _user_request_stopwatch.get_elapsed_ms() < 2000 )
        return _user_request_value;

    switch( value->type )
    {
    case RS2_OPTION_TYPE_FLOAT:
        return value->as_float;
        break;

    case RS2_OPTION_TYPE_INTEGER:
    case RS2_OPTION_TYPE_BOOLEAN:
        return float( value->as_integer );
        break;
    case RS2_OPTION_TYPE_STRING:
        if( range.min == 0.f && range.step == 1.f ) // We can convert enum option to float
        {
            for( auto i = 0.f; i <= range.max; i += range.step )
            {
                auto desc = endpoint->get_option_value_description( opt, i );
                if( ! desc )
                    break;
                if( strcmp( value->as_string, desc ) == 0 )
                    return i;
            }
        }
        break;
    }
    return 0.f;
}


std::string option_model::value_as_string() const
{
    switch( value->type )
    {
    case RS2_OPTION_TYPE_FLOAT:
        if( is_all_integers() )
            return rsutils::string::from() << (int) value->as_float;
        else
            return rsutils::string::from() << value->as_float;
        break;

    case RS2_OPTION_TYPE_INTEGER:
    case RS2_OPTION_TYPE_BOOLEAN:
        return rsutils::string::from() << value->as_integer;
        break;

    case RS2_OPTION_TYPE_STRING:
        return value->as_string;
        break;
    }
    return {};
}


bool option_model::draw_slider( notifications_model & model,
                                std::string & error_message,
                                const char * description,
                                bool use_cm_units )
{
    bool slider_clicked = false;
    std::string txt = rsutils::string::from() << endpoint->get_option_name( opt ) << ":";
    ImGui::Text( "%s", txt.c_str() );

    ImGui::SameLine();
    ImGui::SetCursorPosX( read_only ? 280.f : 257.f );
    ImGui::PushStyleColor( ImGuiCol_Text, grey );
    ImGui::PushStyleColor( ImGuiCol_TextSelectedBg, grey );
    ImGui::PushStyleColor( ImGuiCol_ButtonActive, { 1.f, 1.f, 1.f, 0.f } );
    ImGui::PushStyleColor( ImGuiCol_ButtonHovered, { 1.f, 1.f, 1.f, 0.f } );
    ImGui::PushStyleColor( ImGuiCol_Button, { 1.f, 1.f, 1.f, 0.f } );
    ImGui::Button( textual_icons::question_mark, { 20, 20 } );
    ImGui::PopStyleColor( 5 );
    if( ImGui::IsItemHovered() && description )
    {
        RsImGui::CustomTooltip( "%s", description );
    }

    if( ! read_only )
    {
        ImGui::SameLine();
        ImGui::SetCursorPosX( 280 );
        if( ! edit_mode )
        {
            std::string edit_id = rsutils::string::from() << textual_icons::edit << "##" << id;
            ImGui::PushStyleColor( ImGuiCol_Text, light_grey );
            ImGui::PushStyleColor( ImGuiCol_TextSelectedBg, light_grey );
            ImGui::PushStyleColor( ImGuiCol_ButtonHovered, { 1.f, 1.f, 1.f, 0.f } );
            ImGui::PushStyleColor( ImGuiCol_Button, { 1.f, 1.f, 1.f, 0.f } );
            if( ImGui::Button( edit_id.c_str(), { 20, 20 } ) )
            {
                edit_value = value_as_string();
                edit_mode = true;
            }
            if( ImGui::IsItemHovered() )
            {
                RsImGui::CustomTooltip( "Enter text-edit mode" );
            }
            ImGui::PopStyleColor( 4 );
        }
        else
        {
            std::string edit_id = rsutils::string::from() << textual_icons::edit << "##" << id;
            ImGui::PushStyleColor( ImGuiCol_Text, light_blue );
            ImGui::PushStyleColor( ImGuiCol_TextSelectedBg, light_blue );
            ImGui::PushStyleColor( ImGuiCol_ButtonHovered, { 1.f, 1.f, 1.f, 0.f } );
            ImGui::PushStyleColor( ImGuiCol_Button, { 1.f, 1.f, 1.f, 0.f } );
            if( ImGui::Button( edit_id.c_str(), { 20, 20 } ) )
            {
                edit_mode = false;
            }
            if( ImGui::IsItemHovered() )
            {
                RsImGui::CustomTooltip( "Exit text-edit mode" );
            }
            ImGui::PopStyleColor( 4 );
        }
    }
    float customWidth = 295 - ImGui::GetCursorPosX(); //set slider width from the current Xpos to the right border at 295 (the edit button pos)
    ImGui::PushItemWidth(customWidth);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, black);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, black);
    try
    {
        if( read_only )
        {
            ImVec2 vec{ 0, 20 };
            std::string text = value_as_string();
            if( range.min != range.max )
            {
                ImGui::ProgressBar( ( value_as_float() / ( range.max - range.min ) ), vec, text.c_str() );
            }
            else  // constant value options
            {
                auto c = ImGui::ColorConvertU32ToFloat4( ImGui::GetColorU32( ImGuiCol_FrameBg ) );
                ImGui::PushStyleColor( ImGuiCol_FrameBgActive, c );
                ImGui::PushStyleColor( ImGuiCol_FrameBgHovered, c );
                float dummy = std::floor( value_as_float() );
                if( ImGui::DragFloat( id.c_str(), &dummy, 1, 0, 0, text.c_str() ) )
                {
                    // Changing the depth units not on advanced mode is not allowed,
                    // prompt the user to switch to advanced mode for chaging it.
                    if( RS2_OPTION_DEPTH_UNITS == opt )
                    {
                        auto advanced = dev->dev.as< rs400::advanced_mode >();
                        if( advanced )
                            if( ! advanced.is_enabled() )
                                dev->draw_advanced_mode_prompt = true;
                    }
                }
                ImGui::PopStyleColor( 2 );
            }
        }
        else if( edit_mode )
        {
            std::string buff_str = edit_value;

            // lambda function used to convert meters to cm - while the number is a string
            auto convert_float_str = []( std::string float_str, float conversion_factor ) {
                if( float_str.size() == 0 )
                    return float_str;
                float number_float = std::stof( float_str );
                return std::to_string( number_float * conversion_factor );
            };

            // when cm must be used instead of meters
            if( use_cm_units )
                buff_str = convert_float_str( buff_str, 100.f );

            char buff[TEXT_BUFF_SIZE];
            memset( buff, 0, TEXT_BUFF_SIZE );
            strncpy( buff, buff_str.c_str(), TEXT_BUFF_SIZE - 1 );

            if( ImGui::InputText( id.c_str(),
                                  buff,
                                  TEXT_BUFF_SIZE,
                                  ImGuiInputTextFlags_EnterReturnsTrue ) )
            {
                if( use_cm_units )
                {
                    buff_str = convert_float_str( std::string( buff ), 0.01f );
                    memset( buff, 0, TEXT_BUFF_SIZE );
                    strncpy( buff, buff_str.c_str(), TEXT_BUFF_SIZE - 1 );
                }
                float new_value;
                if( ! rsutils::string::string_to_value< float >( buff, new_value ) )
                {
                    error_message = "Invalid float input!";
                }
                else if( new_value < range.min || new_value > range.max )
                {
                    float val = use_cm_units ? new_value * 100.f : new_value;
                    float min = use_cm_units ? range.min * 100.f : range.min;
                    float max = use_cm_units ? range.max * 100.f : range.max;

                    error_message = rsutils::string::from()
                                 << val << " is out of bounds [" << min << ", " << max << "]";
                }
                else
                {
                    // run when the value is valid and the enter key is pressed to submit the new value
                    set_option(opt, new_value);
                    if (invalidate_flag)
                        *invalidate_flag = true;
                    model.add_log( rsutils::string::from() << "Setting " << opt << " to " << value_as_string() );
                }
                edit_mode = false;
            }
            else if( use_cm_units )
            {
                buff_str = convert_float_str( buff_str, 0.01f );
                memset( buff, 0, TEXT_BUFF_SIZE );
                strncpy( buff, buff_str.c_str(), TEXT_BUFF_SIZE - 1 );
            }
            edit_value = buff;
        }
        else if( is_all_integers() )
        {
            // runs when changing a value with slider and not the textbox
            auto int_value = static_cast< int >( value_as_float() );

            if( RsImGui::SliderIntWithSteps( id.c_str(),
                                             &int_value,
                                             static_cast< int >( range.min ),
                                             static_cast< int >( range.max ),
                                             static_cast< int >( range.step ) ) )
            {
                // TODO: Round to step?
                slider_clicked = slider_selected( opt, static_cast< float >( int_value ), error_message, model );
                last_slider_hold_stopwatch.reset(); // While sliding the control, avoid other means of updating value
            }
            else
            {
                slider_clicked = slider_unselected( opt, static_cast< float >( int_value ), error_message, model );
            }
        }
        else
        {
            float tmp_value = value_as_float();
            float temp_value_displayed = tmp_value;
            float min_range_displayed = range.min;
            float max_range_displayed = range.max;

            // computing the number of decimal digits taken from the step options' property
            // this will then be used to format the displayed value
            auto num_of_decimal_digits = []( float f ) {
                float f_0 = std::fabs( f - (int)f );
                std::string s = std::to_string( f_0 );
                size_t cur_len = s.length();
                // removing trailing zeros
                while( cur_len > 3 && s[cur_len - 1] == '0' )
                    cur_len--;
                return cur_len - 2;
            };
            int num_of_decimal_digits_displayed = (int)num_of_decimal_digits( range.step );

            // displaying in cm instead of meters for D405
            if( use_cm_units )
            {
                temp_value_displayed *= 100.f;
                min_range_displayed *= 100.f;
                max_range_displayed *= 100.f;
                int updated_num_of_decimal_digits_displayed = num_of_decimal_digits_displayed - 2;
                if( updated_num_of_decimal_digits_displayed > 0 )
                    num_of_decimal_digits_displayed = updated_num_of_decimal_digits_displayed;
            }

            std::stringstream formatting_ss;
            formatting_ss << "%." << num_of_decimal_digits_displayed << "f";


            if( ImGui::SliderFloat( id.c_str(),
                                    &temp_value_displayed,
                                    min_range_displayed,
                                    max_range_displayed,
                                    formatting_ss.str().c_str() ) )
            {
                tmp_value = use_cm_units ? temp_value_displayed / 100.f : temp_value_displayed;
                auto loffset = std::abs( fmod( tmp_value, range.step ) );
                auto roffset = range.step - loffset;
                if( tmp_value >= 0 )
                    tmp_value = ( loffset < roffset ) ? tmp_value - loffset : tmp_value + roffset;
                else
                    tmp_value = ( loffset < roffset ) ? tmp_value + loffset : tmp_value - roffset;
                tmp_value = ( tmp_value < range.min ) ? range.min : tmp_value;
                tmp_value = ( tmp_value > range.max ) ? range.max : tmp_value;

                slider_clicked = slider_selected( opt, tmp_value, error_message, model );
            }
            else
            {
                slider_clicked = slider_unselected( opt, tmp_value, error_message, model );
            }
        }
    }
    catch( const error & e )
    {
        error_message = error_to_string( e );
    }
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();
    return slider_clicked;
}

bool option_model::is_checkbox() const
{
    return range.max == 1.0f && range.min == 0.0f && range.step == 1.0f;
}

bool option_model::draw_checkbox( notifications_model & model,
                                  std::string & error_message,
                                  const char * description )
{
    bool checkbox_was_clicked = false;

    bool bool_value = value_as_float() > 0.f;

    if( ImGui::Checkbox( label.c_str(), &bool_value ) )
    {
        checkbox_was_clicked = true;
        model.add_log( rsutils::string::from() << "Setting " << opt << " to " << ( bool_value ? "1.0" : "0.0" ) << " ("
                                               << ( bool_value ? "ON" : "OFF" ) << ")" );

        set_option( opt, bool_value ? 1.f : 0.f );
        if (invalidate_flag)
            *invalidate_flag = true;
    }
    if( ImGui::IsItemHovered() && description )
    {
        RsImGui::CustomTooltip( "%s", description );
    }
    return checkbox_was_clicked;
}

bool option_model::slider_selected( rs2_option opt,
                                    float value,
                                    std::string & /*error_message*/,
                                    notifications_model & model )
{
    check_opt( opt, __func__ );
    // Dispatch every UI tick: the dispatcher action coalesces (per-option
    // _latest_pending_value), and its try_sleep(200ms) enforces the FW-write
    // floor that used to live in this function. set_option_async is O(atomic-CAS)
    // when a job is already pending, so per-tick calls are cheap.
    set_option_async( value );
    if( invalidate_flag )
        *invalidate_flag = true;
    // Throttle the log line to ~5 Hz so the notifications panel stays readable
    // during a drag. The actual FW writes are throttled independently by the
    // dispatcher action's try_sleep(200ms).
    if( last_set_stopwatch.get_elapsed() >= std::chrono::milliseconds( 200 ) )
    {
        last_set_stopwatch.reset();
        model.add_log( rsutils::string::from() << "Setting " << opt << " to " << value );
    }
    return true;
}

bool option_model::slider_unselected( rs2_option opt,
                                      float /*value*/,
                                      std::string & /*error_message*/,
                                      notifications_model & /*model*/ )
{
    check_opt( opt, __func__ );
    // No-op. slider_selected dispatches on every tick (cheap, coalesced), so
    // the user's final position is already in _latest_pending_value and will
    // be picked up by the dispatcher's next action. No retry-on-release needed.
    return false;
}

bool option_model::draw_option(bool update_read_only_options,
    bool is_streaming,
    std::string& error_message, notifications_model& model)
{
    // Surface any pending async FW error (e.g., "exposure can't be changed while AE
    // is enabled" on certain D4xx variants). The async worker stashes the message
    // under _async_state->mutex; we swap it out here on the UI thread and write it
    // to the error_message out-param. The caller chain eventually feeds that to
    // viewer_model::popup_if_error which shows a centered modal — matching the
    // pre-PR synchronous flow's error UX. The popup deduplicates by message and
    // honors the user's "don't show this error again" preference, so we don't need
    // to throttle here: a continuous failing slider drag gets one modal until
    // dismissed, and dismissed-as-ignored errors fall back to the notifications
    // panel automatically.
    {
        std::string async_err;
        {
            std::lock_guard< std::mutex > lk( _async_state->mutex );
            async_err.swap( _async_state->last_error );
        }
        if( ! async_err.empty() )
        {
            try
            {
                error_message = rsutils::string::from()
                              << "Failed to set " << endpoint->get_option_name( opt )
                              << ": " << async_err;
            }
            catch( ... ) {}
        }
    }

    if (update_read_only_options)
    {
        update_supported(error_message);
        if (supported && is_streaming)
        {
            update_read_only_status(error_message);
            if (read_only)
            {
                update_all_fields(error_message, model);
            }
        }
    }
    if (custom_draw_method)
        return custom_draw_method(*this, error_message, model);
    else
        return draw(error_message, model);
}

void option_model::set_option(rs2_option opt, float req_value)
{
    check_opt( opt, __func__ );
    // All UI-thread option writes go through the subdevice dispatcher so the render
    // loop is never blocked on the FW round-trip (set_option is ~200 ms on UVC;
    // readback is another ~200 ms; and they serialize on the per-device USB lock
    // against options_watcher's 1 s poll cycle). The dispatcher's try_sleep(200ms)
    // after each action enforces a uniform FW-write floor across all entry points
    // (slider, checkbox, calibration) — no separate per-call rate-limit needed here.
    last_set_stopwatch.reset();
    set_option_async( req_value );
}

void option_model::check_opt( rs2_option opt, char const * caller ) const
{
    if( opt != this->opt )
        throw std::runtime_error( rsutils::string::from()
                                  << caller << " called on option_model bound to "
                                  << this->opt << " with mismatched opt=" << opt );
}

void option_model::set_option_async( float value )
{
    // Mask the stale cached `value` with the user's just-requested value until the
    // FW echo refreshes it, so draw_slider doesn't snap back to the old value next
    // frame. Write _user_request_value and the stopwatch BEFORE flipping the atomic
    // so any reader that observes the flag set also sees the matching value.
    _user_request_value = value;
    _user_request_stopwatch.reset();
    _has_user_request->store( true );
    // Tell the parent subdevice_model to back off its per-frame option polling for a
    // bit, otherwise the UI thread will issue a sync get_option_value() that serializes
    // on the per-device USB lock behind the dispatcher's in-flight write (and behind any
    // concurrent options_watcher poll cycle), reintroducing the visible UI freeze.
    if( dev )
        dev->last_user_set_stopwatch.reset();

    // Coalesce on the subdevice dispatcher: store the latest requested value, and
    // only enqueue a new action if one isn't already queued. The action clears the
    // pending flag BEFORE re-reading _latest_pending_value, so any post() that
    // arrives during the in-flight FW call gets to enqueue a fresh action for that
    // update. Result: between any two FW writes only the latest user value survives;
    // a 60-Hz slider drag produces ~5 Hz of FW writes (paced by the action's
    // try_sleep below), never queues stale values, and never blocks the UI thread.
    _latest_pending_value->store( value );
    if( _has_pending_job->exchange( true ) )
        return;  // an action is already queued; it will pick up the latest value

    if( ! dev )
        return;
    auto disp = dev->set_dispatcher();
    if( ! disp )
        return;

    // Capture shared_ptrs by value (NOT `this`) so the action is UAF-safe if
    // option_model is destroyed mid-FW-call. ~subdevice_model destroys the
    // dispatcher first (declared last), which stops/joins the worker before any
    // option_model is gone — but `endpoint` (sensor) outlives both via shared_ptr,
    // and the atomics outlive the action via these captured shared_ptrs.
    auto endpoint_copy    = endpoint;
    auto opt_copy         = opt;
    auto state            = _async_state;
    auto has_user_request = _has_user_request;
    auto pending          = _has_pending_job;
    auto latest           = _latest_pending_value;

    disp->invoke(
        [ endpoint_copy, opt_copy, state, has_user_request, pending, latest ](
            dispatcher::cancellable_timer c )
        {
            // Clear the "queued" flag FIRST so a concurrent post() during the FW
            // call can enqueue a follow-up action; then read back the latest value.
            pending->store( false );
            float v = latest->load();
            std::string err_msg;
            try
            {
                endpoint_copy->set_option( opt_copy, v );
            }
            catch( const rs2::error & e )
            {
                LOG_WARNING( "async set_option opt=" << opt_copy << " value=" << v
                                                     << " failed: " << e.what() );
                err_msg = e.what();
            }
            catch( const std::exception & e )
            {
                LOG_WARNING( "async set_option opt=" << opt_copy << " value=" << v
                                                     << " failed: " << e.what() );
                err_msg = e.what();
            }
            catch( ... )
            {
                LOG_WARNING( "async set_option opt=" << opt_copy << " value=" << v
                                                     << " failed with unknown exception" );
                err_msg = "unknown error";
            }
            if( ! err_msg.empty() )
            {
                {
                    std::lock_guard< std::mutex > lk( state->mutex );
                    state->last_error = err_msg;
                }
                // FW rejected the write: drop the user-request mask immediately so
                // the slider snaps back to the actual cached value within a frame
                // rather than waiting for the 2 s timeout.
                has_user_request->store( false );
            }
            // Bus-fairness floor between FW writes on this subdevice. The FW
            // round-trip itself already rate-limits the bus; this small extra idle
            // window (~20 Hz cap when FW is fast) keeps the video frame stream from
            // starving during sustained option-write traffic. 200 ms was too long
            // when one of the queued writes is for a slow option like exposure at
            // high values — subsequent writes (e.g. gain) waited behind it.
            // Returns early if the dispatcher is shutting down (device disconnect).
            c.try_sleep( std::chrono::milliseconds( 50 ) );
        } );
}

void option_model::set_option_sync( float value )
{
    // Synchronous variant for callers that need to verify the write took effect
    // (e.g., on_chip_calib_manager). Goes through the same dispatcher as async
    // writes so it can't race with concurrent UI option writes on the USB bus.
    // Blocks the calling thread until the action runs.
    if( ! dev )
        return;
    auto disp = dev->set_dispatcher();
    if( ! disp )
        return;
    disp->invoke_and_wait(
        [ this, value ]( dispatcher::cancellable_timer c )
        {
            endpoint->set_option( opt, value );
            c.try_sleep( std::chrono::milliseconds( 50 ) );
        },
        []() { return false; } );
}

void option_model::update_value( const rs2::option_value & updated_value, notifications_model & model )
{
    // After slider was dragged, don't update value from outside (usually on_options_changed callback)
    if( last_slider_hold_stopwatch.get_elapsed_ms() < 1000 )
        return;

    value = updated_value;
    _has_user_request->store( false );
}
