// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "d500-dual-color.h"
#include "d500-info.h"
#include "d500-stream-group-transaction.h"
#include "environment.h"
#include "metadata.h"
#include "proc/color-formats-converter.h"  // m420_converter, nv12_converter
#include <src/proc/identity-processing-block.h>
#include <src/uvc-sensor.h>
#include <src/metadata-parser.h>
#include <src/ds/ds-color-common.h>

#include <rsutils/type/fourcc.h>
using rs_fourcc = rsutils::type::fourcc;

#include <set>
#include <map>
#include <chrono>
#include <thread>


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
    }

    void d500_dual_color::register_stream_group_transaction()
    {
        auto raw_sensor = get_raw_depth_sensor();
        auto block_streaming = [raw_sensor]( std::string const & reason ) {
            LOG_ERROR( reason );
            raw_sensor->append_on_open(
                [reason]( std::vector< platform::stream_profile > ) {
                    throw wrong_api_call_sequence_exception( reason );
                } );
        };

        d500_stream_group_transaction transaction( _hw_monitor );
        d500_stream_group_status device_status = {};
        std::string query_error;
        bool protocol_supported = false;
        for( unsigned attempt = 0; attempt < 3; ++attempt )
        {
            try
            {
                // Only an explicit unsupported-opcode response selects the
                // legacy admission-window flow. Transport, readiness, and
                // protocol errors are retried and then fail closed.
                if( ! transaction.query_if_supported( device_status ) )
                {
                    LOG_INFO( "D500 stream-group transaction is unavailable; using legacy flow" );
                    return;
                }
                protocol_supported = true;
                break;
            }
            catch( std::exception const & e )
            {
                query_error = e.what();
            }
            catch( ... )
            {
                query_error = "unknown error";
            }
            if( attempt != 2 )
                std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
        }

        if( ! protocol_supported )
        {
            block_streaming(
                "D500 stream-group capability query failed after 3 attempts; "
                "reconnect or hardware-reset the device before streaming: "
                + query_error );
            return;
        }

        // A new SDK process has no C++ record of a transaction owned by a
        // process that exited abruptly. Reconcile the device-owned generation
        // before installing the runtime hook.
        if( device_status.transaction_id )
        {
            _next_stream_group_transaction_id = device_status.transaction_id;
            _active_stream_group_transaction_id = device_status.transaction_id;
            if( device_status.started_mask )
            {
                block_streaming(
                    "D500 has a stale started stream-group transaction; "
                    "hardware-reset or power-cycle the device before streaming" );
                return;
            }
            cancel_active_stream_group();
            if( _active_stream_group_transaction_id )
            {
                block_streaming(
                    "D500 stale stream-group transaction did not reach IDLE; "
                    "hardware-reset or power-cycle the device before streaming" );
                return;
            }
        }

        std::weak_ptr< uvc_sensor > weak_sensor = raw_sensor;
        raw_sensor->append_on_open(
            [this, weak_sensor]( std::vector< platform::stream_profile > configurations ) {
                auto sensor = weak_sensor.lock();
                if( ! sensor )
                    throw wrong_api_call_sequence_exception( "D500 stream-group sensor is unavailable" );

                // Clear a transaction left by an interrupted prior open before
                // allocating the next nonzero generation.
                cancel_active_stream_group();
                if( _active_stream_group_transaction_id )
                    throw wrong_api_call_sequence_exception(
                        "previous D500 stream-group transaction did not settle" );
                if( ++_next_stream_group_transaction_id == 0 )
                    ++_next_stream_group_transaction_id;
                _active_stream_group_transaction_id =
                    _next_stream_group_transaction_id;

                auto const & advertised = sensor->get_advertised_profiles();
                std::set< uint32_t > color_pins;
                for( auto const & profile : advertised )
                    if( is_color_pin( advertised, profile.pin_index ) )
                        color_pins.insert( profile.pin_index );

                std::map< d500_stream_group_branch, d500_stream_group_profile > entries;
                for( auto const & profile : configurations )
                {
                    d500_stream_group_branch branch;
                    if( profile.format == rs_fourcc( 'Z', '1', '6', ' ' ) )
                        branch = d500_stream_group_branch::depth;
                    else if( profile.format == rs_fourcc( 'Y', '8', 'I', ' ' )
                             || profile.format == rs_fourcc( 'G', 'R', 'E', 'Y' ) )
                        branch = d500_stream_group_branch::infrared;
                    else if( color_pins.size() == 2
                             && color_pins.count( profile.pin_index ) )
                        branch = profile.pin_index == *color_pins.begin()
                            ? d500_stream_group_branch::color_left
                            : d500_stream_group_branch::color_right;
                    else
                        throw invalid_value_exception(
                            "cannot map raw profile to a stream-group branch" );

                    if( ! entries.emplace(
                            branch,
                            d500_stream_group_profile{
                                branch,
                                profile.format,
                                static_cast< uint16_t >( profile.width ),
                                static_cast< uint16_t >( profile.height ),
                                static_cast< uint16_t >( profile.fps ) } )
                              .second )
                        throw invalid_value_exception(
                            "multiple raw profiles map to one stream-group branch" );
                }

                std::vector< d500_stream_group_profile > manifest;
                for( auto const & entry : entries )
                    manifest.push_back( entry.second );
                d500_stream_group_transaction transaction( _hw_monitor );
                transaction.prepare( _active_stream_group_transaction_id, manifest );

                // PREPARE acceptance is the only synchronous barrier on the
                // normal open path. Let uvc_sensor::open() continue directly
                // to the existing backend stream_on() call so host UVC setup
                // overlaps firmware graph construction. Firmware processes
                // PREPARE and all D585 2C OPEN/START events on one FIFO queue,
                // therefore the graph is published before a queued START can
                // activate its branch. QUERY remains available for capability,
                // stale-process reconciliation, rollback, and diagnostics.
            } );
        raw_sensor->register_on_open_error(
            [this]() { cancel_active_stream_group(); } );
    }

    void d500_dual_color::cancel_active_stream_group() noexcept
    {
        auto const transaction_id = _active_stream_group_transaction_id;
        if( ! transaction_id )
            return;

        try
        {
            d500_stream_group_transaction transaction( _hw_monitor );
            auto const deadline = std::chrono::steady_clock::now()
                + std::chrono::seconds( 3 );
            bool cancel_sent = false;
            bool settled = false;
            while( std::chrono::steady_clock::now() < deadline )
            {
                auto const status = transaction.query( transaction_id );
                if( status.state
                        == static_cast< uint8_t >( d500_stream_group_state::idle )
                    || status.transaction_id == 0 )
                {
                    settled = true;
                    break;
                }
                if( status.transaction_id != transaction_id )
                    break;
                if( ! cancel_sent && status.started_mask == 0 )
                {
                    transaction.cancel( transaction_id );
                    cancel_sent = true;
                }
                std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
            }
            if( settled )
                _active_stream_group_transaction_id = 0;
            else
                LOG_WARNING( "D500 stream-group rollback did not reach IDLE" );
        }
        catch( std::exception const & e )
        {
            LOG_WARNING( "D500 stream-group rollback failed: " << e.what() );
        }
        catch( ... )
        {
            LOG_WARNING( "D500 stream-group rollback failed" );
        }
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
