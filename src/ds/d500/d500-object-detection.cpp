// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "d500-object-detection.h"

#include "backend.h"
#include "d500-info.h"
#include "ds/ds-timestamp.h"
#include "environment.h"
#include "platform/platform-utils.h"
#include "proc/processing-blocks-factory.h"
#include "stream.h"

#include <rsutils/type/fourcc.h>


using rs_fourcc = rsutils::type::fourcc;


namespace librealsense
{
    namespace
    {
        constexpr uint32_t object_detection_width = 1067;
        constexpr uint32_t object_detection_height = 1;

        // The OD UVC function appears on different control MIs depending on
        // whether CDC interfaces are present in the firmware configuration.
        constexpr uint32_t object_detection_control_mis[] = { 9, 7 };

        const std::map< uint32_t, rs2_format > object_detection_fourcc_to_rs2_format = {
            { rs_fourcc( 'G', 'R', 'E', 'Y' ), RS2_FORMAT_Y8 },
            { rs_fourcc( 'Y', '8', ' ', ' ' ), RS2_FORMAT_Y8 },
        };

        const std::map< uint32_t, rs2_stream > object_detection_fourcc_to_rs2_stream = {
            { rs_fourcc( 'G', 'R', 'E', 'Y' ), RS2_STREAM_OBJECT_DETECTION },
            { rs_fourcc( 'Y', '8', ' ', ' ' ), RS2_STREAM_OBJECT_DETECTION },
        };
    }


    d500_object_detection::d500_object_detection( std::shared_ptr< const d500_info > const & dev_info )
        : device( dev_info )
        , d500_device( dev_info )
        , _object_detection_stream( new stream( RS2_STREAM_OBJECT_DETECTION ) )
    {
        std::vector< platform::uvc_device_info > od_devices;
        for( auto mi : object_detection_control_mis )
        {
            od_devices = platform::filter_by_mi( dev_info->get_group().uvc_devices, mi );
            if( ! od_devices.empty() )
                break;
        }

        if( od_devices.empty() )
        {
            LOG_DEBUG( "No D500 UVC Object Detection endpoint found" );
            return;
        }

        auto const & uvc_info = od_devices.front();
        auto object_detection_ep = create_object_detection_device( uvc_info );
        _object_detection_device_idx = add_sensor( object_detection_ep );
        _has_object_detection_sensor = true;
        LOG_DEBUG( "D500 Object Detection endpoint found: id=" << uvc_info.id << " mi=" << uvc_info.mi );
    }


    std::shared_ptr< synthetic_sensor > d500_object_detection::create_object_detection_device(
        const platform::uvc_device_info & object_detection_device_info )
    {
        register_stream_to_extrinsic_group( *_object_detection_stream, 0 );

        std::unique_ptr< frame_timestamp_reader > ds_timestamp_reader_backup( new ds_timestamp_reader() );
        auto enable_global_time_option = std::make_shared< global_time_option >();

        auto raw_object_detection_ep = std::make_shared< uvc_sensor >(
            "Raw Object Detection Device",
            get_backend()->create_uvc_device( object_detection_device_info ),
            std::unique_ptr< frame_timestamp_reader >(
                new global_timestamp_reader( std::move( ds_timestamp_reader_backup ),
                                             _tf_keeper,
                                             enable_global_time_option ) ),
            this );

        auto object_detection_ep = std::make_shared< d500_object_detection_sensor >( this, raw_object_detection_ep );
        object_detection_ep->register_option( RS2_OPTION_GLOBAL_TIME_ENABLED, enable_global_time_option );
        object_detection_ep->register_info( RS2_CAMERA_INFO_PHYSICAL_PORT, object_detection_device_info.device_path );
        object_detection_ep->register_processing_block(
            processing_block_factory::create_id_pbf( RS2_FORMAT_Y8, RS2_STREAM_OBJECT_DETECTION ) );

        return object_detection_ep;
    }


    d500_object_detection_sensor::d500_object_detection_sensor( d500_object_detection * owner,
                                                                std::shared_ptr< uvc_sensor > uvc_sensor )
        : synthetic_sensor( "Object Detection Camera",
                            uvc_sensor,
                            owner,
                            object_detection_fourcc_to_rs2_format,
                            object_detection_fourcc_to_rs2_stream )
        , _owner( owner )
    {
    }


    stream_profiles d500_object_detection_sensor::init_stream_profiles()
    {
        auto lock = environment::get_instance().get_extrinsics_graph().lock();
        auto results = synthetic_sensor::init_stream_profiles();
        stream_profiles relevant_results;

        for( auto p : results )
        {
            if( p->get_stream_type() != RS2_STREAM_OBJECT_DETECTION )
                continue;

            auto const profile = to_profile( p.get() );
            if( profile.width != object_detection_width || profile.height != object_detection_height )
                continue;

            assign_stream( _owner->_object_detection_stream, p );

            auto && video = dynamic_cast< video_stream_profile_interface * >( p.get() );
            video->set_intrinsics( []() { return rs2_intrinsics{}; } );
            relevant_results.push_back( std::move( p ) );
        }

        return relevant_results;
    }


    rs2_intrinsics d500_object_detection_sensor::get_intrinsics( const stream_profile & ) const
    {
        return rs2_intrinsics{};
    }
}
