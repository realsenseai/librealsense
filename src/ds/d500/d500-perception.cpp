// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "d500-perception.h"

#include "ds/ds-private.h"
#include "d500-info.h"
#include "ds/ds-timestamp.h"
#include <src/global_timestamp_reader.h>
#include <src/backend.h>
#include <src/metadata-parser.h>
#include <src/proc/processing-blocks-factory.h>
#include "stream.h"
#include "platform/platform-utils.h"
#include <src/platform/uvc-option.h>

#include <rsutils/type/fourcc.h>
using rs_fourcc = rsutils::type::fourcc;

#include <map>
#include <vector>

namespace librealsense
{
    // The stream as the GMSL IR node multiplexes it: MUX stream id, metadata id (routing, and its own optional
    // block of 83+ bytes) and the ODET blob padded to 2347 bytes; 3 KB, so reserved on every capture
    const platform::muxed_stream od_muxed_stream
        = { "Object Detection",
            6,
            { uint32_t( md_type::META_DATA_INTEL_OBJECT_DETECTION_ID ), uint32_t( md_type::META_DATA_INTEL_OBJECT_DETECTION_ID ), 83, false },
            { 2347, 1, rs_fourcc( 'G', 'R', 'E', 'Y' ), 2347, true } };

    d500_perception::d500_perception( std::shared_ptr< const d500_info > const & dev_info )
        : device( dev_info )
        , d500_device( dev_info )
        , d500_depth_mapping( dev_info )
        , _object_detection_stream( new stream( RS2_STREAM_OBJECT_DETECTION ) )
    {
        std::string physical_port;
        auto od_dev = find_od_device( dev_info, physical_port );
        // A muxed stream has no controls of its own; its XU is the depth node's
        std::shared_ptr< uvc_sensor > xu_host = _is_mipi_device ? get_raw_depth_sensor() : nullptr;

        // One device per interface or MUX stream, so each opens and streams on its own; XUs go to their interface
        std::vector< std::shared_ptr< platform::uvc_device > > devs;
        std::map< uint8_t, size_t > xu_owners;
        if( od_dev )
        {
            if( ! xu_host )
                xu_owners[ds::inference_xu.unit] = devs.size();
            devs.push_back( od_dev );
        }
        if( ! _mapping_devs.empty() )
        {
            xu_owners[ds::mapping_xu.unit] = devs.size();
            devs.insert( devs.end(), _mapping_devs.begin(), _mapping_devs.end() );
            if( ! od_dev )
                physical_port = _mapping_port;
        }
        if( devs.empty() )
            return;
        auto pins = std::make_shared< platform::multi_pins_uvc_device >( devs, xu_owners );

        auto od_ep = create_object_detection_device( dev_info->get_context(), pins, physical_port, od_dev != nullptr, xu_host );
        _object_detection_device_idx = add_sensor( od_ep );
        if( ! _mapping_devs.empty() )
            _depth_mapping_idx = _object_detection_device_idx;
    }

    std::shared_ptr< platform::uvc_device > d500_perception::find_od_device( std::shared_ptr< const d500_info > const & dev_info,
                                                                             std::string & physical_port )
    {
        // Over GMSL the stream is multiplexed on the depth sensor's IR node, and the camera takes it by rate alone;
        // over USB it has its own interface. Sensors outlive the device's HW monitor, so a close at teardown skips
        // the camera command: the camera drops the request itself once the capture closes.
        auto od_dev = get_raw_depth_sensor()->get_uvc_device()->create_muxed_stream_device(
            od_muxed_stream,
            [this, alive = _is_alive]( uint32_t stream_id, bool on, const platform::stream_profile & p )
            {
                if( *alive )
                    enable_muxed_stream( stream_id, on, 0, 0, p.fps );
            } );
        if( od_dev )
        {
            physical_port = od_dev->get_device_location();
            LOG_DEBUG( "Object Detection is multiplexed on " << physical_port );
        }
        else
        {
            // OD VideoControl is MI 9 on all currently supported layouts (D555/D555e,
            // D585 0x0C08 and legacy 0x0B6A) - confirmed with the HKR firmware team.
            constexpr uint32_t od_control_mi = 9;
            auto od_devs_info = filter_by_mi( dev_info->get_group().uvc_devices, od_control_mi );

            // Skip if the device does not expose the stream; the rest of the device enumerates normally.
            if( od_devs_info.empty() )
            {
                LOG_WARNING( "Object detection interface not present - Object Detection not offered" );
                return nullptr;
            }
            if( od_devs_info.size() != 1 )
            {
                LOG_WARNING( "Expected a single object detection interface, found " << od_devs_info.size()
                                                                                    << " - Object Detection not offered" );
                return nullptr;
            }
            od_dev = get_backend()->create_uvc_device( od_devs_info.front() );
            physical_port = od_devs_info.front().device_path;
        }
        return od_dev;
    }

