// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include <rsutils/easylogging/easyloggingpp.h>
#include <algorithm>
#include <string>
#include "subdevice-model.h"
#include "embedded-filter-model.h"
#include "control-section.h"
#include "viewer.h"


namespace rs2
{
    namespace
    {
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
        : _embedded_filter(filter), _viewer(viewer), _owner(owner), _destructing(false)
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

    void embedded_filter_model::add_options_to( control_section & section )
    {
        for (auto& id_and_model : _options_id_to_model)
        {
            if( id_and_model.first == RS2_OPTION_EMBEDDED_FILTER_ENABLED )
                continue;

            section.add( std::make_unique< option_control >( id_and_model.second ) );
        }
    }


    void embedded_filter_model::draw_composite_options( std::string & error_message )
    {
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

    // ==== Composite editors =====
    namespace
    {
        // Framing/dim/commit orchestration shared by the three editors: frame the fields plus a
        // Reset-to-Default button, grey the box via global alpha (not BeginDisabled) while the
        // filter is off so fields stay clickable - editing while off forces enable back on at
        // commit time via before_commit.
        template< typename T, typename RangeT >
        void draw_composite_editor( composite_control_editor< T > & editor,
                                    float extra_width,
                                    const std::shared_ptr< rs2::embedded_filter > & filter,
                                    rs2_composite_option_id id,
                                    bool enabled,
                                    bool hold_commit,
                                    std::string & error_message,
                                    const std::function< bool() > & draw_fields,
                                    const std::function< void( T & ) > & sanitize,
                                    const std::function< void( T & ) > & before_commit )
        {
            editor.extra_width = extra_width;
            ImGui::Dummy( ImVec2( 0, 4 ) );
            ImVec2 frame_min = ImGui::GetCursorScreenPos();
            frame_min.x -= 9.f;   // same gap to the sliders on the left as the right
            frame_min.y -= 4.f;
            float frame_width = ImGui::GetContentRegionAvail().x;

            if( ! enabled )
                ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.35f );

            bool any_field_active = draw_fields();
            any_field_active |= editor.template draw_reset_to_default< RangeT >( filter, id, error_message, sanitize );
            ImGui::Dummy( ImVec2( 0, 6 ) );   // bottom padding inside the pending-commit border

            ImVec2 frame_max( frame_min.x + frame_width, ImGui::GetCursorScreenPos().y );
            editor.end_frame_and_maybe_commit( filter, id, error_message, frame_min, frame_max, any_field_active, hold_commit,
                                               before_commit );

            if( ! enabled )
                ImGui::PopStyleVar();
        }

        // Same names/descriptions/value labels as the software temporal filter (src/proc/temporal-filter.cpp)
        // and the DDS one, so the D500 panel reads the same as every other Temporal panel.
        const char * const temporal_alpha_description
            = "Alpha factor of Exp. moving average, 1=no filter, 0=infinite filter. Smooths against previous frames";
        const char * const temporal_delta_description
            = "Step-size boundary, the threshold used to preserve surfaces (edges). Higher = smooths over bigger depth differences";
        const char * const temporal_persistency_description
            = "Persistency mode. Rules for replacing a missing pixel with its last valid value";
        const char * const temporal_persistency_labels[] = { "Disabled",          "Valid in 8/8",      "Valid in 2/last 3",
                                                             "Valid in 2/last 4", "Valid in 2/8",      "Valid in 1/last 2",
                                                             "Valid in 1/last 5", "Valid in 1/8",      "Always on" };
    }

    // Magnitude range is fetched once (see _decimation_filter_dpp_range) rather than on every draw
    // call - this runs every frame the panel is open.
    bool embedded_filter_model::draw_decimation_filter_dpp_fields( std::string & error_message )
    {
        int magnitude_min = _decimation_filter_dpp_editor.value.magnitude;
        int magnitude_max = _decimation_filter_dpp_editor.value.magnitude;
        if( ! _decimation_filter_dpp_range_initialized )
        {
            try
            {
                _decimation_filter_dpp_range = _embedded_filter->get_composite_option_range_as< rs2_decimation_filter_dpp_range >(
                    RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP );
                _decimation_filter_dpp_range_initialized = true;
            }
            catch( const std::exception & )
            {
                // Best-effort range only - fall back to a degenerate [current,current] slider
                // rather than disrupting the rest of the editor over a failed range query. Not
                // marked initialized, so a later frame can retry.
            }
        }
        if( _decimation_filter_dpp_range_initialized )
        {
            magnitude_min = _decimation_filter_dpp_range.min.magnitude;
            magnitude_max = _decimation_filter_dpp_range.max.magnitude;
        }

        return _decimation_filter_dpp_editor.draw_scalar_field( "Magnitude", "Downscale factor - currently fixed by firmware.",
            "decimation_filter_dpp_magnitude", _decimation_filter_dpp_editor.value.magnitude, magnitude_min, magnitude_max, 1, 1.f,
            _decimation_filter_dpp_magnitude_edit, error_message );
    }

