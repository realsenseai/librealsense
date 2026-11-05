// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "d500-object-detection.h"

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
    // 'Y8  ' is not listed here: uvc-types.h's fourcc_map already normalizes it to 'GREY'
    // in the mf-uvc and uvc-device backends before it ever reaches this map.
    const std::map< uint32_t, rs2_format > od_fourcc_to_rs2_format = {
        { rs_fourcc( 'G', 'R', 'E', 'Y' ), RS2_FORMAT_Y8 },
    };
    const std::map< uint32_t, rs2_stream > od_fourcc_to_rs2_stream = {
        { rs_fourcc( 'G', 'R', 'E', 'Y' ), RS2_STREAM_OBJECT_DETECTION },
    };

    // The stream as the GMSL IR node multiplexes it: MUX stream id, metadata id (routing, and its own optional
    // block of 83+ bytes) and the ODET blob padded to 2347 bytes; 3 KB, so reserved on every capture
    const platform::muxed_stream od_muxed_stream
        = { "Object Detection",
            6,
            { uint32_t( md_type::META_DATA_INTEL_OBJECT_DETECTION_ID ), uint32_t( md_type::META_DATA_INTEL_OBJECT_DETECTION_ID ), 83, false },
            { 2347, 1, rs_fourcc( 'G', 'R', 'E', 'Y' ), 2347, true } };

    d500_object_detection::d500_object_detection( std::shared_ptr< const d500_info > const & dev_info )
        : device( dev_info )
        , d500_device( dev_info )
        , _object_detection_stream( new stream( RS2_STREAM_OBJECT_DETECTION ) )
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
        // A muxed stream has no controls of its own; its XU is the depth node's
        std::shared_ptr< uvc_sensor > xu_host = od_dev ? get_raw_depth_sensor() : nullptr;
        std::string physical_port;
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
                LOG_WARNING( "Object detection interface not present - sensor not created" );
                return;
            }
            if( od_devs_info.size() != 1 )
            {
                LOG_WARNING( "Expected a single object detection interface, found " << od_devs_info.size()
                                                                                    << " - OD sensor not created" );
                return;
            }
            od_dev = get_backend()->create_uvc_device( od_devs_info.front() );
            physical_port = od_devs_info.front().device_path;
        }

        auto od_ep = create_object_detection_device( dev_info->get_context(), od_dev, physical_port, xu_host );
        _object_detection_device_idx = add_sensor( od_ep );
    }

    std::shared_ptr< synthetic_sensor >
    d500_object_detection::create_object_detection_device( std::shared_ptr< context > ctx,
                                                           std::shared_ptr< platform::uvc_device > od_device,
                                                           const std::string & physical_port,
                                                           std::shared_ptr< uvc_sensor > xu_host )
    {
        auto enable_global_time_option = std::shared_ptr< global_time_option >( new global_time_option() );

        std::unique_ptr< frame_timestamp_reader > ts_reader_backup( new ds_timestamp_reader() );

        // TODO - check the whole timestamp reader chain - do we get the timestamp from UVC header into the metadata? do
        // we need to convert it from ds timestamp to global timestamp? etc
        auto raw_od_ep = std::make_shared< uvc_sensor >(
            "Raw Object Detection Device",
            od_device,
            std::unique_ptr< frame_timestamp_reader >( new global_timestamp_reader( std::move( ts_reader_backup ),
                                                                                    _tf_keeper,
                                                                                    enable_global_time_option ) ),
            this );

        if( ! xu_host )
            xu_host = raw_od_ep;
        xu_host->register_xu( ds::inference_xu ); // ensure the XU is initialized every time we power the camera

        auto od_ep = std::make_shared< d500_object_detection_sensor >( this,
                                                                       raw_od_ep,
                                                                       od_fourcc_to_rs2_format,
                                                                       od_fourcc_to_rs2_stream );

        od_ep->register_option( RS2_OPTION_GLOBAL_TIME_ENABLED, enable_global_time_option );

        if( _fw_version >= firmware_version( "7.58.40802.12738" ) )
        {
            auto detection_distance = std::make_shared< uvc_xu_option< uint8_t > >(xu_host,
                                                                                   ds::inference_xu,
                                                                                   ds::d500_xu_id::DETECTION_DISTANCE,
                                                                                   "Enable firmware distance calculation for detections" );
            od_ep->register_option( RS2_OPTION_DETECTION_DISTANCE, detection_distance );
        }

        od_ep->register_info( RS2_CAMERA_INFO_PHYSICAL_PORT, physical_port );

        register_metadata( raw_od_ep );
        register_processing_blocks( od_ep );

        return od_ep;
    }

    void d500_object_detection::register_metadata( std::shared_ptr< uvc_sensor > raw_od_ep )
    {
        raw_od_ep->register_metadata( RS2_FRAME_METADATA_FRAME_TIMESTAMP,
                                      make_uvc_header_parser( &platform::uvc_header::timestamp ) );
    }

    void d500_object_detection::register_processing_blocks( std::shared_ptr< d500_object_detection_sensor > od_ep )
    {
        processing_block_factory od_pbf = { { { RS2_FORMAT_Y8, RS2_STREAM_OBJECT_DETECTION } },
                                            { { RS2_FORMAT_Y8, RS2_STREAM_OBJECT_DETECTION } },
                                            []() { return std::make_shared< identity_processing_block >(); } };
        od_ep->register_processing_block( od_pbf );
    }

    void d500_object_detection_sensor::open( const stream_profiles & requests )
    {
        synthetic_sensor::open( requests );
    }

    void d500_object_detection_sensor::start( rs2_frame_callback_sptr callback )
    {
        synthetic_sensor::start( callback );
    }

    void d500_object_detection_sensor::stop()
    {
        synthetic_sensor::stop();
    }

    stream_profiles d500_object_detection_sensor::init_stream_profiles()
    {
        // No extrinsics registered for this stream: FW reports detections as color-frame pixels,
        // so it has no pose of its own and nothing transforms through it.
        auto results = synthetic_sensor::init_stream_profiles();
        for( auto p : results )
        {
            if( p->get_stream_type() == RS2_STREAM_OBJECT_DETECTION )
            {
                assign_stream( _owner->_object_detection_stream, p );
                p->set_name( perception_sensor::STREAM_NAME );
            }
        }
        return results;
    }
}
