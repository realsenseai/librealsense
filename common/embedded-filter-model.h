// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/rs.hpp>
#include <librealsense2/h/rs_decimation_filter_dpp.h>
#include <librealsense2/h/rs_temporal_filter_dpp.h>
#include <librealsense2/h/rs_hdrd_control.h>
#include "composite-control-editor.h"
#include <functional>
#include <string>
#include <vector>


namespace rs2
{
    class subdevice_model;
    class option_model;
    class viewer_model;

    class embedded_filter_model
    {
    public:
        embedded_filter_model( subdevice_model* owner,
            const rs2_embedded_filter_type& type,
            std::shared_ptr<rs2::embedded_filter> filter,
            viewer_model& viewer,
            std::string& error_message);

        virtual ~embedded_filter_model();

        const std::string& get_name() const { return _name; }

        void populate_options( const std::string& opt_base_label,
            subdevice_model* model,
            bool* options_invalidated,
            std::string& error_message );

        void draw_options( viewer_model & viewer,
                           bool update_read_only_options,
                           bool is_streaming,
                           std::string & error_message );

        // Hardcoded editor for RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP specifically. There is
        // no generic per-field composite-option editor (would need per-struct schema knowledge
        // generic view code doesn't have) - same special-casing app code is expected to do. A
        // single manual-editable field (magnitude is currently a fixed [2,2] FW range, read live
        // rather than hardcoded, so a future FW widening it needs no viewer change).
        void draw_decimation_filter_dpp_control_editor( std::string & error_message );

        // Same scheme as draw_decimation_filter_dpp_control_editor() above, for
        // RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP - a second, independent editor. Every field is
        // a plain range-bounded number, so no BeginDisabled()-greyed relevance to track.
        void draw_temporal_filter_dpp_control_editor( std::string & error_message );

        // Same scheme as draw_decimation_filter_dpp_control_editor() above, for
        // RS2_COMPOSITE_OPTION_HDRD_CONTROL - a third, independent editor.
        void draw_hdrd_control_editor( std::string & error_message );

        std::shared_ptr<rs2::embedded_filter> get_filter() { return _embedded_filter; }

        // error_message, if non-null, receives the reason when the device rejects the change
        // (e.g. some composite controls cannot be toggled while the sensor is streaming) - without
        // it the toggle would otherwise appear to silently do nothing.
        void enable( bool e = true, std::string * error_message = nullptr )
        {
            embedded_filter_enable_disable( e, error_message );
        }
        bool is_enabled() const { return _enabled; }

        bool _is_visible = true;

        // Optional predicate; null means always available. When false the enable toggle is
        // grayed out. Set by the owner for filters with runtime constraints (e.g. close range,
        // depth-only, must be off while color streams).
        std::function<bool()> available_predicate;
        // Optional message shown when the toggle is unavailable; empty = none.
        std::string unavailable_tooltip;

        bool is_available() const { return !available_predicate || available_predicate(); }

        // Mirrors _decimation_filter_dpp_editor's/_temporal_filter_dpp_editor's/_hdrd_editor's own
        // pending-auto-commit state so the row header's enable toggle (device-model.cpp) can echo
        // the same "about to send" fade. Only one editor is ever initialized per filter, so
        // checking all three is simplest.
        bool has_pending_composite_commit( float & progress ) const
        {
            if( _decimation_filter_dpp_editor.try_get_progress( progress ) )
                return true;
            if( _temporal_filter_dpp_editor.try_get_progress( progress ) )
                return true;
            return _hdrd_editor.try_get_progress( progress );
        }

        void embedded_filter_enable_disable(bool actual, std::string * error_message = nullptr);

    private:
        // Helpers used only by draw_decimation_filter_dpp_control_editor() - same scheme as the
        // HDRD helpers below, applied to this struct's single manual-editable field.
        bool draw_decimation_filter_dpp_manual_editable_field( const char * label,
                                                                const char * id,
                                                                int & value,
                                                                int min_v,
                                                                int max_v,
                                                                bool & edit_mode,
                                                                std::string & edit_buf );
        bool draw_decimation_filter_dpp_manual_input( const char * id, int & value, int min_v, int max_v,
                                                       bool & edit_mode, std::string & edit_buf );
        bool draw_decimation_filter_dpp_slider_with_arrows( const char * id, int & value, int min_v, int max_v );
        bool draw_decimation_filter_dpp_fields();
        bool draw_decimation_filter_dpp_reset_to_default_overlay( rs2_composite_option_id id,
                                                                   std::string & error_message,
                                                                   float frame_max_x,
                                                                   float frame_max_y );

        // Helpers used only by draw_temporal_filter_dpp_control_editor() - same scheme as the
        // HDRD helpers below, applied to this struct's fields. No enum-field/BeginDisabled
        // machinery here - every field is relevant all the time.
        bool draw_temporal_filter_dpp_manual_editable_field( const char * label,
                                                              const char * id,
                                                              int & value,
                                                              int min_v,
                                                              int max_v,
                                                              bool & edit_mode,
                                                              std::string & edit_buf );
        bool draw_temporal_filter_dpp_manual_input( const char * id, int & value, int min_v, int max_v,
                                                     bool & edit_mode, std::string & edit_buf );
        bool draw_temporal_filter_dpp_slider_with_arrows( const char * id, int & value, int min_v, int max_v );
        // Draws all 3 fields in a row - the "what to draw" concern, kept separate from
        // draw_temporal_filter_dpp_control_editor()'s framing/dim/commit orchestration.
        bool draw_temporal_filter_dpp_fields();
        bool draw_temporal_filter_dpp_reset_to_default_overlay( rs2_composite_option_id id,
                                                                 std::string & error_message,
                                                                 float frame_max_x,
                                                                 float frame_max_y );