    std::shared_ptr< synthetic_sensor >
    d500_perception::create_object_detection_device( std::shared_ptr< context > ctx,
                                                     std::shared_ptr< platform::multi_pins_uvc_device > od_device,
                                                     const std::string & physical_port,
                                                     bool with_object_detection,
                                                     std::shared_ptr< uvc_sensor > xu_host )
    {
        auto enable_global_time_option = std::shared_ptr< global_time_option >( new global_time_option() );

        std::unique_ptr< frame_timestamp_reader > ts_reader_backup( new ds_timestamp_reader() );
        // Mapping frames carry their capture time in their metadata; Object Detection frames fall back to the backup
        std::unique_ptr< frame_timestamp_reader > ts_reader_metadata( new ds_timestamp_reader_from_metadata_depth_mapping( std::move( ts_reader_backup ) ) );

        // TODO - check the whole timestamp reader chain - do we get the timestamp from UVC header into the metadata? do
        // we need to convert it from ds timestamp to global timestamp? etc
        auto raw_od_ep = std::make_shared< uvc_sensor >(
            "Raw Perception Device",
            od_device,
            std::unique_ptr< frame_timestamp_reader >( new global_timestamp_reader( std::move( ts_reader_metadata ),
                                                                                    _tf_keeper,
                                                                                    enable_global_time_option ) ),
            this );

        auto od_ep = std::make_shared< d500_perception_sensor >( this, raw_od_ep, _object_detection_stream );

        if( ! xu_host )
            xu_host = raw_od_ep;
        if( with_object_detection )
            xu_host->register_xu( ds::inference_xu ); // ensure the XU is initialized every time we power the camera

        od_ep->register_option( RS2_OPTION_GLOBAL_TIME_ENABLED, enable_global_time_option );

        if( with_object_detection && _fw_version >= firmware_version( "7.58.40802.12738" ) )
        {
            auto detection_distance = std::make_shared< uvc_xu_option< uint8_t > >(xu_host,
                                                                                   ds::inference_xu,
                                                                                   ds::d500_xu_id::DETECTION_DISTANCE,
                                                                                   "Enable firmware distance calculation for detections" );
            od_ep->register_option( RS2_OPTION_DETECTION_DISTANCE, detection_distance );
        }

        od_ep->register_info( RS2_CAMERA_INFO_PHYSICAL_PORT, physical_port );

        if( _mapping_devs.empty() )  // else register_metadata() below registers it
            raw_od_ep->register_metadata( RS2_FRAME_METADATA_FRAME_TIMESTAMP,
                                          make_uvc_header_parser( &platform::uvc_header::timestamp ) );
        processing_block_factory od_pbf = { { { RS2_FORMAT_Y8, RS2_STREAM_OBJECT_DETECTION } },
                                            { { RS2_FORMAT_Y8, RS2_STREAM_OBJECT_DETECTION } },
                                            []() { return std::make_shared< identity_processing_block >(); } };
        od_ep->register_processing_block( od_pbf );

        // Object Detection and Occupancy are both GREY; Object Detection is the first sub-device's
        raw_od_ep->set_stream_id_resolver( [with_object_detection, pins = od_device.get()](
            const std::vector< platform::stream_profile > &, const platform::stream_profile & p, rs2_stream & type, std::vector< int > & )
        {
            auto pin = p.pin_index;
            if( with_object_detection && pins->decode_pin( pin ) == 0 )
                type = RS2_STREAM_OBJECT_DETECTION;
            else if( is_pcl_transport( p ) )
                type = RS2_STREAM_POINT_CLOUD;
        } );
        if( ! _mapping_devs.empty() )
        {
            raw_od_ep->enable_incremental_streams();
            register_extrinsics();
            register_options( od_ep, raw_od_ep );
            register_metadata( raw_od_ep );
            register_processing_blocks( od_ep );
            _depth_mapping_active = true;
        }

        return od_ep;
    }

