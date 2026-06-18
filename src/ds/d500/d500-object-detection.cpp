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

#include <rsutils/type/fourcc.h>
using rs_fourcc = rsutils::type::fourcc;

#include <map>
#include <vector>

namespace librealsense
{
    const std::map< uint32_t, rs2_format > od_fourcc_to_rs2_format = {
        { rs_fourcc( 'G', 'R', 'E', 'Y' ), RS2_FORMAT_Y8 },
    };
    const std::map< uint32_t, rs2_stream > od_fourcc_to_rs2_stream = {
        { rs_fourcc( 'G', 'R', 'E', 'Y' ), RS2_STREAM_OBJECT_DETECTION },
    };


    d500_object_detection::d500_object_detection( std::shared_ptr< const d500_info > const & dev_info )
        : device( dev_info )
        , d500_device( dev_info )
        , _object_detection_stream( new stream( RS2_STREAM_OBJECT_DETECTION ) )
    {
        static const uint32_t od_stream_mi = 9; // UVC interface index of the object-detection stream.
        auto od_devs_info = filter_by_mi( dev_info->get_group().uvc_devices, od_stream_mi );

        // Skip if the device does not expose the stream; the rest of the device enumerates normally.
        if( od_devs_info.empty() )
        {
            LOG_DEBUG( "Object detection interface not present - sensor not created" );
            return;
        }
        if( od_devs_info.size() != 1 )
        {
            LOG_WARNING( "Expected a single object detection interface, found " << od_devs_info.size()
                                                                                << " - OD sensor not created" );
            return;
        }

        auto od_ep = create_object_detection_device( dev_info->get_context(), od_devs_info );
        _object_detection_device_idx = add_sensor( od_ep );
    }

    std::shared_ptr< synthetic_sensor >
    d500_object_detection::create_object_detection_device( std::shared_ptr< context > ctx,
                                                           const std::vector< platform::uvc_device_info > & od_devices_info )
    {
        auto enable_global_time_option = std::shared_ptr< global_time_option >( new global_time_option() );

        std::unique_ptr< frame_timestamp_reader > ts_reader_backup( new ds_timestamp_reader() );

        // TODO - check the whole timestamp reader chain - do we get the timestamp from UVC header into the metadata? do
        // we need to convert it from ds timestamp to global timestamp? etc
        auto raw_od_ep = std::make_shared< uvc_sensor >(
            "Raw Object Detection Device",
            get_backend()->create_uvc_device( od_devices_info.front() ),
            std::unique_ptr< frame_timestamp_reader >( new global_timestamp_reader( std::move( ts_reader_backup ),
                                                                                    _tf_keeper,
                                                                                    enable_global_time_option ) ),
            this );

        auto od_ep = std::make_shared< d500_object_detection_sensor >( this,
                                                                       raw_od_ep,
                                                                       od_fourcc_to_rs2_format,
                                                                       od_fourcc_to_rs2_stream );

        od_ep->register_option( RS2_OPTION_GLOBAL_TIME_ENABLED, enable_global_time_option );
        od_ep->register_info( RS2_CAMERA_INFO_PHYSICAL_PORT, od_devices_info.front().device_path );

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

    void d500_object_detection::on_depth_sensor_starting() noexcept
    {
        // Firmware requires the Align_Depth XU (selector 0x10) to be set before depth
        // streaming starts; it is silently ignored if sent after streaming begins.
        auto raw_depth = get_raw_depth_sensor();
        if( !raw_depth )
            return;

        try
        {
            raw_depth->invoke_powered( []( platform::uvc_device & dev )
            {
                uint8_t enable = 1;
                if( !dev.set_xu( ds::depth_xu, ds::DS5_ALIGN_DEPTH, &enable, sizeof( enable ) ) )
                    LOG_WARNING( "Failed to enable Align_Depth XU on depth sensor" );
            } );
        }
        catch( std::exception const & e )
        {
            LOG_WARNING( "Align_Depth XU exception: " << e.what() );
        }
        catch( ... )
        {
            LOG_WARNING( "Align_Depth XU: unknown exception" );
        }
    }

    stream_profiles d500_object_detection_sensor::init_stream_profiles()
    {
        // TODO - check if needed. Registers extrinsics, but not sure it is needed for the OD stream, which is not a physical stream.
        auto results = synthetic_sensor::init_stream_profiles();
        for( auto p : results )
        {
            if( p->get_stream_type() == RS2_STREAM_OBJECT_DETECTION )
            {
                assign_stream( _owner->_object_detection_stream, p );
                p->set_name( "Person Detection" );
            }
        }
        return results;
    }
}
