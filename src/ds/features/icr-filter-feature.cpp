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

#include <cstddef>
#include <algorithm>
#include <cstring>


namespace librealsense {


// D500_CAMERA_CID_MINZ range descriptor; step is 1 per R/W slot (all-integer params), 0 for the FW-computed
// MinZ telemetry. Defaults track LibRS's rs2_icr_control field docs.
// FW is the range authority, and USB reads it from FW and ignores these tables. They are only a temporary fallback
// for GMSL, where the MIPI driver does not expose the FW MIN/MAX/RES/DEF windows yet - to be dropped once it does.
static constexpr int32_t icr_min[]  = { 0, 1, 0,   0,     0,     0,   0,   0 };
static constexpr int32_t icr_max[]  = { 2, 2, 2, 256, 10000, 10000, 500, 500 };
static constexpr int32_t icr_step[] = { 1, 1, 1,   1,     1,     1,   0,   0 };
static constexpr int32_t icr_def[]  = { 0, 1, 0, 126,     0,     0,   0,   0 };
static constexpr size_t icr_param_count = sizeof( icr_def ) / sizeof( icr_def[0] );

// The baseline FW has 7 parameters: enable, filter type, downscale ratio, shift mode, shift pixels, threshold mode and
// threshold. They are mapped onto rs2_icr_control - Smoothing, Completion and MinZ read 0 - and the threshold is kept.
static constexpr uint8_t legacy_param_count = 7;
static constexpr size_t icr_params_offset = offsetof( rs2_icr_control, mode );

static std::vector< uint8_t > legacy_to_icr( const uint8_t * data, bool is_step )
{
    int32_t l[icr_param_count + 1], p[icr_param_count] = {};
    std::memcpy( l, data + icr_params_offset, sizeof( int32_t ) * legacy_param_count );
    p[0] = is_step ? 1 : l[0] ? ( l[1] ? RS2_ICR_MODE_HIGH_RES : RS2_ICR_MODE_DOWNSCALE_HDR ) : RS2_ICR_MODE_OFF;
    std::memcpy( p + 1, l + 2, sizeof( int32_t ) * 3 );  // downscale ratio, shift mode, shift pixels
    std::vector< uint8_t > out( data, data + sizeof( rs2_icr_control ) );
    reinterpret_cast< dpp_header * >( out.data() )->param_count = icr_param_count;
    std::memcpy( out.data() + icr_params_offset, p, sizeof( p ) );
    return out;
}

// `legacy` is the current 7-parameter payload: the slots ICR does not map are carried back as they are
static std::vector< uint8_t > icr_to_legacy( const rs2_icr_control & cfg, std::vector< uint8_t > legacy )
{
    int32_t l[legacy_param_count];
    std::memcpy( l, legacy.data() + icr_params_offset, sizeof( l ) );
    l[0] = cfg.mode != RS2_ICR_MODE_OFF;
    if( cfg.mode != RS2_ICR_MODE_OFF )  // Off leaves the filter type as is
        l[1] = cfg.mode == RS2_ICR_MODE_HIGH_RES;
    l[2] = cfg.downscale_ratio;
    l[3] = cfg.high_res_mode;
    l[4] = cfg.high_res_offset;
    std::memcpy( legacy.data() + icr_params_offset, l, sizeof( l ) );
    return legacy;
}

static constexpr dpp_control_desc icr_desc = {
    0x0008,  // ctl_id (D500_DPP_XU_MINZ_CONTROL_ID = BIT(3))
    icr_param_count,
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

// FW rejects unsupported ICR profiles only once streaming starts, with an obscure error (Depth+IR on Windows even
// needs a replug) - so the SDK checks the ICR profile limits first: the explicit profile exclusions, FPS up to 30,
// and integral Secondary dimensions of at least 128 pixels wide for Downscale (480x270 x4 is out as 270 is not
// divisible by 4, 424x240 x4 as its Secondary is 106 wide).
static void icr_allowed_or_throw( const void * icr_control, size_t size, const video_stream_profile_interface & depth,
                                  const sensor_interface & depth_sensor )
{
    if( ! icr_control || size != sizeof( rs2_icr_control ) )
        return;
    rs2_icr_control cfg;
    std::memcpy( &cfg, icr_control, sizeof( cfg ) );
    if( cfg.mode == RS2_ICR_MODE_OFF )
        return;

    uint32_t const w = depth.get_width();
    uint32_t const h = depth.get_height();
    uint32_t const fps = depth.get_framerate();
    bool const excluded = ( w == 1600 && h == 1300 ) || ( w == 1600 && h == 966 ) || ( w == 256 && h == 144 );
    uint32_t const max_fps = 30;
    if( excluded || fps > max_fps )
        throw invalid_value_exception( rsutils::string::from()
            << "Improved Close Range Depth requires Depth up to " << max_fps << " FPS and not 1600x1300, 1600x966 or 256x144, but Depth is "
            << w << "x" << h << " @ " << fps << " FPS" );

    if( cfg.mode != RS2_ICR_MODE_DOWNSCALE_HDR )
        return;
    uint32_t const ratio = cfg.downscale_ratio == 2 ? 4 : 2;
    if( ratio == 4 && embedded_decimation_enabled( depth_sensor ) )
        throw invalid_value_exception(
            "Improved Close Range Depth Downscale x4 cannot be combined with the embedded Decimation filter" );
    if( w % ratio != 0 || h % ratio != 0 )
        throw invalid_value_exception( rsutils::string::from()
            << "Improved Close Range Depth Downscale x" << ratio << " requires Depth dimensions divisible by " << ratio
            << ", but Depth is " << w << "x" << h );
    if( w / ratio < 128 )
        throw invalid_value_exception( rsutils::string::from()
            << "Improved Close Range Depth Downscale x" << ratio << " requires Depth at least " << 128 * ratio
            << " pixels wide, but Depth is " << w << "x" << h );
}

// Rejects out-of-range values locally, never writes the read-only MinZ telemetry slots, and rejects enabling ICR
// while a Depth profile it cannot process is open. `control` is the transport (XU or DDS).
class icr_option : public composite_option_interface
{
public:
    icr_option( std::shared_ptr< composite_option_interface > control, const sensor_interface & depth_sensor )
        : _xu( std::move( control ) )
        , _depth_sensor( depth_sensor )
    {
    }

    std::vector< uint8_t > get_raw() const override
    {
        auto raw = read_xu();
        return is_legacy( raw ) ? legacy_to_icr( raw.data(), false ) : raw;
    }
    void set_raw( const void * data, size_t size ) override
    {
        if( ! data || size != sizeof( rs2_icr_control ) )
            throw invalid_value_exception( "Improved Close Range control payload has an unexpected size" );
        rs2_icr_control cfg;
        std::memcpy( &cfg, data, sizeof( cfg ) );
        range_check_or_throw( cfg );
        if( auto depth = find_depth_profile( _depth_sensor.get_active_streams() ) )
            icr_allowed_or_throw( &cfg, sizeof( cfg ), *depth, _depth_sensor );

        auto xu_raw = read_xu();
        if( is_legacy( xu_raw ) )
        {
            auto legacy = icr_to_legacy( cfg, std::move( xu_raw ) );
            _xu->set_raw( legacy.data(), legacy.size() );
            return;
        }
        // Params 7/8 are FW-computed: carry back what the device reports instead of the caller's values
        auto const & raw = xu_raw;
        rs2_icr_control current;
        std::memcpy( &current, raw.data(), sizeof( current ) );
        cfg.min_z_mm = current.min_z_mm;
        cfg.improved_min_z_mm = current.improved_min_z_mm;
        _xu->set_raw( &cfg, sizeof( cfg ) );
    }
    // {min, max, step, def}, each a complete payload with its own envelope
    std::vector< uint8_t > get_raw_range() const override
    {
        auto raw = _xu->get_raw_range();
        if( raw.size() != 4 * sizeof( rs2_icr_control ) )
            throw invalid_value_exception( "Improved Close Range control returned an unexpected range payload size" );
        for( size_t i = 0; i < 4; ++i )
        {
            auto * bound = raw.data() + i * sizeof( rs2_icr_control );
            check_envelope_or_throw( bound, sizeof( rs2_icr_control ) );
            if( bound[offsetof( dpp_header, param_count )] == legacy_param_count )
            {
                auto icr = legacy_to_icr( bound, i == 2 );
                std::copy( icr.begin(), icr.end(), raw.begin() + i * sizeof( rs2_icr_control ) );
            }
        }
        return raw;
    }
    bool is_read_only() const override { return _xu->is_read_only(); }
    bool is_enabled() const override { return _xu->is_enabled(); }
    const char * get_description() const override { return _xu->get_description(); }

private:
    std::vector< uint8_t > read_xu() const
    {
        auto raw = _xu->get_raw();
        check_envelope_or_throw( raw.data(), raw.size() );
        return raw;
    }
    static bool is_legacy( std::vector< uint8_t > const & raw ) { return raw[offsetof( dpp_header, param_count )] == legacy_param_count; }

    // The payload size, header version, control ID, param count and param type must be the ICR schema
    static void check_envelope_or_throw( const uint8_t * data, size_t size )
    {
        if( size != sizeof( rs2_icr_control ) )
            throw invalid_value_exception( "Improved Close Range control returned an unexpected payload size" );
        dpp_header hdr = {};
        std::memcpy( &hdr, data, sizeof( hdr ) );
        if( hdr.version != DPP_HEADER_CURRENT_VERSION || hdr.ctl_id != icr_desc.ctl_id
            || ( hdr.param_count != icr_desc.param_count && hdr.param_count != legacy_param_count ) || hdr.param_type != icr_desc.param_type )
            throw invalid_value_exception( rsutils::string::from()
                << "Improved Close Range control returned an unsupported schema (version=" << (int)hdr.version
                << " ctl_id=" << (int)hdr.ctl_id << " param_count=" << (int)hdr.param_count
                << " param_type=" << (int)hdr.param_type << ")" );
    }

    // Against the FW-reported range - the SDK keeps no business ranges of its own
    void range_check_or_throw( const rs2_icr_control & cfg ) const
    {
        static const char * const names[icr_param_count] = { "Mode", "Downscale Ratio", "High-Res Mode", "High-Res Offset",
                                                             "Smoothing Offset", "Completion Offset", "MinZ", "Improved MinZ" };
        auto const range = get_raw_range();
        int32_t params[icr_param_count], min[icr_param_count], max[icr_param_count];
        std::memcpy( params, &cfg.mode, sizeof( params ) );
        std::memcpy( min, range.data() + offsetof( rs2_icr_control, mode ), sizeof( min ) );
        std::memcpy( max, range.data() + sizeof( rs2_icr_control ) + offsetof( rs2_icr_control, mode ), sizeof( max ) );
        for( size_t i = 0; i < icr_param_count; ++i )
            if( params[i] < min[i] || params[i] > max[i] )
                throw invalid_value_exception( rsutils::string::from()
                    << "Improved Close Range " << names[i] << " " << params[i] << " is out of range [" << min[i] << ", "
                    << max[i] << "]" );
    }

    std::shared_ptr< composite_option_interface > _xu;
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

    // ICR may have been enabled while the sensor was closed - check it against the Depth profile being opened
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
                LOG_WARNING( "Improved Close Range state unavailable, skipping profile check: " << e.what() );
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