    void embedded_filter_model::draw_decimation_filter_dpp_control_editor( std::string & error_message )
    {
        const auto id = RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP;
        if( ! _decimation_filter_dpp_editor.ensure_initialized( _embedded_filter, id, error_message, print_decimation_filter_dpp_config ) )
            return;
        sanitize_decimation_filter_dpp_config( _decimation_filter_dpp_editor.value );

        // No separate Enable checkbox - the row header's own toggle drives the struct's enabled field.
        // The device also rejects a SET once Depth/IR is streaming (read-only while active) -
        // end_frame_and_maybe_commit()'s own error_message surfaces that rejection.
        draw_composite_editor< rs2_decimation_filter_dpp_config, rs2_decimation_filter_dpp_range >(
            _decimation_filter_dpp_editor, _viewer.panel_extra_width(), _embedded_filter, id, _enabled, sensor_stopping(), error_message,
            [&]() { return draw_decimation_filter_dpp_fields( error_message ); },
            sanitize_decimation_filter_dpp_config,
            [this]( rs2_decimation_filter_dpp_config & v )
            {
                v.enabled = 1;
                _enabled = true;
            } );
    }

    // Ranges per rs_temporal_filter_dpp.h; smooth_alpha is a [0,1] value scaled into [0,1000] on
    // the wire, shown back as 0.00-1.00 like the software/DDS filter's option.
    bool embedded_filter_model::draw_temporal_filter_dpp_fields( std::string & error_message )
    {
        auto & v = _temporal_filter_dpp_editor.value;
        bool any_field_active = _temporal_filter_dpp_editor.draw_scalar_field(
            rs2_option_to_string( RS2_OPTION_FILTER_SMOOTH_ALPHA ), temporal_alpha_description,
            "temporal_filter_dpp_smooth_alpha", v.smooth_alpha, 0, 1000, 10, 1000.f,
            _temporal_filter_dpp_smooth_alpha_edit, error_message );

        any_field_active |= _temporal_filter_dpp_editor.draw_scalar_field(
            rs2_option_to_string( RS2_OPTION_FILTER_SMOOTH_DELTA ), temporal_delta_description,
            "temporal_filter_dpp_smooth_delta", v.smooth_delta, 1, 100, 1, 1.f,
            _temporal_filter_dpp_smooth_delta_edit, error_message );

        any_field_active |= _temporal_filter_dpp_editor.draw_enum_field(
            "Persistency mode", temporal_persistency_description, "temporal_filter_dpp_persistency_index",
            temporal_persistency_labels, v.persistency_index );

        return any_field_active;
    }

    void embedded_filter_model::draw_temporal_filter_dpp_control_editor( std::string & error_message )
    {
        const auto id = RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP;
        if( ! _temporal_filter_dpp_editor.ensure_initialized( _embedded_filter, id, error_message, print_temporal_filter_dpp_config ) )
            return;
        sanitize_temporal_filter_dpp_config( _temporal_filter_dpp_editor.value );

        draw_composite_editor< rs2_temporal_filter_dpp_config, rs2_temporal_filter_dpp_range >(
            _temporal_filter_dpp_editor, _viewer.panel_extra_width(), _embedded_filter, id, _enabled, sensor_stopping(), error_message,
            [&]() { return draw_temporal_filter_dpp_fields( error_message ); },
            sanitize_temporal_filter_dpp_config,
            [this]( rs2_temporal_filter_dpp_config & v )
            {
                v.enabled = 1;
                _enabled = true;
            } );
    }

