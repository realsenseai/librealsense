// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/rs.hpp>
#include <imgui.h>

#include <functional>
#include <limits>
#include <memory>
#include <string>

namespace rs2
{
    // Per-field text-edit toggle state, same role as option_model::edit_mode/edit_value.
    struct field_edit_state
    {
        bool edit_mode = false;
        std::string buf;
    };

    // Debounce bookkeeping plus the field widgets shared by every composite-option editor: touch()
    // while a field is being changed, finalize() once the edit is done, and the whole struct is sent
    // by the derived editor once the countdown lapses quietly. The widgets copy option_model's
    // slider/combobox look so a composite row is indistinguishable from a scalar option row.
    class composite_control_editor_base
    {
    public:
        static constexpr double commit_delay = 1.5;   // seconds of quiet before auto-sending, for every kind of edit

        // Call while a field is actively being changed (every tick of a slider drag). Flags the group
        // dirty and parks the deadline at +infinity so nothing commits mid-edit.
        void touch()
        {
            _dirty = true;
            _commit_deadline = std::numeric_limits< double >::max();
            _release_focus_on_commit = false;   // a new interaction supersedes a pending arrow-edit focus drop
        }

        // Call once a field's edit is finalized (slider released, value submitted, combo picked).
        // A zero delay commits on this frame, without the countdown.
        void finalize( double delay_seconds = commit_delay );

        // progress: 0 = just touched, 1 = about to commit. False when nothing is pending.
        bool try_get_progress( float & progress ) const;

        // How much wider than the default the control pane is; the rows widen by this much, like option_model's
        float extra_width = 0.f;

        // Label row + "?" help + pencil + slider (or InputText in edit mode). value is the wire integer;
        // scale > 1 shows it as value/scale on a float slider (e.g. 1000 -> 0.000..1.000).
        bool draw_scalar_field( const char * name, const char * description, const char * id,
                                int & value, int min_v, int max_v, int step, float scale,
                                field_edit_state & edit, std::string & error_message );

        // Label + combobox of labels; value = selected index + value_offset.
        bool draw_enum_field( const char * name, const char * description, const char * id,
                              const char * const labels[], int count, int & value, int value_offset = 0 );
        template< int N >
        bool draw_enum_field( const char * name, const char * description, const char * id,
                              const char * const ( &labels )[N], int & value, int value_offset = 0 )
        {
            return draw_enum_field( name, description, id, labels, N, value, value_offset );
        }

    protected:
        // Blue border around the group that is erased clockwise as the countdown runs out - visible
        // only while a commit is pending.
        void draw_pending_highlight( const ImVec2 & frame_min, const ImVec2 & frame_max ) const;

        // If focus left the group entirely (not just the normal gap between fields), finish the
        // countdown now instead of making the user wait - skipped on finalize()'s own frame so a fresh
        // deadline can survive.
        void collapse_deadline_on_focus_loss( bool any_field_active_this_frame );

        bool commit_due() const { return _dirty && ImGui::GetTime() >= _commit_deadline; }
        void clear_dirty();

    private:
        void draw_field_header( const char * name, const char * description, const char * id,
                                field_edit_state & edit, const std::string & value_text );

        bool _dirty = false;
        double _commit_deadline = std::numeric_limits< double >::max();
        double _active_delay = commit_delay;   // the delay finalize() last actually used
        bool _just_finalized = false;
        bool _release_focus_on_commit = false;   // an arrow-key edit is pending; drop the slider's focus once sent
    };

    // Debounced auto-commit editor for a composite option's struct T.
    template< typename T >
    class composite_control_editor : public composite_control_editor_base
    {
    public:
        T value{};
        bool initialized = false;

        // Seeds `value` from a GET the first time this is called; a no-op afterward. Returns whether
        // `value` is safe to use. `on_read`, if set, runs on the freshly-read value.
        bool ensure_initialized( const std::shared_ptr< rs2::embedded_filter > & filter,
                                 rs2_composite_option_id id,
                                 std::string & error_message,
                                 const std::function< void( const T & ) > & on_read = nullptr )
        {
            if( initialized )
                return true;
            try
            {
                value = filter->get_composite_option_as< T >( id );
                initialized = true;
                if( on_read )
                    on_read( value );
            }
            catch( const std::exception & e )
            {
                error_message = e.what();
            }
            return initialized;
        }

        // Right-aligned "Reset to Default" button; on click adopts the FW-reported default and sends it
        // right away.
        template< typename RangeT >
        bool draw_reset_to_default( const std::shared_ptr< rs2::embedded_filter > & filter,
                                    rs2_composite_option_id id,
                                    std::string & error_message,
                                    const std::function< void( T & ) > & sanitize )
        {
            const char * label = "Reset to Default";
            float width = ImGui::CalcTextSize( label ).x + ImGui::GetStyle().FramePadding.x * 2.f;
            ImGui::SetCursorPosX( 295.f - width );
            std::string button_id = std::string( label ) + "##" + std::to_string( (int)id );
            if( ImGui::Button( button_id.c_str() ) )
            {
                try
                {
                    value = filter->get_composite_option_range_as< RangeT >( id ).def;
                    sanitize( value );
                    touch();
                    finalize( 0.0 );
                }
                catch( const std::exception & e )
                {
                    error_message = e.what();
                }
            }
            if( ImGui::IsItemHovered() )
                ImGui::SetTooltip( "Restore all fields to the firmware-reported default values" );
            return ImGui::IsItemActive();
        }

        // Draws the pending-commit highlight for [frame_min, frame_max] and sends `value` once the
        // debounce timer lapses - or, while `hold_commit`, on the first frame after it clears.
        void end_frame_and_maybe_commit( const std::shared_ptr< rs2::embedded_filter > & filter,
                                         rs2_composite_option_id id,
                                         std::string & error_message,
                                         const ImVec2 & frame_min,
                                         const ImVec2 & frame_max,
                                         bool any_field_active_this_frame,
                                         bool hold_commit,
                                         const std::function< void( T & ) > & before_commit = nullptr )
        {
            draw_pending_highlight( frame_min, frame_max );
            collapse_deadline_on_focus_loss( any_field_active_this_frame );
            if( hold_commit || ! commit_due() )
                return;
            try
            {
                if( before_commit )
                    before_commit( value );
                filter->set_composite_option_from( id, value );
            }
            catch( const std::exception & e )
            {
                error_message = e.what();
                initialized = false;   // re-read what the device kept instead of showing the rejected value
            }
            clear_dirty();
        }
    };
}
