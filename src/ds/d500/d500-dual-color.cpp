// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "d500-dual-color.h"
#include "d500-info.h"
#include "environment.h"
#include "metadata.h"
#include "proc/color-formats-converter.h"  // m420_converter, nv12_converter
#include <src/proc/identity-processing-block.h>
#include <src/uvc-sensor.h>
#include <src/platform/uvc-option.h>
#include <src/metadata-parser.h>
#include <src/ds/ds-color-common.h>
#include <src/ds/ds-timestamp.h>
#include <src/firmware-version.h>
#include <src/backend.h>
#include <src/platform/platform-utils.h>

#include <cstring>

#include <rsutils/type/fourcc.h>
using rs_fourcc = rsutils::type::fourcc;

#include <algorithm>
#include <set>


namespace librealsense
{
    // Image and calibration encodings published by the color pins.
    // The 16-bit raw can have several spellings: RW16 over USB (V4L2 passes it through, WMF normalizes it to BYR2)
    // and BA10 or GR16 over GMSL, depending on the d4xx driver version.
    static const std::map< uint32_t, rs2_format > color_pin_formats = {
        { rs_fourcc( 'M', '4', '2', '0' ), RS2_FORMAT_M420 },
        { rs_fourcc( 'N', 'V', '1', '2' ), RS2_FORMAT_NV12 },
        { rs_fourcc( 'Y', 'U', 'Y', '2' ), RS2_FORMAT_YUYV },
        { rs_fourcc( 'Y', 'U', 'Y', 'V' ), RS2_FORMAT_YUYV },
        { rs_fourcc( 'B', 'A', '1', '0' ), RS2_FORMAT_RAW16 },
        { rs_fourcc( 'R', 'W', '1', '6' ), RS2_FORMAT_RAW16 },
        { rs_fourcc( 'B', 'Y', 'R', '2' ), RS2_FORMAT_RAW16 },
        { rs_fourcc( 'G', 'R', '1', '6' ), RS2_FORMAT_RAW16 }
    };

    d500_dual_color::d500_dual_color( std::shared_ptr< const d500_info > const & dev_info )
        : d500_device( dev_info )
        , device( dev_info )
        , _color_stream_1( new stream( RS2_STREAM_COLOR, 1 ) )
        , _color_stream_2( new stream( RS2_STREAM_COLOR, 2 ) )
    {
        auto & depth_sensor = get_depth_sensor();
        auto raw_depth_sensor = get_raw_depth_sensor();

        // Map the color pins' image and calibration encodings so their raw profiles survive enumeration.
        // They default to infrared; resolve_color_stream below retypes the ones that arrive on a color pin.
        auto & raw_fourcc_to_rs2_format_map = raw_depth_sensor->get_fourcc_to_rs2_format_map();
        auto & raw_fourcc_to_rs2_stream_map = raw_depth_sensor->get_fourcc_to_rs2_stream_map();
        for( auto const & entry : color_pin_formats )
        {
            raw_fourcc_to_rs2_format_map->insert( entry );
            raw_fourcc_to_rs2_stream_map->insert( { entry.first, RS2_STREAM_INFRARED } );
        }

        raw_depth_sensor->set_stream_id_resolver( resolve_color_stream );

        // NV12 registered before M420 so RGB targets resolve to NV12 when present, and to M420 when it is not
        // (converter breaks ties by registration order).
        for( auto target : { RS2_FORMAT_RGB8, RS2_FORMAT_RGBA8, RS2_FORMAT_BGR8, RS2_FORMAT_BGRA8 } )
        {
            depth_sensor.register_processing_block( { { RS2_FORMAT_NV12, RS2_STREAM_COLOR } },
                                                      { { target, RS2_STREAM_COLOR, 1 }, { target, RS2_STREAM_COLOR, 2 } },
                                                      [target]() { return std::make_shared< nv12_converter >( target ); } );
            depth_sensor.register_processing_block( { { RS2_FORMAT_M420, RS2_STREAM_COLOR } },
                                                      { { target, RS2_STREAM_COLOR, 1 }, { target, RS2_STREAM_COLOR, 2 } },
                                                      [target]() { return std::make_shared< m420_converter >( target ); } );
        }

        // Expose native image and calibration encodings as passthrough color profiles.
        for( auto native : { RS2_FORMAT_NV12, RS2_FORMAT_M420, RS2_FORMAT_YUYV, RS2_FORMAT_RAW16 } )
            depth_sensor.register_processing_block( { { native, RS2_STREAM_COLOR } },
                                                      { { native, RS2_STREAM_COLOR, 1 }, { native, RS2_STREAM_COLOR, 2 } },
                                                      []() { return std::make_shared< identity_processing_block >(); } );

        // The color profiles are produced by the depth sensor; hand it the stream objects so it can assign them
        // (matched by stream type + index) when it builds its profiles.
        auto & d500_depth = dynamic_cast< d500_depth_sensor & >( depth_sensor );
        d500_depth.add_stream( _color_stream_1 );
        d500_depth.add_stream( _color_stream_2 );

        add_stream_combination_validator( [this]( const stream_profiles & requests ) { frame_rates_allowed_or_throw( requests ); } );

        register_color_extrinsics();
        register_color_metadata();
        register_ae_policy_option();
        register_color_options( dev_info );
    }

