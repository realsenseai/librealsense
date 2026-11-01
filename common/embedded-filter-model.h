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
    class control_section;
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

        void add_options_to( control_section & section );
        void draw_composite_options( std::string & error_message );

        // Hardcoded editors, one per composite option. There is no generic per-field composite-option
        // editor (would need per-struct schema knowledge generic view code doesn't have) - same
        // special-casing app code is expected to do.
        void draw_decimation_filter_dpp_control_editor( std::string & error_message );
        void draw_temporal_filter_dpp_control_editor( std::string & error_message );
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

        // The composite option's own synced enabled field - unlike is_enabled() (which defaults
        // true before any sync), this defaults false until the editor actually reads the device,
        // so an unpopulated/never-drawn filter reads as disabled rather than enabled.
        bool is_decimation_filter_dpp_enabled() const
        {
            return _decimation_filter_dpp_editor.initialized && _decimation_filter_dpp_editor.value.enabled != 0;
        }

        // Seeds the cache above straight from the device, so is_decimation_filter_dpp_enabled() is
        // right from the first frame instead of only once the editor has been drawn. Idempotent -
        // ensure_initialized() no-ops after a successful read, and a failure leaves it unread.
        void sync_decimation_filter_dpp_state( std::string & error_message )
        {
            if( ! _embedded_filter
                || _embedded_filter->get_type() != RS2_EMBEDDED_FILTER_TYPE_DECIMATION
                || ! _embedded_filter->supports_composite_option( RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP ) )
                return;
            _decimation_filter_dpp_editor.ensure_initialized(
                _embedded_filter, RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP, error_message );
        }

        // Optional predicate; null means always available. When false the enable toggle is
        // grayed out (e.g. must be off while color streams), showing unavailable_tooltip (empty = none) on hover.
        std::function<bool()> available_predicate;
        std::string unavailable_tooltip;

        bool is_available() const { return !available_predicate || available_predicate(); }

        void embedded_filter_enable_disable(bool actual, std::string * error_message = nullptr);

    private:
        // The "what to draw" half of each editor - returns whether the user is actively interacting
        // with any field this frame, which the editor's debounce countdown needs.
        bool draw_decimation_filter_dpp_fields( std::string & error_message );
        bool draw_temporal_filter_dpp_fields( std::string & error_message );
        // Each HDRD field is wrapped in BeginDisabled()/EndDisabled() when the current
        // filter_type/shift_mode selection makes it irrelevant.
        bool draw_hdrd_fields( std::string & error_message );

        // The three concerns populate_options() used to inline directly, split out so each is
        // readable on its own: scalar rs2_option models, composite-option editor priming, and
        // the on_options_changed() callback that keeps them synced to later external changes.
        void populate_scalar_options( const std::string & opt_base_label,
                                       subdevice_model * model,
                                       std::string & error_message );
        void populate_composite_options( std::string & error_message );
        void register_options_changed_callback();

    protected:
        // The viewer stops sensors in the background; a SET sent meanwhile blocks the UI and sees the stream as still open
        bool sensor_stopping() const;

        viewer_model& _viewer;
        subdevice_model * _owner;  // holds this model in its embedded_filters, so it always outlives it
        std::atomic<bool> _destructing;
        bool _enabled = true;
        std::shared_ptr<rs2::embedded_filter> _embedded_filter;
        std::map< rs2_option, option_model > _options_id_to_model;
        // Composite options are a separate identity/registry space from scalar rs2_option -
        // enumerated/drawn through their own loop rather than folded into _options_id_to_model.
        std::vector< rs2_composite_option_id > _composite_option_ids;
        std::string _name;

        // Debounced auto-commit editor per composite option (see composite-control-editor.h), plus
        // the text-edit toggle state of each numeric field.
        composite_control_editor< rs2_decimation_filter_dpp_config > _decimation_filter_dpp_editor;
        field_edit_state _decimation_filter_dpp_magnitude_edit;

        // Magnitude range, fetched once (draw_decimation_filter_dpp_fields() used to query it on
        // every draw call - a real device round-trip in the render loop).
        rs2_decimation_filter_dpp_range _decimation_filter_dpp_range{};
        bool _decimation_filter_dpp_range_initialized = false;

        composite_control_editor< rs2_temporal_filter_dpp_config > _temporal_filter_dpp_editor;
        field_edit_state _temporal_filter_dpp_smooth_alpha_edit;
        field_edit_state _temporal_filter_dpp_smooth_delta_edit;

        composite_control_editor< rs2_hdrd_control > _hdrd_editor;
        field_edit_state _hdrd_shift_edit;
        field_edit_state _hdrd_threshold_edit;
    };
}
