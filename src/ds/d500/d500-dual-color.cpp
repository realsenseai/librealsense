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
#include <src/firmware-version.h>

#include <rsutils/type/fourcc.h>
using rs_fourcc = rsutils::type::fourcc;

#include <set>


namespace librealsense
{
    d500_dual_color::d500_dual_color( std::shared_ptr< const d500_info > const & dev_info )
        : d500_device( dev_info )
        , device( dev_info )
        , _color_stream_1( new stream( RS2_STREAM_COLOR, 1 ) )
        , _color_stream_2( new stream( RS2_STREAM_COLOR, 2 ) )
    {
        auto & depth_sensor = get_depth_sensor();
        auto raw_depth_sensor = get_raw_depth_sensor();

        // The color pins publish the RGB image in several encodings at once: NV12 (current firmware) and/or
        // legacy M420, plus YUY2. Map all three so their raw profiles survive enumeration.
        auto & raw_fourcc_to_rs2_format_map = raw_depth_sensor->get_fourcc_to_rs2_format_map();
        raw_fourcc_to_rs2_format_map->insert( { rs_fourcc( 'M', '4', '2', '0' ), RS2_FORMAT_M420 } );
        raw_fourcc_to_rs2_format_map->insert( { rs_fourcc( 'N', 'V', '1', '2' ), RS2_FORMAT_NV12 } );
        raw_fourcc_to_rs2_format_map->insert( { rs_fourcc( 'Y', 'U', 'Y', '2' ), RS2_FORMAT_YUYV } );
        raw_fourcc_to_rs2_format_map->insert( { rs_fourcc( 'Y', 'U', 'Y', 'V' ), RS2_FORMAT_YUYV } );
        auto & raw_fourcc_to_rs2_stream_map = raw_depth_sensor->get_fourcc_to_rs2_stream_map();
        raw_fourcc_to_rs2_stream_map->insert( { rs_fourcc( 'M', '4', '2', '0' ), RS2_STREAM_INFRARED } );
        raw_fourcc_to_rs2_stream_map->insert( { rs_fourcc( 'N', 'V', '1', '2' ), RS2_STREAM_INFRARED } );
        raw_fourcc_to_rs2_stream_map->insert( { rs_fourcc( 'Y', 'U', 'Y', '2' ), RS2_STREAM_INFRARED } );
        raw_fourcc_to_rs2_stream_map->insert( { rs_fourcc( 'Y', 'U', 'Y', 'V' ), RS2_STREAM_INFRARED } );

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

        // Expose each raw encoding (NV12, M420, YUY2) as a passthrough color profile so it can be streamed as-is.
        for( auto native : { RS2_FORMAT_NV12, RS2_FORMAT_M420, RS2_FORMAT_YUYV } )
            depth_sensor.register_processing_block( { { native, RS2_STREAM_COLOR } },
                                                      { { native, RS2_STREAM_COLOR, 1 }, { native, RS2_STREAM_COLOR, 2 } },
                                                      []() { return std::make_shared< identity_processing_block >(); } );

        // The color profiles are produced by the depth sensor; hand it the stream objects so it can assign them
        // (matched by stream type + index) when it builds its profiles.
        auto & d500_depth = dynamic_cast< d500_depth_sensor & >( depth_sensor );
        d500_depth.add_stream( _color_stream_1 );
        d500_depth.add_stream( _color_stream_2 );

        register_color_extrinsics();
        register_color_metadata();
        register_ae_policy_option();
#if defined(_WIN32)
        register_color_options();
#endif
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

#if defined(_WIN32)
    void d500_dual_color::register_color_options()
    {
        // The dual-color UVC function contains Depth and RGB processing units. Windows' aggregate
        // IAMVideoProcAmp binds to the first (Depth) PU, so RGB controls must address its topology node directly.
        static const platform::processing_unit rgb_pu = { 0, 0x07, 6 };

        auto & color_ep = get_depth_sensor();
        auto raw_ep = get_raw_depth_sensor();
        auto make_rgb_option = [raw_ep](rs2_option option)
        {
            return std::make_shared<uvc_pu_option>(raw_ep, option, rgb_pu);
        };

        color_ep.register_option(RS2_OPTION_BACKLIGHT_COMPENSATION,
                                 make_rgb_option(RS2_OPTION_BACKLIGHT_COMPENSATION));
        color_ep.register_option(RS2_OPTION_BRIGHTNESS, make_rgb_option(RS2_OPTION_BRIGHTNESS));
        color_ep.register_option(RS2_OPTION_CONTRAST, make_rgb_option(RS2_OPTION_CONTRAST));
        color_ep.register_option(RS2_OPTION_SATURATION, make_rgb_option(RS2_OPTION_SATURATION));
        color_ep.register_option(RS2_OPTION_GAMMA, make_rgb_option(RS2_OPTION_GAMMA));
        color_ep.register_option(RS2_OPTION_SHARPNESS, make_rgb_option(RS2_OPTION_SHARPNESS));
        color_ep.register_option(RS2_OPTION_HUE, make_rgb_option(RS2_OPTION_HUE));

        std::map<float, std::string> power_line_descriptions = {
            { 0.f, "Disabled" },
            { 1.f, "50Hz" },
            { 2.f, "60Hz" }
        };
        color_ep.register_option(
            RS2_OPTION_POWER_LINE_FREQUENCY,
            std::make_shared<uvc_pu_option>(raw_ep,
                                            RS2_OPTION_POWER_LINE_FREQUENCY,
                                            rgb_pu,
                                            power_line_descriptions));

        auto white_balance = make_rgb_option(RS2_OPTION_WHITE_BALANCE);
        auto auto_white_balance = make_rgb_option(RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE);
        color_ep.register_option(RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE, auto_white_balance);
        color_ep.register_option(
            RS2_OPTION_WHITE_BALANCE,
            std::make_shared<auto_disabling_control>(white_balance, auto_white_balance));
    }
#endif

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
        if( p.format != rs_fourcc( 'M', '4', '2', '0' ) && p.format != rs_fourcc( 'N', 'V', '1', '2' )
            && p.format != rs_fourcc( 'Y', 'U', 'Y', '2' ) && p.format != rs_fourcc( 'Y', 'U', 'Y', 'V' ) )
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