        // Helpers used only by draw_hdrd_control_editor() - each renders one field/element and
        // returns whether the user is actively interacting with it this frame. frame_max is
        // separate x/y floats rather than ImVec2 so this header need not pull in imgui.h.
        //
        // Shared body for the four enum fields below - an ImGui::SliderInt showing the enum's
        // name instead of the raw integer ("slider enum" idiom from Dear ImGui's demo). value_offset
        // converts wire values to the slider's 0-based index (only downscale_ratio isn't already 0-based).
        bool draw_hdrd_enum_field( const char * label,
                                                    const char * id,
                                                    const char * const labels[],
                                                    int count,
                                                    int & value,
                                                    int value_offset );
        bool draw_hdrd_filter_type_field();
        bool draw_hdrd_downscale_ratio_field();
        bool draw_hdrd_shift_mode_field();
        bool draw_hdrd_threshold_mode_field();
        // Draws all 6 fields, each wrapped in BeginDisabled()/EndDisabled() when the current
        // filter_type/shift_mode selection makes it irrelevant - the "what to draw" concern,
        // kept separate from draw_hdrd_control_editor()'s framing/dim/commit orchestration.
        bool draw_hdrd_fields();
        // Manual-entry escape hatch shared by Shift Pixels and Threshold (mm).
        bool draw_hdrd_manual_editable_field( const char * label,
                                               const char * id,
                                               int & value,
                                               int min_v,
                                               int max_v,
                                               bool & edit_mode,
                                               std::string & edit_buf );
        bool draw_hdrd_manual_input( const char * id, int & value, int min_v, int max_v,
                                      bool & edit_mode, std::string & edit_buf );
        bool draw_hdrd_slider_with_arrows( const char * id, int & value, int min_v, int max_v );
        bool draw_hdrd_reset_to_default_overlay( rs2_composite_option_id id,
                                                  std::string & error_message,
                                                  float frame_max_x,
                                                  float frame_max_y );

        // The three concerns populate_options() used to inline directly, split out so each is
        // readable on its own: scalar rs2_option models, composite-option editor priming, and
        // the on_options_changed() callback that keeps them synced to later external changes.
        void populate_scalar_options( const std::string & opt_base_label,
                                       subdevice_model * model,
                                       std::string & error_message );
        void populate_composite_options( std::string & error_message );
        void register_options_changed_callback();

    protected:
        viewer_model& _viewer;
        std::atomic<bool> _destructing;
        bool _enabled = true;
        std::shared_ptr<rs2::embedded_filter> _embedded_filter;
        std::map< rs2_option, option_model > _options_id_to_model;
        // Composite options are a separate identity/registry space from scalar rs2_option -
        // enumerated/drawn through their own loop rather than folded into _options_id_to_model.
        // No generic per-field editing UI yet; draw_options() shows read-only metadata only.
        std::vector< rs2_composite_option_id > _composite_option_ids;
        std::string _name;

        // Debounced auto-commit editor for the Decimation Filter DPP control, built on the
        // reusable composite_control_editor<T> mechanism (see composite-control-editor.h).
        composite_control_editor< rs2_decimation_filter_dpp_config > _decimation_filter_dpp_editor;

        // Per-field manual-entry toggle state for the single magnitude field.
        bool _decimation_filter_dpp_magnitude_edit_mode = false;
        std::string _decimation_filter_dpp_magnitude_edit_buf;

        // Same scheme as _decimation_filter_dpp_editor above, for RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP.
        composite_control_editor< rs2_temporal_filter_dpp_config > _temporal_filter_dpp_editor;

        // Per-field manual-entry toggle state, same scheme as the HDRD ones below - one pair per
        // field here since every field in this struct is manual-editable (none are enum-valued).
        bool _temporal_filter_dpp_smooth_alpha_edit_mode = false;
        std::string _temporal_filter_dpp_smooth_alpha_edit_buf;
        bool _temporal_filter_dpp_smooth_delta_edit_mode = false;
        std::string _temporal_filter_dpp_smooth_delta_edit_buf;
        bool _temporal_filter_dpp_persistency_index_edit_mode = false;
        std::string _temporal_filter_dpp_persistency_index_edit_buf;

        // Same scheme as _decimation_filter_dpp_editor above, for RS2_COMPOSITE_OPTION_HDRD_CONTROL.
        composite_control_editor< rs2_hdrd_control > _hdrd_editor;

        // Per-field manual-entry toggle state for the Shift Pixels / Threshold (mm) sliders -
        // mirrors option_model's own edit_mode/edit_value pattern rather than relying on ImGui's
        // native SliderInt Ctrl+Click text-input, which wasn't reliably discoverable here.
        bool _hdrd_shift_edit_mode = false;
        std::string _hdrd_shift_edit_buf;
        bool _hdrd_threshold_edit_mode = false;
        std::string _hdrd_threshold_edit_buf;
    };
}
