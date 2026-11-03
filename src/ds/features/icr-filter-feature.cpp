// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.


#include <src/ds/features/icr-filter-feature.h>
#include <src/ds/d500/d500-device.h>
#include <src/ds/d500/icr-embedded-filter.h>
#include <src/ds/ds-private.h>
#include <src/ds/composite-xu-option.h>
#include <src/uvc-sensor.h>
#include <src/core/video.h>
#include <src/librealsense-exception.h>
#include <rsutils/string/from.h>

#include <librealsense2/h/rs_composite_option.h>
#include <librealsense2/h/rs_icr_control.h>
#include <librealsense2/h/rs_decimation_filter_dpp.h>

#include <algorithm>
#include <cstring>


namespace librealsense {


// D500_CAMERA_CID_MINZ range descriptor; step is 1 per slot (all-integer params). Defaults
// track LibRS's rs2_icr_control field docs.
// GMSL cannot query ranges (V4L2 array controls report one scalar bound and the FW
// registers hold only the current value), so these tables are the only source of the MIPI
// range. USB reads it from FW and ignores this desc.
static constexpr int32_t icr_min[]  = { 0, 0, 1, 0,   0, 0, 0 };
static constexpr int32_t icr_max[]  = { 1, 1, 2, 2, 256, 2, 65535 };
static constexpr int32_t icr_step[] = { 1, 1, 1, 1,   1, 1, 1 };
static constexpr int32_t icr_def[]  = { 0, 0, 1, 0, 126, 1, 0 };

static constexpr dpp_control_desc icr_desc = {
    0x0008,  // ctl_id (D500_DPP_XU_MINZ_CONTROL_ID = BIT(3))
    7,       // param_count
    0x00,    // param_type (all-integer)
    icr_min, icr_max, icr_step, icr_def
};


static std::shared_ptr< video_stream_profile_interface > find_depth_profile( const stream_profiles & profiles )
{
    for( auto & p : profiles )
        if( p && p->get_stream_type() == RS2_STREAM_DEPTH )
            return std::dynamic_pointer_cast< video_stream_profile_interface >( p );
    return nullptr;
}

static bool embedded_decimation_enabled( const sensor_interface & depth_sensor )
{
    for( auto & filter : depth_sensor.get_supported_embedded_filters() )
        if( filter->supports_composite_option( RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP ) )
        {
            rs2_decimation_filter_dpp_config cfg;
            std::vector< uint8_t > raw;
            try
            {
                raw = filter->get_composite_option( RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP ).get_raw();
            }
            catch( const std::exception & e )
            {
                LOG_WARNING( "Decimation state unavailable, treating it as disabled: " << e.what() );
                return false;
            }
            if( raw.size() != sizeof( cfg ) )
                return false;
            std::memcpy( &cfg, raw.data(), sizeof( cfg ) );
            return cfg.enabled != 0;
        }
        // DDS exposes Decimation as scalar options
        else if( filter->get_type() == RS2_EMBEDDED_FILTER_TYPE_DECIMATION )
            return filter->supports_option( RS2_OPTION_EMBEDDED_FILTER_ENABLED )
                && filter->get_option( RS2_OPTION_EMBEDDED_FILTER_ENABLED ).query() != 0;
    return false;
}

// FW rejects unsupported Improved Close Range profiles only once streaming starts, with an obscure error (Depth+IR on Windows even
// needs a replug) - so the SDK checks the Improved Close Range profile limits first.
static void icr_allowed_or_throw( const void * icr_control, size_t size, const video_stream_profile_interface & depth,
                                   const sensor_interface & depth_sensor )
{
    if( ! icr_control || size != sizeof( rs2_icr_control ) )
        return;
    rs2_icr_control cfg;
    std::memcpy( &cfg, icr_control, sizeof( cfg ) );
    if( ! cfg.enable )
        return;

    bool const lookup_shift = cfg.filter_type != 0;
    bool const x4 = ! lookup_shift && cfg.downscale_ratio == 2;
    if( x4 && embedded_decimation_enabled( depth_sensor ) )
        throw invalid_value_exception(
            "Improved Close Range Depth Downscale x4 cannot be combined with the embedded Decimation filter" );

    // Downscale x2 defaults
    const char * mode = "Downscale x2";
    uint32_t min_w = 640;
    uint32_t min_h = 360;
    uint32_t align = 2;
    if( x4 )
    {
        mode = "Downscale x4";
        min_w = 1280;
        min_h = 720;
        align = 4;
    }
    else if( lookup_shift )
    {
        mode = "Lookup Shift";
        min_w = 424;
        min_h = 240;
        align = 1;
    }

    uint32_t const max_fps = 30;
    uint32_t const w = depth.get_width();
    uint32_t const h = depth.get_height();
    uint32_t const fps = depth.get_framerate();
    bool const size_ok = w >= min_w && h >= min_h && w % align == 0 && h % align == 0;
    if( ! size_ok || fps > max_fps )
        throw invalid_value_exception( rsutils::string::from()
            << "Improved Close Range Depth " << mode << " requires Depth of at least " << min_w << "x" << min_h
            << " at up to " << max_fps << " FPS, but Depth is " << w << "x" << h << " @ " << fps << " FPS" );
}

// Rejects enabling Improved Close Range while a Depth profile it cannot process is open. `control` is the transport (XU or DDS).
class icr_option : public composite_option_interface
{
public:
    icr_option( std::shared_ptr< composite_option_interface > control, const sensor_interface & depth_sensor )
        : _control( std::move( control ) )
        , _depth_sensor( depth_sensor )
    {
    }