    d500_perception_sensor::d500_perception_sensor( d500_perception * owner,
                                                    std::shared_ptr< uvc_sensor > uvc_sensor,
                                                    std::shared_ptr< stream_interface > object_detection_stream )
        : d500_depth_mapping_sensor( owner, uvc_sensor, mapping_fourcc_to_rs2_format, mapping_fourcc_to_rs2_stream,
                                     perception_sensor::SENSOR_NAME )
        , _object_detection_stream( object_detection_stream )
    {
    }

    void d500_perception_sensor::open( const stream_profiles & requests )
    {
        auto & depth_sensor = _owner->get_depth_sensor();
        if( find_stream( requests, RS2_STREAM_OBJECT_DETECTION )
            && depth_sensor.supports_option( RS2_OPTION_DUAL_RGB_RECTIFICATION )
            && depth_sensor.get_option( RS2_OPTION_DUAL_RGB_RECTIFICATION ).query() != 0.f )
            throw wrong_api_call_sequence_exception( "Object detection is not supported while dual RGB rectification is on" );

        // The checks, the selection and the commit inside open() are one transaction, also when a Mapping stream joins live
        std::lock_guard< std::recursive_mutex > lock( _synthetic_configure_lock );
        auto const is_mapping = []( const std::shared_ptr< stream_profile_interface > & p ) { return p->get_stream_type() != RS2_STREAM_OBJECT_DETECTION; };
        auto const active = get_active_streams();
        if( std::count_if( requests.begin(), requests.end(), is_mapping ) + std::count_if( active.begin(), active.end(), is_mapping ) > 1 )
            throw invalid_value_exception( "The Perception sensor streams one of Point Cloud, Labeled Point Cloud, or Occupancy at a time" );
        _owner->colors_with_mapping_or_throw( depth_sensor.get_active_streams(), requests );
        auto const mapping = std::find_if( requests.begin(), requests.end(), is_mapping );
        if( mapping != requests.end() )
            _owner->select_mapping_stream( { *mapping } );
        synthetic_sensor::open( requests );
    }

    void d500_perception_sensor::start( rs2_frame_callback_sptr callback )
    {
        synthetic_sensor::start( callback );
    }

    void d500_perception_sensor::stop()
    {
        synthetic_sensor::stop();
    }

    std::string d500_perception_sensor::get_depth_mapping_params() const
    {
        return _owner->get_depth_mapping_params();
    }

    void d500_perception_sensor::set_depth_mapping_params( const std::string & params_json_str ) const
    {
        _owner->set_depth_mapping_params( params_json_str );
    }

    stream_profiles d500_perception_sensor::init_stream_profiles()
    {
        // No extrinsics registered for this stream: FW reports detections as color-frame pixels,
        // so it has no pose of its own and nothing transforms through it.
        auto results = d500_depth_mapping_sensor::init_stream_profiles();
        for( auto p : results )
        {
            if( p->get_stream_type() == RS2_STREAM_OBJECT_DETECTION )
            {
                assign_stream( _object_detection_stream, p );
                p->set_name( perception_sensor::STREAM_NAME );
            }
        }
        return results;
    }
}
