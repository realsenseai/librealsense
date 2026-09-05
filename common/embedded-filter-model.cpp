// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include <rsutils/easylogging/easyloggingpp.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include "subdevice-model.h"
#include "embedded-filter-model.h"
#include "textual-icons.h"
#include "viewer.h"


namespace rs2
{
    namespace
    {
        // Ties the manual-entry pencil icon and the number it opens together visually - the same
        // color on both is what says "these two are one mode," not just proximity.
        const ImVec4 manual_edit_color( 1.0f, 0.65f, 0.0f, 1.0f );

        // DEBUG: every field read back from FW for RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP -
        // called only after a real GET, never an every-frame no-op re-read. LOG_DEBUG so this only
        // emits at DEBUG+ verbosity, not the default INFO level.
        void print_decimation_filter_dpp_config( const rs2_decimation_filter_dpp_config & v )
        {
            LOG_DEBUG( "[Decimation Filter DPP GET] version=" << (int)v.header.version
                       << " flags=" << (int)v.header.flags
                       << " ctl_id=0x" << std::hex << v.header.ctl_id << std::dec
                       << " param_count=" << (int)v.header.param_count
                       << " param_type=" << (int)v.header.param_type
                       << " enabled=" << v.enabled
                       << " magnitude=" << v.magnitude );
        }

        // No enum-valued fields to clamp here - every field is just range-bounded. Still zeroes
        // the reserved slots - "MUST be zero on SET" rule.
        void sanitize_decimation_filter_dpp_config( rs2_decimation_filter_dpp_config & v )
        {
            for( auto & r : v.reserved )
                r = 0;
        }

        // Same scheme as print_decimation_filter_dpp_config() above, for
        // RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP.
        void print_temporal_filter_dpp_config( const rs2_temporal_filter_dpp_config & v )
        {
            LOG_DEBUG( "[Temporal Filter DPP GET] version=" << (int)v.header.version
                       << " flags=" << (int)v.header.flags
                       << " ctl_id=0x" << std::hex << v.header.ctl_id << std::dec
                       << " param_count=" << (int)v.header.param_count
                       << " param_type=" << (int)v.header.param_type
                       << " enabled=" << v.enabled
                       << " smooth_alpha=" << v.smooth_alpha
                       << " smooth_delta=" << v.smooth_delta
                       << " persistency_index=" << v.persistency_index
                       << " reserved=[" << v.reserved[0] << "," << v.reserved[1] << ","
                       << v.reserved[2] << "," << v.reserved[3] << "]" );
        }

        // No enum-valued fields to clamp here (unlike sanitize_hdrd_control() below) - every field
        // is just range-bounded. Still zeroes the reserved slots - same "MUST be zero on SET" rule.
        void sanitize_temporal_filter_dpp_config( rs2_temporal_filter_dpp_config & v )
        {
            for( auto & r : v.reserved )
                r = 0;
        }

        // Same scheme as print_decimation_filter_dpp_config() above, for
        // RS2_COMPOSITE_OPTION_HDRD_CONTROL.
        void print_hdrd_control( const rs2_hdrd_control & v )
        {
            LOG_DEBUG( "[Improved Close Range GET] version=" << (int)v.header.version
                       << " flags=" << (int)v.header.flags
                       << " ctl_id=0x" << std::hex << v.header.ctl_id << std::dec
                       << " param_count=" << (int)v.header.param_count
                       << " param_type=" << (int)v.header.param_type
                       << " enable=" << v.enable
                       << " filter_type=" << v.filter_type
                       << " downscale_ratio=" << v.downscale_ratio
                       << " shift_mode=" << v.shift_mode
                       << " shift_pixels=" << v.shift_pixels
                       << " threshold_mode=" << v.threshold_mode
                       << " threshold_mm=" << v.threshold_mm
                       << " reserved=[" << v.reserved[0] << "]" );
        }

        // A real device's firmware may still speak the pre-design-review wire layout, so a field
        // can land outside its enum's legal range. Called after every raw device read, before any
        // branching/display code uses it, so the stored value and what's shown stay consistent.
        void sanitize_hdrd_control( rs2_hdrd_control & v )
        {
            v.filter_type = std::min( std::max( v.filter_type, 0 ), 1 );
            v.downscale_ratio = std::min( std::max( v.downscale_ratio, 1 ), 2 );
            v.shift_mode = std::min( std::max( v.shift_mode, 0 ), 2 );
            v.threshold_mode = std::min( std::max( v.threshold_mode, 0 ), 2 );
            // Header doc: "MUST be zero on SET". A real device may hand back a non-zero byte here
            // on GET (pre-design-review firmware, or simply unused memory) - without this, every
            // later auto-commit/enable-toggle would echo that non-zero value straight back on SET.
            v.reserved[0] = 0;
        }
    }

    embedded_filter_model::embedded_filter_model(
        subdevice_model* owner,
        const rs2_embedded_filter_type& type,
        std::shared_ptr<rs2::embedded_filter> filter,
        viewer_model& viewer,
        std::string& error_message)
        : _embedded_filter(filter), _viewer(viewer), _destructing(false)
    {
        _name = rs2_embedded_filter_type_to_string(type);

        std::stringstream ss;
        ss << "##" << ((owner) ? owner->dev.get_info(RS2_CAMERA_INFO_NAME) : _name)
            << "/" << ((owner) ? (*owner->s).get_info(RS2_CAMERA_INFO_NAME) : "_")
            << "/" << (long long)this;

        // following method also updates the data member "_enabled"
        populate_options(ss.str().c_str(), owner, owner ? &owner->_options_invalidated : nullptr, error_message);
    }


    embedded_filter_model::~embedded_filter_model()
    {
        _destructing.store(true);
        try
        {
            _embedded_filter->on_options_changed([](const options_list& list) {});
        }
        catch (...)
        {
        }
    }

