// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "composite-control-editor.h"
#include "device-model.h"
#include "textual-icons.h"
#include <realsense_imgui.h>
#include <rsutils/string/from.h>
#include <rsutils/string/string-utilities.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace rs2
{
    namespace
    {
        // Decimal digits needed to show one step of value/scale, e.g. step 10 over scale 1000 -> 2.
        int decimal_digits( int step, float scale )
        {
            float v = step / scale;
            int digits = 0;
            while( digits < 6 && std::fabs( v - std::round( v ) ) > 1e-6f )
            {
                v *= 10.f;
                ++digits;
            }
            return digits;
        }

        std::string format_value( int value, float scale, int digits )
        {
            if( scale == 1.f )
                return std::to_string( value );
            char buf[32];
            snprintf( buf, sizeof( buf ), "%.*f", digits, value / scale );
            return buf;
        }
    }

    void composite_control_editor_base::finalize( double delay_seconds )
    {
        _commit_deadline = ImGui::GetTime() + delay_seconds;
        _active_delay = delay_seconds;
        // A discrete edit's touch()+finalize() run in the SAME frame as the draw call that just set
        // this deadline - suppress the focus-loss shortcut for that one frame so it can't collapse it.
        _just_finalized = true;
    }

    bool composite_control_editor_base::try_get_progress( float & progress ) const
    {
        if( ! _dirty || _active_delay <= 0.0 )
            return false;
        double remaining = _commit_deadline - ImGui::GetTime();
        double frac_remaining = std::min( std::max( remaining / _active_delay, 0.0 ), 1.0 );
        progress = static_cast< float >( 1.0 - frac_remaining );
        return true;
    }

    void composite_control_editor_base::clear_dirty()
    {
        _dirty = false;
        _commit_deadline = std::numeric_limits< double >::max();
        if( _release_focus_on_commit )
            RsImGui::ClearItemFocus();
        _release_focus_on_commit = false;
    }

    void composite_control_editor_base::collapse_deadline_on_focus_loss( bool any_field_active_this_frame )
    {
        if( _dirty && ! any_field_active_this_frame && ImGui::IsAnyItemActive() && ! _just_finalized )
            _commit_deadline = ImGui::GetTime();
        _just_finalized = false;
    }

    void composite_control_editor_base::draw_pending_highlight( const ImVec2 & frame_min, const ImVec2 & frame_max ) const
    {
        float progress;
        if( ! try_get_progress( progress ) )
            return;
        // Clockwise from the top-left corner; the part already consumed by the countdown is not drawn.
        constexpr float thickness = 3.f;
        const float half = thickness * 0.5f;
        const ImVec2 corners[5] = { { frame_min.x + half, frame_min.y + half }, { frame_max.x - half, frame_min.y + half },
                                    { frame_max.x - half, frame_max.y - half }, { frame_min.x + half, frame_max.y - half },
                                    { frame_min.x + half, frame_min.y + half } };
        const float width = corners[1].x - corners[0].x, height = corners[2].y - corners[1].y;
        const float side[4] = { width, height, width, height };
        float skip = 2.f * ( width + height ) * progress;
        auto * draw_list = ImGui::GetWindowDrawList();
        for( int i = 0; i < 4; ++i )
        {
            if( skip >= side[i] )
            {
                skip -= side[i];
                continue;
            }
            const float t = skip / side[i];
            if( draw_list->_Path.Size == 0 )
                draw_list->PathLineTo( { corners[i].x + ( corners[i + 1].x - corners[i].x ) * t,
                                         corners[i].y + ( corners[i + 1].y - corners[i].y ) * t } );
            draw_list->PathLineTo( corners[i + 1] );
            skip = 0.f;
        }
        draw_list->PathStroke( ImGui::ColorConvertFloat4ToU32( regular_blue ), 0, thickness );
    }

    // Same layout as option_model::draw_slider's label row: name, "?" help at x=257, pencil at x=280.
    void composite_control_editor_base::draw_field_header( const char * name, const char * description, const char * id,
                                                           field_edit_state & edit, const std::string & value_text )
    {
        ImGui::Text( "%s:", name );

        ImGui::SameLine();
        ImGui::SetCursorPosX( 257.f + extra_width );
        ImGui::PushStyleColor( ImGuiCol_Text, grey );
        ImGui::PushStyleColor( ImGuiCol_TextSelectedBg, grey );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, { 1.f, 1.f, 1.f, 0.f } );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, { 1.f, 1.f, 1.f, 0.f } );
        ImGui::PushStyleColor( ImGuiCol_Button, { 1.f, 1.f, 1.f, 0.f } );
        std::string help_id = rsutils::string::from() << textual_icons::question_mark << "##" << id << "_help";
        ImGui::Button( help_id.c_str(), { 20, 20 } );
        ImGui::PopStyleColor( 5 );
        if( ImGui::IsItemHovered() && description )
            RsImGui::CustomTooltip( "%s", description );

        ImGui::SameLine();
        ImGui::SetCursorPosX( 280.f + extra_width );
        std::string edit_id = rsutils::string::from() << textual_icons::edit << "##" << id << "_edit";
        ImGui::PushStyleColor( ImGuiCol_Text, edit.edit_mode ? light_blue : light_grey );
        ImGui::PushStyleColor( ImGuiCol_TextSelectedBg, edit.edit_mode ? light_blue : light_grey );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, { 1.f, 1.f, 1.f, 0.f } );
        ImGui::PushStyleColor( ImGuiCol_Button, { 1.f, 1.f, 1.f, 0.f } );
        if( ImGui::Button( edit_id.c_str(), { 20, 20 } ) )
        {
            if( ! edit.edit_mode )
                edit.buf = value_text;
            edit.edit_mode = ! edit.edit_mode;
        }
        if( ImGui::IsItemHovered() )
            RsImGui::CustomTooltip( edit.edit_mode ? "Exit text-edit mode" : "Enter text-edit mode" );
        ImGui::PopStyleColor( 4 );
    }

    bool composite_control_editor_base::draw_scalar_field( const char * name, const char * description, const char * id,
                                                           int & value, int min_v, int max_v, int step, float scale,
                                                           field_edit_state & edit, std::string & error_message )
    {
        const int digits = decimal_digits( step, scale );
        draw_field_header( name, description, id, edit, format_value( value, scale, digits ) );

        std::string widget_id = rsutils::string::from() << "##" << id;
        ImGui::PushItemWidth( 295.f + extra_width - ImGui::GetCursorPosX() );
        ImGui::PushStyleColor( ImGuiCol_FrameBgHovered, black );
        ImGui::PushStyleColor( ImGuiCol_FrameBgActive, black );

        if( edit.edit_mode )
        {
            char buf[32] = {};
            strncpy( buf, edit.buf.c_str(), sizeof( buf ) - 1 );
            if( ImGui::InputText( widget_id.c_str(), buf, sizeof( buf ), ImGuiInputTextFlags_EnterReturnsTrue ) )
            {
                float new_value;
                float min_shown = min_v / scale, max_shown = max_v / scale;
                if( ! rsutils::string::string_to_value< float >( buf, new_value ) )
                    error_message = "Invalid float input!";
                else if( new_value < min_shown || new_value > max_shown )
                    error_message = rsutils::string::from() << new_value << " is out of bounds [" << min_shown << ", " << max_shown << "]";
                else
                {
                    value = (int)std::lround( new_value * scale );
                    touch();
                    finalize();
                }
                edit.edit_mode = false;
            }
            edit.buf = buf;
        }
        else
        {
            bool changed;
            if( scale == 1.f )
                changed = RsImGui::SliderIntWithSteps( widget_id.c_str(), &value, min_v, max_v, step );
            else
            {
                float shown = value / scale;
                std::string format = rsutils::string::from() << "%." << digits << "f";
                changed = ImGui::SliderFloat( widget_id.c_str(), &shown, min_v / scale, max_v / scale, format.c_str() );
                if( changed )
                    value = (int)std::lround( shown * scale );
            }
            if( changed || ImGui::IsItemActive() )
                touch();
            if( ImGui::IsItemDeactivatedAfterEdit() )
                finalize();
            // Each arrow step re-arms the countdown; the delay outlasts the key-repeat interval, so a
            // held key lands as a single SET once released.
            float nudged = (float)value;
            if( RsImGui::SliderArrowNudge( &nudged, (float)min_v, (float)max_v, (float)step ) )
            {
                value = (int)std::lround( nudged );
                touch();
                finalize();
                _release_focus_on_commit = true;
            }
        }

        ImGui::PopStyleColor( 2 );
        ImGui::PopItemWidth();
        return ImGui::IsItemActive();
    }

    // Same layout as option_model::draw_combobox: name, then the combo on the same line.
    bool composite_control_editor_base::draw_enum_field( const char * name, const char * description, const char * id,
                                                         const char * const labels[], int count, int & value, int value_offset )
    {
        std::string txt = rsutils::string::from() << name << ":";
        float combo_position_x = ImGui::GetCursorPosX() + ImGui::CalcTextSize( txt.c_str() ).x + 5;
        ImGui::Text( "%s", txt.c_str() );
        if( ImGui::IsItemHovered() && description )
            RsImGui::CustomTooltip( "%s", description );

        ImGui::SameLine();
        ImGui::SetCursorPosX( combo_position_x );
        ImGui::PushItemWidth( ImGui::GetContentRegionAvail().x - 25 );
        ImGui::PushStyleColor( ImGuiCol_TextSelectedBg, { 1, 1, 1, 1 } );
        int selected = std::min( std::max( value - value_offset, 0 ), count - 1 );
        std::string widget_id = rsutils::string::from() << "##" << id;
        if( RsImGui::CustomComboBox( widget_id.c_str(), &selected, labels, count ) )
        {
            value = selected + value_offset;
            touch();
            finalize();
        }
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
        return ImGui::IsItemActive();
    }
}