    // The rule below only bites once a color stream shares the depth sensor's imagers.
    static bool color_requested( const stream_profiles & requests )
    {
        return std::any_of( requests.begin(), requests.end(), []( auto & p )
                            { return p && p->get_stream_type() == RS2_STREAM_COLOR; } );
    }

    static bool depth_or_ir_requested( const stream_profiles & requests )
    {
        return std::any_of( requests.begin(), requests.end(), []( auto & p )
                            { return p && ( p->get_stream_type() == RS2_STREAM_DEPTH || p->get_stream_type() == RS2_STREAM_INFRARED ); } );
    }

    // Produce a friendly stream name to the user, e.g. "Depth" / "Color 1"
    static std::string stream_name( const stream_profile_interface & profile )
    {
        std::string name = get_string( profile.get_stream_type() );
        if( profile.get_stream_index() )
            name += " " + std::to_string( profile.get_stream_index() );
        return name;
    }

    // Resolutions may differ freely, but a frame-rate mismatch silently starves the streams - both
    // between the two color pins and between color and depth/IR.
    void d500_dual_color::frame_rates_allowed_or_throw( const stream_profiles & requests ) const
    {
        if( ! color_requested( requests ) )
            return;

        // Depth/IR and color run off the same imagers, so together they cap at 45 FPS - the enumerated
        // 60 and 90 FPS profiles stream only when each runs without the other.
        static const uint32_t MAX_COMBINED_FPS = 45;

        bool const with_depth_or_ir = depth_or_ir_requested( requests );

        stream_profile_interface * first = nullptr;
        for( auto & p : requests )
        {
            if( ! p )
                continue;
            if( with_depth_or_ir && p->get_framerate() > MAX_COMBINED_FPS )
                throw wrong_api_call_sequence_exception( rsutils::string::from()
                    << "Depth/Infrared and Color cannot stream together at 60 or 90 FPS ("
                    << stream_name( *p ) << " requested " << p->get_framerate() << " FPS)" );
            if( ! first )
                first = p.get();
            else if( p->get_framerate() != first->get_framerate() )
                throw wrong_api_call_sequence_exception( rsutils::string::from()
                    << "All streams must share one frame rate while color is streaming ("
                    << stream_name( *first ) << " requested " << first->get_framerate() << " FPS, "
                    << stream_name( *p ) << " requested " << p->get_framerate() << " FPS)" );
        }
    }

