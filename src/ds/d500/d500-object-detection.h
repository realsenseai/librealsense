// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d500-device.h"
#include <src/inference-sensor.h>
#include "core/video.h"

namespace librealsense
{
    class d500_object_detection_sensor;

    // Adding an object-detection stream support. Device classes inherit it to expose object detection.
    // Since it is supported only on some FW versions, if the device does not provide the stream, the sensor is silently
    // not created, not failing the device initialization.
    class d500_object_detection : public virtual d500_device
    {
    public:
        d500_object_detection( std::shared_ptr< const d500_info > const & );

        std::shared_ptr< synthetic_sensor > create_object_detection_device( std::shared_ptr< context > ctx,
                                                                            const std::vector< platform::uvc_device_info > & od_devices_info );

        void on_depth_sensor_starting() override;

    private:
        friend class d500_object_detection_sensor;

        void register_metadata( std::shared_ptr< uvc_sensor > raw_od_ep );
        void register_processing_blocks( std::shared_ptr< d500_object_detection_sensor > od_ep );

    protected:
        std::shared_ptr< stream_interface > _object_detection_stream;
        uint8_t _object_detection_device_idx = 0;
    };

    class d500_object_detection_sensor : public synthetic_sensor,
                                         public object_detection_sensor
    {
    public:
        explicit d500_object_detection_sensor( d500_object_detection * owner,
                                               std::shared_ptr< uvc_sensor > uvc_sensor,
                                               std::map< uint32_t, rs2_format > od_fourcc_to_rs2_format,
                                               std::map< uint32_t, rs2_stream > od_fourcc_to_rs2_stream )
            : synthetic_sensor( "Person Detection Camera", uvc_sensor, owner, od_fourcc_to_rs2_format, od_fourcc_to_rs2_stream )
            , _owner( owner )
        {
        }

        stream_profiles init_stream_profiles() override;

    protected:
        const d500_object_detection * _owner;
    };
}
