// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.


#include <src/ds/features/hdrd-filter-feature.h>
#include <src/ds/d500/d500-device.h>
#include <src/ds/d500/hdrd-embedded-filter.h>
#include <src/ds/ds-private.h>
#include <src/ds/composite-xu-option.h>
#include <src/uvc-sensor.h>
#include <src/core/video.h>
#include <src/librealsense-exception.h>
#include <rsutils/string/from.h>

#include <librealsense2/h/rs_composite_option.h>
#include <librealsense2/h/rs_hdrd_control.h>
#include <librealsense2/h/rs_decimation_filter_dpp.h>

#include <algorithm>
#include <cstring>


namespace librealsense {


// D500_CAMERA_CID_MINZ range descriptor; step is 1 per slot (all-integer params). Defaults
// track LibRS's rs2_hdrd_control field docs.
// GMSL cannot query ranges (V4L2 array controls report one scalar bound and the FW
// registers hold only the current value), so these tables are the only source of the MIPI
// range. USB reads it from FW and ignores this desc.
static constexpr int32_t minz_min[]  = { 0, 0, 1, 0,   0, 0, 0 };
static constexpr int32_t minz_max[]  = { 1, 1, 2, 2, 256, 2, 65535 };
static constexpr int32_t minz_step[] = { 1, 1, 1, 1,   1, 1, 1 };
static constexpr int32_t minz_def[]  = { 0, 0, 1, 0, 126, 1, 0 };

static constexpr dpp_control_desc minz_desc = {
    0x0008,  // ctl_id (D500_DPP_XU_MINZ_CONTROL_ID = BIT(3))
    7,       // param_count
    0x00,    // param_type (all-integer)
    minz_min, minz_max, minz_step, minz_def
};


static std::shared_ptr< video_stream_profile_interface > find_depth_profile( const stream_profiles & profiles )
{
    for( auto & p : profiles )
        if( p && p->get_stream_type() == RS2_STREAM_DEPTH )
            return std::dynamic_pointer_cast< video_stream_profile_interface >( p );
    return nullptr;
}

static bool embedded_decimation_enabled( const d500_depth_sensor & depth_sensor )
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
    return false;
}

// FW rejects unsupported MinZ profiles only once streaming starts, with an obscure error (Depth+IR on Windows even
// needs a replug) - so the SDK checks the D585 MinZ profile limits first.
static void minz_allowed_or_throw( const void * hdrd_control, size_t size, const video_stream_profile_interface & depth,
                                   const d500_depth_sensor & depth_sensor )
{
    if( ! hdrd_control || size != sizeof( rs2_hdrd_control ) )
        return;
    rs2_hdrd_control cfg;
    std::memcpy( &cfg, hdrd_control, sizeof( cfg ) );
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

// Rejects enabling MinZ while a Depth profile it cannot process is open.
class minz_option : public composite_option_interface
{
public:
    minz_option( std::shared_ptr< composite_xu_option > xu, const d500_depth_sensor & depth_sensor )
        : _xu( std::move( xu ) )
        , _depth_sensor( depth_sensor )
    {
    }

    std::vector< uint8_t > get_raw() const override { return _xu->get_raw(); }
    void set_raw( const void * data, size_t size ) override
    {
        auto active = _depth_sensor.get_active_streams();
        geometry_unchanged_or_throw( data, size, active );
        if( auto depth = find_depth_profile( active ) )
            minz_allowed_or_throw( data, size, *depth, _depth_sensor );
        _xu->set_raw( data, size );
    }
    std::vector< uint8_t > get_raw_range() const override { return _xu->get_raw_range(); }
    bool is_read_only() const override { return _xu->is_read_only(); }
    bool is_enabled() const override { return _xu->is_enabled(); }
    const char * get_description() const override { return _xu->get_description(); }

private:
    // FW rejects changing Filter Type or Downscale Ratio while Depth or IR stream, with an obscure error
    void geometry_unchanged_or_throw( const void * data, size_t size, const stream_profiles & active ) const
    {
        bool const streaming = std::any_of( active.begin(), active.end(), []( const std::shared_ptr< stream_profile_interface > & p )
                                            { return p->get_stream_type() == RS2_STREAM_DEPTH || p->get_stream_type() == RS2_STREAM_INFRARED; } );
        if( ! streaming || ! data || size != sizeof( rs2_hdrd_control ) )
            return;
        rs2_hdrd_control next;
        rs2_hdrd_control current;
        std::memcpy( &next, data, sizeof( next ) );
        auto raw = _xu->get_raw();
        if( raw.size() != sizeof( current ) )
            return;
        std::memcpy( &current, raw.data(), sizeof( current ) );
        if( next.filter_type != current.filter_type || next.downscale_ratio != current.downscale_ratio )
            throw wrong_api_call_sequence_exception( "Improved Close Range Depth Filter Type and Downscale Ratio cannot be "
                                                     "changed while Depth or Infrared is streaming" );
    }

    std::shared_ptr< composite_xu_option > _xu;
    const d500_depth_sensor & _depth_sensor;
};


/* static */ const feature_id hdrd_filter_feature::ID = "Improved Close Range filter feature";

hdrd_filter_feature::hdrd_filter_feature( d500_device & owner, d500_depth_sensor & depth_sensor, bool is_mipi )
{
    // Registers the ONE composite option this filter exposes, mirroring temporal_filter_feature.
    // ds::DS5_HKR_HDRD_CONTROL drives the same physical XU control formerly exposed as the
    // scalar "Improved Close Range Depth" option (close_range_xu_option, since removed).
    auto raw_depth_ep = std::dynamic_pointer_cast< uvc_sensor >( depth_sensor.get_raw_sensor() );
    auto option = std::make_shared< minz_option >(
        composite_xu_option::create(
            is_mipi,
            raw_depth_ep,
            ds::depth_xu,
            ds::DS5_HKR_HDRD_CONTROL,
            static_cast< uint32_t >( sizeof( rs2_hdrd_control ) ),
            "Improved Close Range Control (prototype) - use rs2_set/get_composite_option, see rs_hdrd_control.h",
            minz_desc ),
        depth_sensor );
    depth_sensor.add_embedded_filter( std::make_shared< hdrd_embedded_filter >( option, RS2_COMPOSITE_OPTION_HDRD_CONTROL ) );

    // MinZ may have been enabled while the sensor was closed - check it against the Depth profile being opened
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
            minz_allowed_or_throw( raw.data(), raw.size(), *depth, depth_sensor );
        } );
}

feature_id hdrd_filter_feature::get_id() const
{
    return ID;
}


}  // namespace librealsense