    void d500_dual_color::register_ae_policy_option()
    {
        if( _fw_version < firmware_version( "7.58.45946.14332" ) )
            return;

        // IR imagers feed both RGB and depth at once, so AE has to be arbitrated between the IPU (color priority) and the SDP (depth priority).
        auto options_map = std::map< float, std::string >{ { static_cast< float >( RS2_COLORED_IR_AUTO_EXPOSURE_AUTO ), "Auto" },
                                                           { static_cast< float >( RS2_COLORED_IR_AUTO_EXPOSURE_DEPTH_PRIORITY ), "Depth Priority" },
                                                           { static_cast< float >( RS2_COLORED_IR_AUTO_EXPOSURE_COLOR_PRIORITY ), "Color Priority" },
                                                           { static_cast< float >( RS2_COLORED_IR_AUTO_EXPOSURE_HYBRID ), "Hybrid" } };
        get_depth_sensor().register_option( RS2_OPTION_DEPTH_AUTO_EXPOSURE_MODE,
                                            std::make_shared< uvc_xu_option< uint8_t > >( get_raw_depth_sensor(),
                                                                                          ds::depth_xu,
                                                                                          ds::d500_xu_id::COLORED_IR_AE_POLICY,
                                                                                          "Auto exposure policy for sensor with both color and depth streams",
                                                                                          options_map,
                                                                                          false ) ); // Not settable while streaming
    }

    // D585 2C dual-color topology: on the depth-function UVC interface, the RGB streams' PU chain
    // is UVC entity 0x07 and, on Windows, KS topology node 6.
    constexpr uint8_t D585_2C_RGB_PU_UNIT_ID  = 0x07;
    constexpr int     D585_2C_RGB_PU_KS_NODE = 6;

    // The PU does not publish the same set on every platform - e.g. backlight compensation missing over GMSL.
    // An unpublished control reads back as a degenerate range (V4L2) or throws (WMF).
    static bool is_control_published( const option & opt, rs2_option id )
    {
        try
        {
            auto range = opt.get_range();
            return ! ( range.min == 0.f && range.max == 0.f && range.def == 0.f && range.step == 0.f );
        }
        catch( const std::exception & e )
        {
            LOG_DEBUG( "Dual-color RGB control " << id << " not published: " << e.what() );
            return false;
        }
    }

    void d500_dual_color::register_color_options( std::shared_ptr< const d500_info > const & dev_info )
    {
        // Route RGB controls via the RGB PU: node-based routing on WMF, a dedicated raw sensor on V4L2.
        static const platform::processing_unit rgb_pu = { 0, D585_2C_RGB_PU_UNIT_ID, D585_2C_RGB_PU_KS_NODE };

        auto raw_ep = pick_rgb_pu_raw_endpoint( dev_info, rgb_pu );
        if( ! raw_ep )
            return;  // discovery failed on this backend; leave the options unregistered rather than expose broken ones

        auto & color_ep = get_depth_sensor();
        auto make_rgb_option = [raw_ep](rs2_option option)
        {
            return std::make_shared<uvc_pu_option>(raw_ep, option, rgb_pu);
        };

        auto register_if_published = [&color_ep]( rs2_option id, std::shared_ptr< option > opt )
        {
            // Registering unpublished would only add a dead control, verify befor registering.
            if( ! is_control_published( *opt, id ) )
                return;
            color_ep.register_option( id, opt );
        };

        for( auto id : { RS2_OPTION_BACKLIGHT_COMPENSATION, RS2_OPTION_BRIGHTNESS, RS2_OPTION_CONTRAST,
                         RS2_OPTION_SATURATION, RS2_OPTION_GAMMA, RS2_OPTION_SHARPNESS, RS2_OPTION_HUE } )
            register_if_published( id, make_rgb_option( id ) );

        std::map<float, std::string> power_line_descriptions = {
            { 0.f, "Disabled" },
            { 1.f, "50Hz" },
            { 2.f, "60Hz" }
        };
        register_if_published( RS2_OPTION_POWER_LINE_FREQUENCY,
                               std::make_shared<uvc_pu_option>(raw_ep,
                                                               RS2_OPTION_POWER_LINE_FREQUENCY,
                                                               rgb_pu,
                                                               power_line_descriptions));

        auto white_balance = make_rgb_option(RS2_OPTION_WHITE_BALANCE);
        if( is_control_published( *white_balance, RS2_OPTION_WHITE_BALANCE ) )
        {
            // Without auto white balance the manual control is still needed, just not wrapped in the auto-disabling proxy.
            auto auto_white_balance = make_rgb_option( RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE );
            if( is_control_published( *auto_white_balance, RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE ) )
            {
                color_ep.register_option( RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE, auto_white_balance );
                color_ep.register_option( RS2_OPTION_WHITE_BALANCE,
                                          std::make_shared< auto_disabling_control >( white_balance, auto_white_balance ) );
            }
            else
                color_ep.register_option( RS2_OPTION_WHITE_BALANCE, white_balance );
        }
    }