    void embedded_filter_model::draw_options( viewer_model & viewer,
                                               bool update_read_only_options,
                                               bool is_streaming,
                                               std::string & error_message )
    {
        for (auto& id_and_model : _options_id_to_model)
        {
            if( id_and_model.first == RS2_OPTION_EMBEDDED_FILTER_ENABLED )
                continue;

            id_and_model.second.draw_option( update_read_only_options, is_streaming, error_message, *viewer.not_model );
        }

        // Composite options have no generic per-field editing UI - Decimation, Temporal Filter DPP
        // and HDRD each get a hardcoded editor below; everything else shows read-only metadata.
        for( auto id : _composite_option_ids )
        {
            try
            {
                if( id == RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP )
                {
                    draw_decimation_filter_dpp_control_editor( error_message );
                    continue;
                }
                if( id == RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP )
                {
                    draw_temporal_filter_dpp_control_editor( error_message );
                    continue;
                }
                if( id == RS2_COMPOSITE_OPTION_HDRD_CONTROL )
                {
                    // No always-visible description line here - draw_hdrd_control_editor shows
                    // it as a tooltip on hovering the framed control instead, to save vertical
                    // space in this narrow side panel.
                    draw_hdrd_control_editor( error_message );
                    continue;
                }

                // TextWrapped, not TextDisabled - a one-line description reliably clips in this
                // narrow panel. Description fetched BEFORE the push - if it throws mid-argument
                // with the push already done, PopStyleColor() would never run.
                auto description = _embedded_filter->get_composite_option_description( id );
                ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled] );
                ImGui::TextWrapped( "%s", description );
                ImGui::PopStyleColor();

                auto bytes = _embedded_filter->get_composite_option( id );
                ImGui::TextDisabled( "  (%zu bytes, composite option - no generic editor yet)", bytes.size() );
            }
            catch( const std::exception& e )
            {
                error_message = e.what();
            }
        }
    }

    // ==== RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP editor =====
    // The InputText half of draw_decimation_filter_dpp_manual_editable_field()'s two editing
    // modes: a narrow, centered box seeded with the current value; Enter parses, clamps, commits,
    // then flips back to slider mode. touch() fires every frame this box has focus so a countdown
    // can't fire mid-edit.
    bool embedded_filter_model::draw_decimation_filter_dpp_manual_input( const char * id, int & value, int min_v, int max_v,
                                                                          bool & edit_mode, std::string & edit_buf )
    {
        char buf[32] = {};
        strncpy( buf, edit_buf.c_str(), sizeof( buf ) - 1 );

        float avail_width = ImGui::GetContentRegionAvail().x;
        float input_width = ImGui::CalcTextSize( "000000" ).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX( ImGui::GetCursorPosX() + std::max( 0.0f, ( avail_width - input_width ) * 0.5f ) );
        ImGui::PushItemWidth( input_width );
        ImGui::PushStyleColor( ImGuiCol_Text, manual_edit_color );
        std::string input_id = rsutils::string::from() << "##" << id << "_input";
        bool submitted = ImGui::InputText( input_id.c_str(), buf, sizeof( buf ),
                                            ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue );
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        if( submitted )
        {
            char * end = nullptr;
            long parsed = std::strtol( buf, &end, 10 );
            if( end != buf )
            {
                value = (int)std::min( std::max( parsed, (long)min_v ), (long)max_v );
                _decimation_filter_dpp_editor.touch();
                _decimation_filter_dpp_editor.finalize( _decimation_filter_dpp_editor.numeric_commit_delay );
            }
            edit_mode = false;
        }
        else
            edit_buf = buf;

        if( ImGui::IsItemActive() )
            _decimation_filter_dpp_editor.touch();
        return ImGui::IsItemActive();
    }

    // The slider half of draw_decimation_filter_dpp_manual_editable_field()'s two editing modes.
    // ImGui sliders don't nudge via arrow keys on their own, so it's implemented by hand: one
    // focused arrow press is one complete, discrete edit (touch()+finalize() together).
    bool embedded_filter_model::draw_decimation_filter_dpp_slider_with_arrows( const char * id, int & value, int min_v, int max_v )
    {
        std::string slider_id = rsutils::string::from() << "##" << id;
        if( ImGui::SliderInt( slider_id.c_str(), &value, min_v, max_v ) )
            _decimation_filter_dpp_editor.touch();
        if( ImGui::IsItemActive() )
            _decimation_filter_dpp_editor.touch();
        if( ImGui::IsItemDeactivatedAfterEdit() )
            _decimation_filter_dpp_editor.finalize( _decimation_filter_dpp_editor.numeric_commit_delay );
        else if( ImGui::IsItemFocused() && ! ImGui::IsItemActive() )
        {
            if( ImGui::IsKeyPressed( ImGuiKey_RightArrow ) )
            {
                value = std::min( value + 1, max_v );
                _decimation_filter_dpp_editor.touch();
                _decimation_filter_dpp_editor.finalize( _decimation_filter_dpp_editor.fast_commit_delay );
            }
            else if( ImGui::IsKeyPressed( ImGuiKey_LeftArrow ) )
            {
                value = std::max( value - 1, min_v );
                _decimation_filter_dpp_editor.touch();
                _decimation_filter_dpp_editor.finalize( _decimation_filter_dpp_editor.fast_commit_delay );
            }
        }
        return ImGui::IsItemActive();
    }

    // One "label + pencil-toggle + (slider OR manual InputText)" field for the single magnitude
    // field. The pencil toggles between draw_decimation_filter_dpp_manual_input()/
    // draw_decimation_filter_dpp_slider_with_arrows() above.
    bool embedded_filter_model::draw_decimation_filter_dpp_manual_editable_field( const char * label, const char * id, int & value,
                                                                                   int min_v, int max_v, bool & edit_mode,
                                                                                   std::string & edit_buf )
    {
        ImGui::Text( "%s", label );
        ImGui::SameLine();
        {
            std::string edit_id = rsutils::string::from() << textual_icons::edit << "##" << id << "_edit";
            const bool color_pushed = edit_mode;
            if( color_pushed )
                ImGui::PushStyleColor( ImGuiCol_Text, manual_edit_color );
            if( ImGui::SmallButton( edit_id.c_str() ) )
            {
                if( ! edit_mode )
                    edit_buf = std::to_string( value );
                edit_mode = ! edit_mode;
            }
            if( color_pushed )
                ImGui::PopStyleColor();
            if( ImGui::IsItemHovered() )
                ImGui::SetTooltip( edit_mode ? "Back to slider" : "Type an exact value" );
        }

        return edit_mode ? draw_decimation_filter_dpp_manual_input( id, value, min_v, max_v, edit_mode, edit_buf )
                          : draw_decimation_filter_dpp_slider_with_arrows( id, value, min_v, max_v );
    }

    // Draws the single magnitude field. Range comes live from the device
    // (get_composite_option_range_as() below), not hardcoded - FW currently reports a fixed
    // [2,2] (or wider on newer builds), but this editor doesn't assume that stays fixed.
    bool embedded_filter_model::draw_decimation_filter_dpp_fields()
    {
        int magnitude_min = _decimation_filter_dpp_editor.value.magnitude;
        int magnitude_max = _decimation_filter_dpp_editor.value.magnitude;
        try
        {
            auto range = _embedded_filter->get_composite_option_range_as< rs2_decimation_filter_dpp_range >(
                RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP );
            magnitude_min = range.min.magnitude;
            magnitude_max = range.max.magnitude;
        }
        catch( const std::exception & )
        {
            // Best-effort range only - fall back to a degenerate [current,current] slider rather
            // than disrupting the rest of the editor over a failed range query.
        }

        bool any_field_active = draw_decimation_filter_dpp_manual_editable_field( "Magnitude:", "decimation_filter_dpp_magnitude",
            _decimation_filter_dpp_editor.value.magnitude, magnitude_min, magnitude_max,
            _decimation_filter_dpp_magnitude_edit_mode, _decimation_filter_dpp_magnitude_edit_buf );
        if( ImGui::IsItemHovered() )
            ImGui::SetTooltip( "Downscale factor - currently fixed by firmware." );

        return any_field_active;
    }

    // Reset to Default corner overlay - see draw_hdrd_reset_to_default_overlay() below, which
    // this mirrors exactly (same collapse/reveal/fade behavior), against this struct's own
    // range/editor.
    bool embedded_filter_model::draw_decimation_filter_dpp_reset_to_default_overlay( rs2_composite_option_id id,
                                                                                      std::string & error_message,
                                                                                      float frame_max_x,
                                                                                      float frame_max_y )
    {
        bool any_active = false;
        ImVec2 frame_max( frame_max_x, frame_max_y );
        ImVec2 saved_cursor = ImGui::GetCursorScreenPos();

        ImVec2 button_size = ImGui::CalcTextSize( "Reset to Default" );
        button_size.x += ImGui::GetStyle().FramePadding.x * 2.0f;
        button_size.y += ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 button_pos( frame_max.x - button_size.x - 4.0f, frame_max.y - button_size.y - 4.0f );

        constexpr float reveal_margin = 24.0f;
        ImVec2 mouse = ImGui::GetIO().MousePos;
        bool nearby = mouse.x >= button_pos.x - reveal_margin && mouse.x <= frame_max.x + reveal_margin
                   && mouse.y >= button_pos.y - reveal_margin && mouse.y <= frame_max.y + reveal_margin;

        if( nearby )
        {
            ImGui::SetCursorScreenPos( button_pos );
            if( ImGui::Button( "Reset to Default##decimation_filter_dpp" ) )
            {
                try
                {
                    auto range = _embedded_filter->get_composite_option_range_as< rs2_decimation_filter_dpp_range >( id );
                    _decimation_filter_dpp_editor.value = range.def;
                    sanitize_decimation_filter_dpp_config( _decimation_filter_dpp_editor.value );
                    _decimation_filter_dpp_editor.touch();
                    _decimation_filter_dpp_editor.finalize();
                }
                catch( const std::exception & e )
                {
                    error_message = e.what();
                }
            }
            any_active = ImGui::IsItemActive();
            if( ImGui::IsItemHovered() )
                ImGui::SetTooltip( "Restore all fields to the firmware-reported default values" );
        }
        else
        {
            ImVec2 marker_size = ImGui::CalcTextSize( "..." );
            ImGui::SetCursorScreenPos( ImVec2( frame_max.x - marker_size.x - 8.0f, frame_max.y - marker_size.y - 6.0f ) );
            ImGui::Text( "..." );
        }

        ImGui::SetCursorScreenPos( saved_cursor );
        return any_active;
    }

    void embedded_filter_model::draw_decimation_filter_dpp_control_editor( std::string & error_message )
    {
        const auto id = RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP;

        if( ! _decimation_filter_dpp_editor.ensure_initialized( _embedded_filter, id, error_message, print_decimation_filter_dpp_config ) )
            return;
        sanitize_decimation_filter_dpp_config( _decimation_filter_dpp_editor.value );

        ImGui::Indent( -5.f );
        ImGui::Dummy( ImVec2( 0, 2 ) );

        float frame_left = ImGui::GetCursorScreenPos().x - 4.0f;
        float frame_top = ImGui::GetCursorScreenPos().y - 4.0f;
        float frame_width = ImGui::GetContentRegionAvail().x;

        constexpr float slider_right_inset = 8.0f;
        ImGui::PushItemWidth( frame_width - slider_right_inset );

        // No separate Enable checkbox - the row header's own toggle drives
        // rs2_decimation_filter_dpp_config::enabled directly, same as the others. Unlike them,
        // the device itself also rejects a SET once Depth/IR is streaming (read-only while
        // active) - end_frame_and_maybe_commit()'s own error_message surfaces that rejection.
        const bool dim_while_disabled = ! _enabled;
        if( dim_while_disabled )
            ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.35f );

        bool any_field_active = draw_decimation_filter_dpp_fields();

        ImGui::PopItemWidth();

        ImGui::Dummy( ImVec2( 0, 2 ) );

        ImVec2 reset_button_size = ImGui::CalcTextSize( "Reset to Default" );
        reset_button_size.y += ImGui::GetStyle().FramePadding.y * 2.0f;
        ImGui::Dummy( ImVec2( 0, ( reset_button_size.y + 6.0f ) * 0.5f ) );

        float frame_bottom = ImGui::GetCursorScreenPos().y;
        ImVec2 frame_min( frame_left, frame_top );
        ImVec2 frame_max( frame_left + frame_width, frame_bottom );

        any_field_active |= draw_decimation_filter_dpp_reset_to_default_overlay( id, error_message, frame_max.x, frame_max.y );

        _decimation_filter_dpp_editor.end_frame_and_maybe_commit( _embedded_filter, id, error_message, frame_min, frame_max, any_field_active,
            [this]( rs2_decimation_filter_dpp_config & v )
            {
                v.enabled = 1;
                _enabled = true;
            } );

        if( dim_while_disabled )
            ImGui::PopStyleVar();

        ImGui::Unindent( -5.f );
    }

    // ==== RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP editor - same scheme as Decimation above =====
    // The InputText half - see draw_decimation_filter_dpp_manual_input() above, which this
    // mirrors field for field.
    bool embedded_filter_model::draw_temporal_filter_dpp_manual_input( const char * id, int & value, int min_v, int max_v,
                                                                        bool & edit_mode, std::string & edit_buf )
    {
        char buf[32] = {};
        strncpy( buf, edit_buf.c_str(), sizeof( buf ) - 1 );

        float avail_width = ImGui::GetContentRegionAvail().x;
        float input_width = ImGui::CalcTextSize( "000000" ).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX( ImGui::GetCursorPosX() + std::max( 0.0f, ( avail_width - input_width ) * 0.5f ) );
        ImGui::PushItemWidth( input_width );
        ImGui::PushStyleColor( ImGuiCol_Text, manual_edit_color );
        std::string input_id = rsutils::string::from() << "##" << id << "_input";
        bool submitted = ImGui::InputText( input_id.c_str(), buf, sizeof( buf ),
                                            ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue );
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        if( submitted )
        {
            char * end = nullptr;
            long parsed = std::strtol( buf, &end, 10 );
            if( end != buf )
            {
                value = (int)std::min( std::max( parsed, (long)min_v ), (long)max_v );
                _temporal_filter_dpp_editor.touch();
                _temporal_filter_dpp_editor.finalize( _temporal_filter_dpp_editor.numeric_commit_delay );
            }
            edit_mode = false;
        }
        else
            edit_buf = buf;

        if( ImGui::IsItemActive() )
            _temporal_filter_dpp_editor.touch();
        return ImGui::IsItemActive();
    }

    // The slider half - see draw_decimation_filter_dpp_slider_with_arrows() above, which this
    // mirrors field for field.
    bool embedded_filter_model::draw_temporal_filter_dpp_slider_with_arrows( const char * id, int & value, int min_v, int max_v )
    {
        std::string slider_id = rsutils::string::from() << "##" << id;
        if( ImGui::SliderInt( slider_id.c_str(), &value, min_v, max_v ) )
            _temporal_filter_dpp_editor.touch();
        if( ImGui::IsItemActive() )
            _temporal_filter_dpp_editor.touch();
        if( ImGui::IsItemDeactivatedAfterEdit() )
            _temporal_filter_dpp_editor.finalize( _temporal_filter_dpp_editor.numeric_commit_delay );
        else if( ImGui::IsItemFocused() && ! ImGui::IsItemActive() )
        {
            if( ImGui::IsKeyPressed( ImGuiKey_RightArrow ) )
            {
                value = std::min( value + 1, max_v );
                _temporal_filter_dpp_editor.touch();
                _temporal_filter_dpp_editor.finalize( _temporal_filter_dpp_editor.fast_commit_delay );
            }
            else if( ImGui::IsKeyPressed( ImGuiKey_LeftArrow ) )
            {
                value = std::max( value - 1, min_v );
                _temporal_filter_dpp_editor.touch();
                _temporal_filter_dpp_editor.finalize( _temporal_filter_dpp_editor.fast_commit_delay );
            }
        }
        return ImGui::IsItemActive();
    }

    // One "label + pencil-toggle + (slider OR manual InputText)" field - see
    // draw_decimation_filter_dpp_manual_editable_field() above, which this mirrors exactly. Every
    // field here uses this pattern since none of them are enum-valued.
    bool embedded_filter_model::draw_temporal_filter_dpp_manual_editable_field( const char * label, const char * id, int & value,
                                                                                 int min_v, int max_v, bool & edit_mode,
                                                                                 std::string & edit_buf )
    {
        ImGui::Text( "%s", label );
        ImGui::SameLine();
        {
            std::string edit_id = rsutils::string::from() << textual_icons::edit << "##" << id << "_edit";
            const bool color_pushed = edit_mode;
            if( color_pushed )
                ImGui::PushStyleColor( ImGuiCol_Text, manual_edit_color );
            if( ImGui::SmallButton( edit_id.c_str() ) )
            {
                if( ! edit_mode )
                    edit_buf = std::to_string( value );
                edit_mode = ! edit_mode;
            }
            if( color_pushed )
                ImGui::PopStyleColor();
            if( ImGui::IsItemHovered() )
                ImGui::SetTooltip( edit_mode ? "Back to slider" : "Type an exact value" );
        }

        return edit_mode ? draw_temporal_filter_dpp_manual_input( id, value, min_v, max_v, edit_mode, edit_buf )
                          : draw_temporal_filter_dpp_slider_with_arrows( id, value, min_v, max_v );
    }

    // Draws all 3 fields, in order. No BeginDisabled()-greyed conditional relevance here: every
    // field is meaningful all the time.
    bool embedded_filter_model::draw_temporal_filter_dpp_fields()
    {
        bool any_field_active = draw_temporal_filter_dpp_manual_editable_field( "Smooth Alpha:", "temporal_filter_dpp_smooth_alpha",
            _temporal_filter_dpp_editor.value.smooth_alpha, 0, 1000,
            _temporal_filter_dpp_smooth_alpha_edit_mode, _temporal_filter_dpp_smooth_alpha_edit_buf );
        if( ImGui::IsItemHovered() )
            ImGui::SetTooltip( "Normalized [0,1] scaled into [0,1000] - e.g. 400 = 0.4." );

        any_field_active |= draw_temporal_filter_dpp_manual_editable_field( "Smooth Delta:", "temporal_filter_dpp_smooth_delta",
            _temporal_filter_dpp_editor.value.smooth_delta, 1, 100,
            _temporal_filter_dpp_smooth_delta_edit_mode, _temporal_filter_dpp_smooth_delta_edit_buf );

        any_field_active |= draw_temporal_filter_dpp_manual_editable_field( "Persistency Index:", "temporal_filter_dpp_persistency_index",
            _temporal_filter_dpp_editor.value.persistency_index, 0, 8,
            _temporal_filter_dpp_persistency_index_edit_mode, _temporal_filter_dpp_persistency_index_edit_buf );

        return any_field_active;
    }

    // Reset to Default corner overlay - see draw_decimation_filter_dpp_reset_to_default_overlay()
    // above, which this mirrors exactly (same collapse/reveal/fade behavior), against this
    // struct's own range/editor.
    bool embedded_filter_model::draw_temporal_filter_dpp_reset_to_default_overlay( rs2_composite_option_id id,
                                                                                    std::string & error_message,
                                                                                    float frame_max_x,
                                                                                    float frame_max_y )
    {
        bool any_active = false;
        ImVec2 frame_max( frame_max_x, frame_max_y );
        ImVec2 saved_cursor = ImGui::GetCursorScreenPos();

        ImVec2 button_size = ImGui::CalcTextSize( "Reset to Default" );
        button_size.x += ImGui::GetStyle().FramePadding.x * 2.0f;
        button_size.y += ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 button_pos( frame_max.x - button_size.x - 4.0f, frame_max.y - button_size.y - 4.0f );

        constexpr float reveal_margin = 24.0f;
        ImVec2 mouse = ImGui::GetIO().MousePos;
        bool nearby = mouse.x >= button_pos.x - reveal_margin && mouse.x <= frame_max.x + reveal_margin
                   && mouse.y >= button_pos.y - reveal_margin && mouse.y <= frame_max.y + reveal_margin;

        if( nearby )
        {
            ImGui::SetCursorScreenPos( button_pos );
            if( ImGui::Button( "Reset to Default##temporal_filter_dpp" ) )
            {
                try
                {
                    auto range = _embedded_filter->get_composite_option_range_as< rs2_temporal_filter_dpp_range >( id );
                    _temporal_filter_dpp_editor.value = range.def;
                    sanitize_temporal_filter_dpp_config( _temporal_filter_dpp_editor.value );
                    _temporal_filter_dpp_editor.touch();
                    _temporal_filter_dpp_editor.finalize();
                }
                catch( const std::exception & e )
                {
                    error_message = e.what();
                }
            }
            any_active = ImGui::IsItemActive();
            if( ImGui::IsItemHovered() )
                ImGui::SetTooltip( "Restore all fields to the firmware-reported default values" );
        }
        else
        {
            ImVec2 marker_size = ImGui::CalcTextSize( "..." );
            ImGui::SetCursorScreenPos( ImVec2( frame_max.x - marker_size.x - 8.0f, frame_max.y - marker_size.y - 6.0f ) );
            ImGui::Text( "..." );
        }

        ImGui::SetCursorScreenPos( saved_cursor );
        return any_active;
    }

    void embedded_filter_model::draw_temporal_filter_dpp_control_editor( std::string & error_message )
    {
        const auto id = RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP;

        if( ! _temporal_filter_dpp_editor.ensure_initialized( _embedded_filter, id, error_message, print_temporal_filter_dpp_config ) )
            return;
        sanitize_temporal_filter_dpp_config( _temporal_filter_dpp_editor.value );

        ImGui::Indent( -5.f );
        ImGui::Dummy( ImVec2( 0, 2 ) );

        float frame_left = ImGui::GetCursorScreenPos().x - 4.0f;
        float frame_top = ImGui::GetCursorScreenPos().y - 4.0f;
        float frame_width = ImGui::GetContentRegionAvail().x;

        constexpr float slider_right_inset = 8.0f;
        ImGui::PushItemWidth( frame_width - slider_right_inset );

        // No separate Enable checkbox - the row header's own toggle drives
        // rs2_temporal_filter_dpp_config::enabled directly, same as the others.
        const bool dim_while_disabled = ! _enabled;
        if( dim_while_disabled )
            ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.35f );

        bool any_field_active = draw_temporal_filter_dpp_fields();

        ImGui::PopItemWidth();

        ImGui::Dummy( ImVec2( 0, 2 ) );

        ImVec2 reset_button_size = ImGui::CalcTextSize( "Reset to Default" );
        reset_button_size.y += ImGui::GetStyle().FramePadding.y * 2.0f;
        ImGui::Dummy( ImVec2( 0, ( reset_button_size.y + 6.0f ) * 0.5f ) );

        float frame_bottom = ImGui::GetCursorScreenPos().y;
        ImVec2 frame_min( frame_left, frame_top );
        ImVec2 frame_max( frame_left + frame_width, frame_bottom );

        any_field_active |= draw_temporal_filter_dpp_reset_to_default_overlay( id, error_message, frame_max.x, frame_max.y );

        _temporal_filter_dpp_editor.end_frame_and_maybe_commit( _embedded_filter, id, error_message, frame_min, frame_max, any_field_active,
            [this]( rs2_temporal_filter_dpp_config & v )
            {
                v.enabled = 1;
                _enabled = true;
            } );

        if( dim_while_disabled )
            ImGui::PopStyleVar();

        ImGui::Unindent( -5.f );
    }

    // ==== RS2_COMPOSITE_OPTION_HDRD_CONTROL editor =====
    // "Slider enum" widget - an ImGui::SliderInt showing the enum's own name instead of a number,
    // the idiom from Dear ImGui's own Sliders demo. Not a dropdown: one drag/click, no popup, and
    // it shares the drag/arrow-key interaction of every other slider (draw_hdrd_slider_with_arrows()).
    bool embedded_filter_model::draw_hdrd_enum_field( const char * label,
                                                                       const char * id,
                                                                       const char * const labels[],
                                                                       int count,
                                                                       int & value,
                                                                       int value_offset )
    {
        // Width comes from the ambient PushItemWidth() the caller (draw_hdrd_control_editor())
        // wraps the whole field block in - shared with every other slider in the box, not pushed here,
        // so they all end at the exact same x regardless of field type.
        ImGui::Text( "%s", label );
        int selected = value - value_offset;
        const char * current_label = ( selected >= 0 && selected < count ) ? labels[selected] : "?";
        if( ImGui::SliderInt( id, &selected, 0, count - 1, current_label ) )
        {
            value = selected + value_offset;
            _hdrd_editor.touch();
        }
        if( ImGui::IsItemActive() )
            _hdrd_editor.touch();
        // Same fast_commit_delay as the arrow-key nudge below - releasing this slider IS the
        // discrete choice, so it gets near-immediate turnaround, not the longer numeric default.
        if( ImGui::IsItemDeactivatedAfterEdit() )
            _hdrd_editor.finalize( _hdrd_editor.fast_commit_delay );
        else if( ImGui::IsItemFocused() && ! ImGui::IsItemActive() )
        {
            if( ImGui::IsKeyPressed( ImGuiKey_RightArrow ) && selected < count - 1 )
            {
                value = selected + 1 + value_offset;
                _hdrd_editor.touch();
                _hdrd_editor.finalize( _hdrd_editor.fast_commit_delay );   // arrow-key nudge - fast turnaround
            }
            else if( ImGui::IsKeyPressed( ImGuiKey_LeftArrow ) && selected > 0 )
            {
                value = selected - 1 + value_offset;
                _hdrd_editor.touch();
                _hdrd_editor.finalize( _hdrd_editor.fast_commit_delay );   // arrow-key nudge - fast turnaround
            }
        }
        return ImGui::IsItemActive();
    }

    bool embedded_filter_model::draw_hdrd_filter_type_field()
    {
        static const char * const labels[] = { "Downscale", "Lookup Shift" };
        return draw_hdrd_enum_field( "Filter Type:", "##hdrd_filter_type",
            labels, 2, _hdrd_editor.value.filter_type, 0 );
    }

    bool embedded_filter_model::draw_hdrd_downscale_ratio_field()
    {
        // Wire values are 1 (x2) and 2 (x4), not 0-based like the other enum fields - value_offset
        // converts to and from a 0-based slider index rather than changing the documented wire values.
        static const char * const labels[] = { "x2", "x4" };
        return draw_hdrd_enum_field( "Downscale Ratio:", "##hdrd_downscale_ratio",
            labels, 2, _hdrd_editor.value.downscale_ratio, 1 );
    }

    bool embedded_filter_model::draw_hdrd_shift_mode_field()
    {
        // Lookup Shift: pick a fixed preset, or Manual - which draw_hdrd_control_editor()
        // reveals via a separate draw_hdrd_manual_editable_field() call for Shift Pixels.
        static const char * const labels[] = { "Shift 126px", "Shift 64px", "Manual" };
        return draw_hdrd_enum_field( "Shift Mode:", "##hdrd_shift_mode",
            labels, 3, _hdrd_editor.value.shift_mode, 0 );
    }

    // The InputText half of draw_hdrd_manual_editable_field()'s two editing modes: a narrow,
    // centered box seeded with the current value; Enter parses, clamps, commits, then flips back
    // to slider mode. touch() fires every frame this box has focus so a countdown can't fire mid-edit.
    bool embedded_filter_model::draw_hdrd_manual_input( const char * id, int & value, int min_v, int max_v,
                                                          bool & edit_mode, std::string & edit_buf )
    {
        char buf[32] = {};
        strncpy( buf, edit_buf.c_str(), sizeof( buf ) - 1 );

        // Narrow, centered input box (not full-width/left-aligned like the slider it replaces)
        // plus the same highlight color as the pencil icon beside it, so the box visually reads
        // as "a small typed value," distinct from the wide slider it stands in for while active.
        float avail_width = ImGui::GetContentRegionAvail().x;
        float input_width = ImGui::CalcTextSize( "000000" ).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX( ImGui::GetCursorPosX() + std::max( 0.0f, ( avail_width - input_width ) * 0.5f ) );
        ImGui::PushItemWidth( input_width );
        ImGui::PushStyleColor( ImGuiCol_Text, manual_edit_color );
        std::string input_id = rsutils::string::from() << "##" << id << "_input";
        bool submitted = ImGui::InputText( input_id.c_str(), buf, sizeof( buf ),
                                            ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue );
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        if( submitted )
        {
            char * end = nullptr;
            long parsed = std::strtol( buf, &end, 10 );
            if( end != buf )
            {
                value = (int)std::min( std::max( parsed, (long)min_v ), (long)max_v );
                _hdrd_editor.touch();
                // Shorter than the enum fields' default commit_delay - a typed-and-submitted
                // number, like a slider drag release, doesn't carry the same "did I mean to pick
                // that?" risk a discrete choice does.
                _hdrd_editor.finalize( _hdrd_editor.numeric_commit_delay );
            }
            edit_mode = false;
        }
        else
            edit_buf = buf;

        if( ImGui::IsItemActive() )
            _hdrd_editor.touch();
        return ImGui::IsItemActive();
    }

    // The slider half of draw_hdrd_manual_editable_field()'s two editing modes. ImGui sliders
    // don't nudge via arrow keys on their own, so it's implemented by hand: one focused arrow
    // press is one complete, discrete edit (touch()+finalize() together).
    bool embedded_filter_model::draw_hdrd_slider_with_arrows( const char * id, int & value, int min_v, int max_v )
    {
        std::string slider_id = rsutils::string::from() << "##" << id;
        if( ImGui::SliderInt( slider_id.c_str(), &value, min_v, max_v ) )
            _hdrd_editor.touch();
        if( ImGui::IsItemActive() )
            _hdrd_editor.touch();
        // Shorter than the enum fields' default commit_delay - dragging a plain value slider
        // doesn't carry the same "did I mean to pick that?" risk a discrete choice does.
        if( ImGui::IsItemDeactivatedAfterEdit() )
            _hdrd_editor.finalize( _hdrd_editor.numeric_commit_delay );
        else if( ImGui::IsItemFocused() && ! ImGui::IsItemActive() )
        {
            if( ImGui::IsKeyPressed( ImGuiKey_RightArrow ) )
            {
                value = std::min( value + 1, max_v );
                _hdrd_editor.touch();
                _hdrd_editor.finalize( _hdrd_editor.fast_commit_delay );   // arrow-key nudge - fast turnaround
            }
            else if( ImGui::IsKeyPressed( ImGuiKey_LeftArrow ) )
            {
                value = std::max( value - 1, min_v );
                _hdrd_editor.touch();
                _hdrd_editor.finalize( _hdrd_editor.fast_commit_delay );   // arrow-key nudge - fast turnaround
            }
        }
        return ImGui::IsItemActive();
    }

    // One "label + pencil-toggle + (slider OR manual InputText)" field - shared by Disparity
    // Shift and Threshold (mm), which otherwise repeated this pattern twice. The pencil toggles
    // between draw_hdrd_manual_input()/draw_hdrd_slider_with_arrows() above.
    bool embedded_filter_model::draw_hdrd_manual_editable_field( const char * label, const char * id, int & value,
                                                                   int min_v, int max_v, bool & edit_mode,
                                                                   std::string & edit_buf )
    {
        ImGui::Text( "%s", label );
        ImGui::SameLine();
        {
            std::string edit_id = rsutils::string::from() << textual_icons::edit << "##" << id << "_edit";
            // Captured once, before the button can flip edit_mode - push and pop must agree on
            // the SAME decision, or a click toggling edit_mode this frame leaves them unbalanced
            // (an unmatched push, or a pop with nothing pushed).
            const bool color_pushed = edit_mode;
            if( color_pushed )
                ImGui::PushStyleColor( ImGuiCol_Text, manual_edit_color );
            if( ImGui::SmallButton( edit_id.c_str() ) )
            {
                if( ! edit_mode )
                    edit_buf = std::to_string( value );
                edit_mode = ! edit_mode;
            }
            if( color_pushed )
                ImGui::PopStyleColor();
            if( ImGui::IsItemHovered() )
                ImGui::SetTooltip( edit_mode ? "Back to slider" : "Type an exact value" );
        }

        return edit_mode ? draw_hdrd_manual_input( id, value, min_v, max_v, edit_mode, edit_buf )
                          : draw_hdrd_slider_with_arrows( id, value, min_v, max_v );
    }

    bool embedded_filter_model::draw_hdrd_threshold_mode_field()
    {
        // Zero range / MinZ (firmware-computed) / Manual. Per the design review, the FW-computed
        // MinZ value itself is NOT surfaced to the user at this stage - "MinZ (computed)" reveals
        // no readback field at all, unlike "Manual" (see draw_hdrd_manual_editable_field()).
        static const char * const labels[] = { "Zero range", "MinZ (computed)", "Manual" };
        bool active = draw_hdrd_enum_field( "Threshold Mode:", "##hdrd_threshold_mode",
            labels, 3, _hdrd_editor.value.threshold_mode, 0 );
        if( ImGui::IsItemHovered() )
            ImGui::SetTooltip( "Zero range: fill only originally-empty depth pixels.\n"
                                "MinZ (computed): firmware picks the threshold for the active resolution.\n"
                                "Manual: use the threshold value below." );
        return active;
    }

    // Reset to Default starts hidden behind a small "..." marker in the box's bottom-right
    // corner - a rarely-used action that shouldn't compete for attention. Hovering within
    // reveal_margin swaps it for the real button, drawn as an absolute overlay.
    bool embedded_filter_model::draw_hdrd_reset_to_default_overlay( rs2_composite_option_id id,
                                                                      std::string & error_message,
                                                                      float frame_max_x,
                                                                      float frame_max_y )
    {
        bool any_active = false;
        ImVec2 frame_max( frame_max_x, frame_max_y );
        ImVec2 saved_cursor = ImGui::GetCursorScreenPos();

        ImVec2 button_size = ImGui::CalcTextSize( "Reset to Default" );
        button_size.x += ImGui::GetStyle().FramePadding.x * 2.0f;
        button_size.y += ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 button_pos( frame_max.x - button_size.x - 4.0f, frame_max.y - button_size.y - 4.0f );

        constexpr float reveal_margin = 24.0f;
        ImVec2 mouse = ImGui::GetIO().MousePos;
        bool nearby = mouse.x >= button_pos.x - reveal_margin && mouse.x <= frame_max.x + reveal_margin
                   && mouse.y >= button_pos.y - reveal_margin && mouse.y <= frame_max.y + reveal_margin;

        if( nearby )
        {
            // Goes through the same touch()/finalize() pipeline as every other field edit, so it
            // gets the fade animation and undo grace window for free. range.def is the FULL
            // FW-reported default; before_commit still forces enable back on, same as any edit.
            ImGui::SetCursorScreenPos( button_pos );
            if( ImGui::Button( "Reset to Default##hdrd" ) )
            {
                try
                {
                    auto range = _embedded_filter->get_composite_option_range_as< rs2_hdrd_control_range >( id );
                    _hdrd_editor.value = range.def;
                    sanitize_hdrd_control( _hdrd_editor.value );
                    _hdrd_editor.touch();
                    _hdrd_editor.finalize();
                }
                catch( const std::exception & e )
                {
                    error_message = e.what();
                }
            }
            any_active = ImGui::IsItemActive();
            if( ImGui::IsItemHovered() )
                ImGui::SetTooltip( "Restore all fields to the firmware-reported default values" );
        }
        else
        {
            ImVec2 marker_size = ImGui::CalcTextSize( "..." );
            ImGui::SetCursorScreenPos( ImVec2( frame_max.x - marker_size.x - 8.0f, frame_max.y - marker_size.y - 6.0f ) );
            // Full-brightness text, not TextDisabled's muted grey - the marker is easy to miss
            // otherwise, and unlike the fields above it, it isn't meant to read as unavailable.
            ImGui::Text( "..." );
        }

        ImGui::SetCursorScreenPos( saved_cursor );
        return any_active;
    }

    // any_field_active distinguishes "nothing of ours is active" from "focus left the group" for
    // the caller's debounce countdown. |= (not ||=): each call draws real widgets and must run
    // every frame. Irrelevant fields stay visible but BeginDisabled()-greyed, not hidden.
    bool embedded_filter_model::draw_hdrd_fields()
    {
        bool any_field_active = draw_hdrd_filter_type_field();

        const bool downscale_relevant = ( _hdrd_editor.value.filter_type == 0 );
        if( ! downscale_relevant )
            ImGui::BeginDisabled();
        any_field_active |= draw_hdrd_downscale_ratio_field();
        if( ! downscale_relevant )
            ImGui::EndDisabled();

        const bool shift_relevant = ( _hdrd_editor.value.filter_type == 1 );
        if( ! shift_relevant )
            ImGui::BeginDisabled();
        any_field_active |= draw_hdrd_shift_mode_field();
        if( ! shift_relevant )
            ImGui::EndDisabled();

        const bool shift_pixels_relevant = shift_relevant && ( _hdrd_editor.value.shift_mode == 2 );
        if( ! shift_pixels_relevant )
            ImGui::BeginDisabled();
        any_field_active |= draw_hdrd_manual_editable_field( "Shift Pixels:", "hdrd_shift",
            _hdrd_editor.value.shift_pixels, 0, 256, _hdrd_shift_edit_mode, _hdrd_shift_edit_buf );
        if( ! shift_pixels_relevant )
            ImGui::EndDisabled();

        any_field_active |= draw_hdrd_threshold_mode_field();

        const bool threshold_relevant = ( _hdrd_editor.value.threshold_mode == 2 );
        if( ! threshold_relevant )
            ImGui::BeginDisabled();
        any_field_active |= draw_hdrd_manual_editable_field( "Threshold (mm):", "hdrd_threshold",
            _hdrd_editor.value.threshold_mm, 0, 65535, _hdrd_threshold_edit_mode, _hdrd_threshold_edit_buf );
        if( ! threshold_relevant )
            ImGui::EndDisabled();

        return any_field_active;
    }

    void embedded_filter_model::draw_hdrd_control_editor( std::string & error_message )
    {
        const auto id = RS2_COMPOSITE_OPTION_HDRD_CONTROL;

        if( ! _hdrd_editor.ensure_initialized( _embedded_filter, id, error_message, print_hdrd_control ) )
            return;
        sanitize_hdrd_control( _hdrd_editor.value );

        // Minimal indent so widget text doesn't sit flush on the frame border. Indent(w) ADDS w
        // (negative shifts left); the matching Unindent below MUST pass the same -5.f, since
        // Unindent SUBTRACTS its argument (Unindent(-5.f) is what actually cancels Indent(-5.f)).
        ImGui::Indent( -5.f );
        ImGui::Dummy( ImVec2( 0, 2 ) );

        float frame_left = ImGui::GetCursorScreenPos().x - 4.0f;
        float frame_top = ImGui::GetCursorScreenPos().y - 4.0f;
        float frame_width = ImGui::GetContentRegionAvail().x;

        // Every slider in the box shares this one pushed width so they all end at the same x. A
        // few pixels narrower than the frame so they finish just inside its right edge.
        constexpr float slider_right_inset = 8.0f;
        ImGui::PushItemWidth( frame_width - slider_right_inset );

        // No separate Enable checkbox - the row header's own toggle drives rs2_hdrd_control::enable
        // directly. Grey the box via global alpha (not BeginDisabled) so fields stay clickable -
        // editing while off forces enable back on at commit time below.
        const bool dim_while_disabled = ! _enabled;
        if( dim_while_disabled )
            ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.35f );

        bool any_field_active = draw_hdrd_fields();

        ImGui::PopItemWidth();

        ImGui::Dummy( ImVec2( 0, 2 ) );

        // Reserve a blank row below the last field for the Reset-to-Default corner overlay - just
        // enough for the collapsed "..." marker, not the taller expanded-button hover state.
        ImVec2 reset_button_size = ImGui::CalcTextSize( "Reset to Default" );
        reset_button_size.y += ImGui::GetStyle().FramePadding.y * 2.0f;
        ImGui::Dummy( ImVec2( 0, ( reset_button_size.y + 6.0f ) * 0.5f ) );

        float frame_bottom = ImGui::GetCursorScreenPos().y;
        ImVec2 frame_min( frame_left, frame_top );
        ImVec2 frame_max( frame_left + frame_width, frame_bottom );

        any_field_active |= draw_hdrd_reset_to_default_overlay( id, error_message, frame_max.x, frame_max.y );

        // Draws the dirty-state fade/border and sends the whole struct once the debounce timer
        // lapses. before_commit forces enable back on: with no Enable checkbox here, editing
        // e.g. Threshold alone would otherwise send with enable unchanged (often off).
        _hdrd_editor.end_frame_and_maybe_commit( _embedded_filter, id, error_message, frame_min, frame_max, any_field_active,
            [this]( rs2_hdrd_control & v )
            {
                v.enable = 1;
                _enabled = true;
            } );

        if( dim_while_disabled )
            ImGui::PopStyleVar();

        ImGui::Unindent( -5.f );   // undo Indent(-5.f) above, exactly - see the comment there
    }

    void embedded_filter_model::embedded_filter_enable_disable(bool actual, std::string * error_message)
    {
        // Composite-only embedded filters register no RS2_OPTION_EMBEDDED_FILTER_ENABLED scalar
        // option - route the toggle through the composite option's own `enable` field instead,
        // read-modify-write so the other fields go back as last reported, not zero-initialized.
        // The device itself also rejects this SET while Depth/IR is streaming for Decimation -
        // that rejection surfaces via the caught exception below like any other, no
        // special-casing needed here.
        if( _embedded_filter->supports_composite_option( RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP ) )
        {
            try
            {
                // Only re-read if we don't already have a live-tracked value - once we do, it
                // already mirrors every write made, so a fresh GET is a needless second XU
                // transaction that roughly doubles the toggle's response time.
                if( ! _decimation_filter_dpp_editor.initialized )
                {
                    _decimation_filter_dpp_editor.value = _embedded_filter->get_composite_option_as< rs2_decimation_filter_dpp_config >(
                        RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP );
                    sanitize_decimation_filter_dpp_config( _decimation_filter_dpp_editor.value );
                    print_decimation_filter_dpp_config( _decimation_filter_dpp_editor.value );
                    _decimation_filter_dpp_editor.initialized = true;
                }
                _decimation_filter_dpp_editor.value.enabled = actual ? 1 : 0;
                _embedded_filter->set_composite_option_from( RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP, _decimation_filter_dpp_editor.value );
                _enabled = actual;
            }
            catch( const std::exception & e )
            {
                // Leave _enabled as it was - the toggle stays in its last known-good state
                // rather than claiming a change happened when the device rejected it - but still
                // report why, or the toggle just appears to silently do nothing.
                if( error_message )
                    *error_message = e.what();
            }
            return;
        }

        // Same scheme as Decimation above, for RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP.
        if( _embedded_filter->supports_composite_option( RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP ) )
        {
            try
            {
                if( ! _temporal_filter_dpp_editor.initialized )
                {
                    _temporal_filter_dpp_editor.value = _embedded_filter->get_composite_option_as< rs2_temporal_filter_dpp_config >(
                        RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP );
                    sanitize_temporal_filter_dpp_config( _temporal_filter_dpp_editor.value );
                    print_temporal_filter_dpp_config( _temporal_filter_dpp_editor.value );
                    _temporal_filter_dpp_editor.initialized = true;
                }
                _temporal_filter_dpp_editor.value.enabled = actual ? 1 : 0;
                _embedded_filter->set_composite_option_from( RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP, _temporal_filter_dpp_editor.value );
                _enabled = actual;
            }
            catch( const std::exception & e )
            {
                if( error_message )
                    *error_message = e.what();
            }
            return;
        }

        // Same scheme as Decimation above, for RS2_COMPOSITE_OPTION_HDRD_CONTROL.
        if( _embedded_filter->supports_composite_option( RS2_COMPOSITE_OPTION_HDRD_CONTROL ) )
        {
            try
            {
                if( ! _hdrd_editor.initialized )
                {
                    _hdrd_editor.value = _embedded_filter->get_composite_option_as< rs2_hdrd_control >(
                        RS2_COMPOSITE_OPTION_HDRD_CONTROL );
                    sanitize_hdrd_control( _hdrd_editor.value );
                    print_hdrd_control( _hdrd_editor.value );
                    _hdrd_editor.initialized = true;
                }
                _hdrd_editor.value.enable = actual ? 1 : 0;
                _embedded_filter->set_composite_option_from( RS2_COMPOSITE_OPTION_HDRD_CONTROL, _hdrd_editor.value );
                _enabled = actual;
            }
            catch( const std::exception & e )
            {
                if( error_message )
                    *error_message = e.what();
            }
            return;
        }

        if( ! _embedded_filter->supports( RS2_OPTION_EMBEDDED_FILTER_ENABLED ) )
            return;
        _embedded_filter->set_option(RS2_OPTION_EMBEDDED_FILTER_ENABLED, actual ? 1.0f : 0.0f);
        _enabled = _embedded_filter->get_option(RS2_OPTION_EMBEDDED_FILTER_ENABLED);
    }

    void embedded_filter_model::populate_scalar_options( const std::string & opt_base_label,
                                                          subdevice_model * model,
                                                          std::string & error_message )
    {
        // Regular (scalar) options - own registry, own loop. DDS-based embedded filters still
        // register RS2_OPTION_EMBEDDED_FILTER_ENABLED here, the primary source of _enabled.
        for (option_value option : _embedded_filter->get_supported_option_values())
        {
            // Build the model first and insert only on success: an option whose range cannot be read
            // throws, and map::operator[] would leave a default-constructed (null-endpoint) entry
            // behind. Isolate per option so one bad control does not drop the rest.
            try
            {
                auto om = create_option_model( option,
                                               opt_base_label,
                                               model,
                                               _embedded_filter,
                                               model ? &model->_options_invalidated : nullptr,
                                               error_message );
                _options_id_to_model[option->id] = std::move( om );
            }
            catch( const std::exception & e )
            {
                if( _viewer.not_model )
                    _viewer.not_model->add_log( e.what(), RS2_LOG_SEVERITY_WARN );
            }
        }
        if( _embedded_filter->supports( RS2_OPTION_EMBEDDED_FILTER_ENABLED ) )
            _enabled = _embedded_filter->get_option(RS2_OPTION_EMBEDDED_FILTER_ENABLED);
    }

    void embedded_filter_model::populate_composite_options( std::string & error_message )
    {
        // Composite options are a completely separate registry from scalar rs2_option (see
        // rs_composite_option.h), enumerated and primed in its own loop rather than folded into
        // populate_scalar_options() above.
        _composite_option_ids = _embedded_filter->get_supported_composite_options();
        for( auto id : _composite_option_ids )
        {
            // No generic per-composite-option editor exists yet (see draw_options()) - Decimation,
            // Temporal Filter DPP and Improved Close Range are the three hardcoded cases, and the
            // only ones with an `enable`/`enabled` field to prime here.
            if( ! _embedded_filter->supports_composite_option( id ) )
                continue;

            if( id == RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP )
            {
                if( _decimation_filter_dpp_editor.ensure_initialized( _embedded_filter, id, error_message, print_decimation_filter_dpp_config ) )
                {
                    sanitize_decimation_filter_dpp_config( _decimation_filter_dpp_editor.value );
                    if( ! _embedded_filter->supports( RS2_OPTION_EMBEDDED_FILTER_ENABLED ) )
                        _enabled = _decimation_filter_dpp_editor.value.enabled != 0;
                }
            }
            else if( id == RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP )
            {
                if( _temporal_filter_dpp_editor.ensure_initialized( _embedded_filter, id, error_message, print_temporal_filter_dpp_config ) )
                {
                    sanitize_temporal_filter_dpp_config( _temporal_filter_dpp_editor.value );
                    if( ! _embedded_filter->supports( RS2_OPTION_EMBEDDED_FILTER_ENABLED ) )
                        _enabled = _temporal_filter_dpp_editor.value.enabled != 0;
                }
            }
            else if( id == RS2_COMPOSITE_OPTION_HDRD_CONTROL )
            {
                if( _hdrd_editor.ensure_initialized( _embedded_filter, id, error_message, print_hdrd_control ) )
                {
                    sanitize_hdrd_control( _hdrd_editor.value );
                    // Composite state is the FALLBACK source of _enabled, not an override - a DDS
                    // filter that already set it from the scalar option in populate_scalar_options()
                    // keeps that value.
                    if( ! _embedded_filter->supports( RS2_OPTION_EMBEDDED_FILTER_ENABLED ) )
                        _enabled = _hdrd_editor.value.enable != 0;
                }
            }
        }
    }

    void embedded_filter_model::register_options_changed_callback()
    {
        try
        {
            _embedded_filter->on_options_changed([this](const options_list& list)
                {
                    for (auto changed_option : list)
                    {
                        auto it = _options_id_to_model.find(changed_option->id);
                        // Callback runs in different context, checking _options_id_to_model still valid
                        if (it != _options_id_to_model.end() && !_destructing)
                        {
                            it->second.update_value(changed_option, *_viewer.not_model);
                            if (it->first == RS2_OPTION_EMBEDDED_FILTER_ENABLED)
                            {
                                // rs2_option_value is a union over as_float/as_integer. For a FLOAT
                                // option, only as_float is initialized - reading as_integer would
                                // pick up the uninitialized upper 4 bytes (int64_t vs float).
                                if (changed_option->is_valid)
                                    _enabled = (changed_option->type == RS2_OPTION_TYPE_FLOAT)
                                             ? (changed_option->as_float != 0.0f)
                                             : (changed_option->as_integer != 0);
                            }
                        }
                    }
                });
        }
        catch (const std::exception& e)
        {
            if (_viewer.not_model)
                _viewer.not_model->add_log(e.what(), RS2_LOG_SEVERITY_WARN);
        }
    }

    void embedded_filter_model::populate_options(const std::string& opt_base_label,
        subdevice_model* model,
        bool* options_invalidated,
        std::string& error_message)
    {
        populate_scalar_options( opt_base_label, model, error_message );
        populate_composite_options( error_message );
        register_options_changed_callback();
    }
}