    // Irrelevant fields stay visible but BeginDisabled()-greyed, not hidden. |= (not ||): each
    // call draws real widgets and must run every frame.
    bool embedded_filter_model::draw_hdrd_fields( std::string & error_message )
    {
        static const char * const filter_type_labels[] = { "Downscale", "Lookup Shift" };
        static const char * const downscale_ratio_labels[] = { "x2", "x4" };
        static const char * const shift_mode_labels[] = { "Shift 126px", "Shift 64px", "Manual" };
        static const char * const threshold_mode_labels[] = { "Zero range", "MinZ (computed)", "Manual" };

        auto & v = _hdrd_editor.value;
        bool any_field_active = _hdrd_editor.draw_enum_field( "Filter Type",
            "Downscale: reduce resolution by a fixed ratio. Lookup Shift: shift the disparity lookup by a pixel count.\n"
            "Not all Depth resolutions and frame rates are supported.",
            "hdrd_filter_type", filter_type_labels, v.filter_type );

        const bool downscale_relevant = ( v.filter_type == 0 );
        if( ! downscale_relevant )
            ImGui::BeginDisabled();
        // Wire values are 1 (x2) and 2 (x4), not 0-based like the other enum fields - value_offset
        // converts to and from a 0-based index rather than changing the documented wire values.
        any_field_active |= _hdrd_editor.draw_enum_field( "Downscale Ratio", "Resolution reduction applied by the Downscale filter type.",
            "hdrd_downscale_ratio", downscale_ratio_labels, v.downscale_ratio, 1 );
        if( ! downscale_relevant )
            ImGui::EndDisabled();

        const bool shift_relevant = ( v.filter_type == 1 );
        if( ! shift_relevant )
            ImGui::BeginDisabled();
        any_field_active |= _hdrd_editor.draw_enum_field( "Shift Mode", "Fixed shift preset, or Manual to use the Shift Pixels value below.",
            "hdrd_shift_mode", shift_mode_labels, v.shift_mode );
        if( ! shift_relevant )
            ImGui::EndDisabled();

        const bool shift_pixels_relevant = shift_relevant && ( v.shift_mode == 2 );
        if( ! shift_pixels_relevant )
            ImGui::BeginDisabled();
        any_field_active |= _hdrd_editor.draw_scalar_field( "Shift Pixels", "Disparity lookup shift, in pixels, used when Shift Mode is Manual.",
            "hdrd_shift", v.shift_pixels, 0, 256, 1, 1.f, _hdrd_shift_edit, error_message );
        if( ! shift_pixels_relevant )
            ImGui::EndDisabled();

        any_field_active |= _hdrd_editor.draw_enum_field( "Threshold Mode",
            "Zero range: fill only originally-empty depth pixels.\n"
            "MinZ (computed): firmware picks the threshold for the active resolution.\n"
            "Manual: use the threshold value below.",
            "hdrd_threshold_mode", threshold_mode_labels, v.threshold_mode );

        const bool threshold_relevant = ( v.threshold_mode == 2 );
        if( ! threshold_relevant )
            ImGui::BeginDisabled();
        any_field_active |= _hdrd_editor.draw_scalar_field( "Threshold (mm)", "Depth threshold, in mm, used when Threshold Mode is Manual.",
            "hdrd_threshold", v.threshold_mm, 0, 65535, 1, 1.f, _hdrd_threshold_edit, error_message );
        if( ! threshold_relevant )
            ImGui::EndDisabled();

        return any_field_active;
    }

    void embedded_filter_model::draw_hdrd_control_editor( std::string & error_message )
    {
        const auto id = RS2_COMPOSITE_OPTION_HDRD_CONTROL;
        // A rejected enable (e.g. unsupported Depth profile) leads to a re-read here - keep the toggle in sync with it
        auto on_read = [this]( const rs2_hdrd_control & v )
        {
            print_hdrd_control( v );
            _enabled = v.enable != 0;
        };
        if( ! _hdrd_editor.ensure_initialized( _embedded_filter, id, error_message, on_read ) )
            return;
        sanitize_hdrd_control( _hdrd_editor.value );

        draw_composite_editor< rs2_hdrd_control, rs2_hdrd_control_range >(
            _hdrd_editor, _viewer.panel_extra_width(), _embedded_filter, id, _enabled, sensor_stopping(), error_message,
            [&]() { return draw_hdrd_fields( error_message ); },
            sanitize_hdrd_control,
            [this]( rs2_hdrd_control & v )
            {
                v.enable = 1;
                _enabled = true;
            } );
    }

    bool embedded_filter_model::sensor_stopping() const
    {
        return _owner && _owner->is_stopping();
    }

    void embedded_filter_model::embedded_filter_enable_disable(bool actual, std::string * error_message)
    {
        // Composite-only embedded filters register no RS2_OPTION_EMBEDDED_FILTER_ENABLED scalar
        // option - route the toggle through the composite option's own `enable` field instead,
        // read-modify-write so the other fields go back as last reported, not zero-initialized.
        // Only re-read if we don't already have a live-tracked value - once we do, it already
        // mirrors every write made, so a fresh GET is a needless second XU transaction. The
        // device itself also rejects this SET while Depth/IR is streaming for Decimation - that
        // rejection surfaces via the caught exception below like any other, no special-casing
        // needed here.
        if( _embedded_filter->supports_composite_option( RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP ) )
        {
            try
            {
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