    std::shared_ptr< uvc_sensor > d500_dual_color::pick_rgb_pu_raw_endpoint(
        std::shared_ptr< const d500_info > const & dev_info,
        const platform::processing_unit & rgb_pu )
    {
#if defined(_WIN32)
        // WMF: any depth-function pin resolves to the same IMFMediaSource and node routing picks the PU.
        return get_raw_depth_sensor();
#else
        // V4L2: each /dev/videoN's fd only exposes its own PU chain's CIDs; probe MI-0 siblings to find
        // the one whose fd hosts the RGB PU. Skip the depth pin - the multi_pins depth sensor already
        // holds it and opening a second fd there just adds startup latency.
        std::string depth_path;
        try { depth_path = get_depth_sensor().get_info( RS2_CAMERA_INFO_PHYSICAL_PORT ); }
        catch( ... ) {}

        for( auto & info : filter_by_mi( dev_info->get_group().uvc_devices, 0 ) )
        {
            if( ! depth_path.empty() && info.device_path == depth_path )
                continue;

            std::shared_ptr< platform::uvc_device > uvc_dev;
            try { uvc_dev = get_backend()->create_uvc_device( info ); }
            catch( ... ) { continue; }
            if( ! uvc_dev )
                continue;

            auto candidate = std::make_shared< uvc_sensor >(
                "Raw RGB PU Sensor", uvc_dev,
                std::make_unique< ds_timestamp_reader >(), this );
            try
            {
                auto r = candidate->invoke_powered( [ & rgb_pu ]( platform::uvc_device & dev )
                {
                    return dev.get_pu_range( rgb_pu, RS2_OPTION_BRIGHTNESS );
                } );
                // v4l_uvc_device::get_pu_range returns an all-zero range for unknown CIDs instead of throwing - reject that fallback shape.
                if( r.max.size() >= sizeof( int32_t ) && r.min.size() >= sizeof( int32_t ) )
                {
                    int32_t r_min = 0, r_max = 0;
                    std::memcpy( &r_min, r.min.data(), sizeof( int32_t ) );
                    std::memcpy( &r_max, r.max.data(), sizeof( int32_t ) );
                    if( r_min != 0 || r_max != 0 )
                    {
                        _raw_rgb_ep = candidate;
                        return _raw_rgb_ep;
                    }
                }
            }
            catch( ... )
            {
                // this pin doesn't recognize the CID - try the next
            }
        }

        LOG_WARNING( "Dual-color RGB PU pin not found on MI 0; RGB controls will not be registered" );
        return nullptr;
#endif
    }

    void d500_dual_color::register_color_metadata()
    {
        auto & depth_sensor = get_depth_sensor();

        // Color frames arrive on the depth sensor but carry the RGB metadata layout (md_rgb_mode), distinct from
        // the depth/IR layout already registered. Register common fields with RGB layout offsets.
        auto md_prop_offset = metadata_raw_mode_offset + offsetof( md_rgb_mode, rgb_mode ) + offsetof( md_rgb_normal_mode, intel_rgb_control );
        depth_sensor.register_metadata( RS2_FRAME_METADATA_AUTO_EXPOSURE,
            make_attribute_parser( &md_rgb_control::ae_mode, md_rgb_control_attributes::ae_mode_attribute, md_prop_offset,
                []( rs2_metadata_type param ) { return ( param != 1 ); } ) ); // OFF value via UVC is 1 (ON is 8)

        auto md_prop_offset_stats = metadata_raw_mode_offset + offsetof( md_rgb_mode, rgb_mode ) + offsetof( md_rgb_normal_mode, intel_capture_stats );
        depth_sensor.register_metadata( RS2_FRAME_METADATA_FRAME_TIMESTAMP,
            make_attribute_parser( &md_capture_stats::hw_timestamp, md_capture_stat_attributes::hw_timestamp_attribute, md_prop_offset_stats ) );

        auto md_prop_offset_timing = metadata_raw_mode_offset + offsetof( md_rgb_mode, rgb_mode ) + offsetof( md_rgb_normal_mode, intel_capture_timing );
        depth_sensor.register_metadata( RS2_FRAME_METADATA_SENSOR_TIMESTAMP,
            make_rs400_sensor_ts_parser( make_attribute_parser( &md_capture_stats::hw_timestamp, md_capture_stat_attributes::hw_timestamp_attribute, md_prop_offset_stats ),
                make_attribute_parser( &md_capture_timing::sensor_timestamp, md_capture_timing_attributes::sensor_timestamp_attribute, md_prop_offset_timing ) ) );

        // The remaining RGB control/stats attributes (gain, exposure, white balance, brightness, ...) are common to
        // all DS color sensors - reuse the shared registration on the depth sensor.
        ds_color_common color_md( get_raw_depth_sensor(), depth_sensor, _fw_version, _hw_monitor, this );
        color_md.register_metadata();
    }

