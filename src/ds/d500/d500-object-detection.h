// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d500-device.h"
#include "core/video.h"
#include "inference-sensor.h"


namespace librealsense
{
    class d500_object_detection_sensor;

    class d500_object_detection
        : public virtual d500_device
    {
    public:
        d500_object_detection( std::shared_ptr< const d500_info > const & );

        bool has_object_detection_sensor() const { return _has_object_detection_sensor; }

    private:
        friend class d500_object_detection_sensor;

        std::shared_ptr< synthetic_sensor > create_object_detection_device(
            const platform::uvc_device_info & object_detection_device_info );

    protected:
        std::shared_ptr< stream_interface > _object_detection_stream;
        uint8_t _object_detection_device_idx = 0;
        bool _has_object_detection_sensor = false;
    };


    class d500_object_detection_sensor
        : public synthetic_sensor
        , public video_sensor_interface
        , public inference_sensor
        , public object_detection_sensor
    {
    public:
        explicit d500_object_detection_sensor( d500_object_detection * owner,
                                               std::shared_ptr< uvc_sensor > uvc_sensor );

        rs2_intrinsics get_intrinsics( const stream_profile & profile ) const override;
        stream_profiles init_stream_profiles() override;

    protected:
        const d500_object_detection * _owner;
    };
}