    std::vector< uint8_t > get_raw() const override
    {
        auto raw = _control->get_raw();
        check_envelope_or_throw( raw.data(), raw.size() );
        return raw;
    }
    void set_raw( const void * data, size_t size ) override
    {
        auto active = _depth_sensor.get_active_streams();
        geometry_unchanged_or_throw( data, size, active );
        if( auto depth = find_depth_profile( active ) )
            icr_allowed_or_throw( data, size, *depth, _depth_sensor );
        _control->set_raw( data, size );
    }
    // {min, max, step, def}, each a complete payload with its own envelope
    std::vector< uint8_t > get_raw_range() const override
    {
        auto raw = _control->get_raw_range();
        if( raw.size() != 4 * sizeof( rs2_icr_control ) )
            throw invalid_value_exception( "Improved Close Range control returned an unexpected range payload size" );
        for( size_t i = 0; i < 4; ++i )
            check_envelope_or_throw( raw.data() + i * sizeof( rs2_icr_control ), sizeof( rs2_icr_control ) );
        return raw;
    }
    bool is_read_only() const override { return _control->is_read_only(); }
    bool is_enabled() const override { return _control->is_enabled(); }
    const char * get_description() const override { return _control->get_description(); }

private:
    // The payload size, header version, control ID, param count and param type must be the ICR schema
    static void check_envelope_or_throw( const uint8_t * data, size_t size )
    {
        if( size != sizeof( rs2_icr_control ) )
            throw invalid_value_exception( "Improved Close Range control returned an unexpected payload size" );
        dpp_header hdr = {};
        std::memcpy( &hdr, data, sizeof( hdr ) );
        if( hdr.version != DPP_HEADER_CURRENT_VERSION || hdr.ctl_id != icr_desc.ctl_id
            || hdr.param_count != icr_desc.param_count || hdr.param_type != icr_desc.param_type )
            throw invalid_value_exception( rsutils::string::from()
                << "Improved Close Range control returned an unsupported schema (version=" << (int)hdr.version
                << " ctl_id=" << (int)hdr.ctl_id << " param_count=" << (int)hdr.param_count
                << " param_type=" << (int)hdr.param_type << ")" );
    }

    // FW rejects changing Filter Type or Downscale Ratio while Depth or IR stream, with an obscure error
    void geometry_unchanged_or_throw( const void * data, size_t size, const stream_profiles & active ) const
    {
        bool const streaming = std::any_of( active.begin(), active.end(), []( const std::shared_ptr< stream_profile_interface > & p )
                                            { return p->get_stream_type() == RS2_STREAM_DEPTH || p->get_stream_type() == RS2_STREAM_INFRARED; } );
        if( ! streaming || ! data || size != sizeof( rs2_icr_control ) )
            return;
        rs2_icr_control next;
        rs2_icr_control current;
        std::memcpy( &next, data, sizeof( next ) );
        auto raw = _control->get_raw();
        if( raw.size() != sizeof( current ) )
            return;
        std::memcpy( &current, raw.data(), sizeof( current ) );
        if( next.filter_type != current.filter_type || next.downscale_ratio != current.downscale_ratio )
            throw wrong_api_call_sequence_exception( "Improved Close Range Depth Filter Type and Downscale Ratio cannot be "
                                                     "changed while Depth or Infrared is streaming" );
    }

    std::shared_ptr< composite_option_interface > _control;
    const sensor_interface & _depth_sensor;
};


/* static */ const feature_id icr_filter_feature::ID = "Improved Close Range filter feature";

icr_filter_feature::icr_filter_feature( d500_device & owner, d500_depth_sensor & depth_sensor, bool is_mipi )
    : icr_filter_feature( owner,
                           depth_sensor,
                           composite_xu_option::create(
                               is_mipi,
                               std::dynamic_pointer_cast< uvc_sensor >( depth_sensor.get_raw_sensor() ),
                               ds::depth_xu,
                               ds::DS5_HKR_ICR_CONTROL,
                               static_cast< uint32_t >( sizeof( rs2_icr_control ) ),
                               "Improved Close Range Control (prototype) - use rs2_set/get_composite_option, see rs_icr_control.h",
                               icr_desc ) )
{
    depth_sensor.add_embedded_filter( _filter );
}

icr_filter_feature::icr_filter_feature( device & owner, const sensor_interface & depth_sensor,
                                          std::shared_ptr< composite_option_interface > control )
{
    auto option = std::make_shared< icr_option >( std::move( control ), depth_sensor );
    _filter = std::make_shared< icr_embedded_filter >( option, RS2_COMPOSITE_OPTION_ICR_CONTROL );

    // Improved Close Range may have been enabled while the sensor was closed - check it against the Depth profile being opened
    owner.add_stream_combination_validator(
        [option, &depth_sensor]( const stream_profiles & requests )
        {
            auto depth = find_depth_profile( requests );
            if( ! depth )
                return;
            std::vector< uint8_t > raw;
            try
            {
                raw = option->get_raw();
            }
            catch( const std::exception & e )
            {
                LOG_WARNING( "Improved Close Range Depth state unavailable, skipping profile check: " << e.what() );
                return;
            }
            icr_allowed_or_throw( raw.data(), raw.size(), *depth, depth_sensor );
        } );
}

feature_id icr_filter_feature::get_id() const
{
    return ID;
}


}  // namespace librealsense