    void d500_dual_color::register_color_extrinsics()
    {
        // Each RGB stream comes from the same physical imager as its matching infrared stream, so it shares
        // that stream's extrinsics.
        auto & graph = environment::get_instance().get_extrinsics_graph();
        graph.register_same_extrinsics( *_left_ir_stream, *_color_stream_1 );
        graph.register_same_extrinsics( *_right_ir_stream, *_color_stream_2 );
        register_stream_to_extrinsic_group( *_color_stream_1, 0 );
        register_stream_to_extrinsic_group( *_color_stream_2, 0 );
    }

    // Stream-id resolver: the two RGB cameras arrive on separate pins (USB endpoints), each advertising identical
    // {w,h,fps,format} color profiles in every published encoding (NV12/M420/YUY2). Map the color pins to Color 1 /
    // Color 2 in descending pin order, so the color indexes line up with the infrared 1 / 2 imagers (the lowest
    // color pin is co-located with the right / infrared-2 imager).
    void d500_dual_color::resolve_color_stream( const std::vector< platform::stream_profile > & all,
                                              const platform::stream_profile & p, rs2_stream & type, int & index )
    {
        if( ! color_pin_formats.count( p.format ) )
            return;

        if( ! is_color_pin( all, p.pin_index ) )
            return;  // stereo-imager color format stays infrared - no color converter, so it is not exposed

        // Rank this pin among all color pins by ascending pin order.
        std::set< uint32_t > pins, color_pins;
        for( auto & q : all )
            pins.insert( q.pin_index );
        for( auto pin : pins )
            if( is_color_pin( all, pin ) )
                color_pins.insert( pin );

        int rank = 0;
        for( auto cp : color_pins )
        {
            if( cp == p.pin_index )
                break;
            ++rank;
        }

        // Assign in descending order so the highest pin -> Color 1, matching infrared 1 / 2.
        type = RS2_STREAM_COLOR;
        index = static_cast< int >( color_pins.size() ) - rank;
    }

    // Identify a color pin: it advertises the native color format (M420 or NV12) paired with a YUY2/YUYV
    // companion. The infrared pin also advertises the native color format (colored infrared) but pairs it with
    // UYVY/Y8I, not YUY2 - so the companion distinguishes color pins from the infrared pin. Holds across SKUs.
    bool d500_dual_color::is_color_pin( const std::vector< platform::stream_profile > & all, uint32_t pin )
    {
        bool color = false, yuy2 = false;
        for( auto & q : all )
        {
            if( q.pin_index != pin )
                continue;
            if( q.format == rs_fourcc( 'M', '4', '2', '0' ) || q.format == rs_fourcc( 'N', 'V', '1', '2' ) )
                color = true;
            // For the same format Windows exposes YUY2, linux exposes identical YUYV
            if( q.format == rs_fourcc( 'Y', 'U', 'Y', '2' ) || q.format == rs_fourcc( 'Y', 'U', 'Y', 'V' ) )
                yuy2 = true;
        }
        return color && yuy2;
    }
}
