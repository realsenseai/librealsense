// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d500-depth-mapping.h"
#include <src/perception-sensor.h>
#include "core/video.h"

namespace librealsense
{
    // Object Detection and the Mapping streams on one sensor (D585 legacy: Object Detection only)
    // Adding an object-detection stream support. Device classes inherit it to expose object detection.
    // Since it is supported only on some FW versions, if the device does not provide the stream, the sensor is silently
    // created without it (not at all without the Mapping streams), not failing the device initialization.
    class d500_perception : public d500_depth_mapping
    {
    public:
        d500_perception( std::shared_ptr< const d500_info > const & );

        // `xu_host` is the sensor whose node publishes the inference XU, or null for the OD sensor itself
        std::shared_ptr< synthetic_sensor > create_object_detection_device( std::shared_ptr< context > ctx,
                                                                            std::shared_ptr< platform::multi_pins_uvc_device > od_device,
                                                                            const std::string & physical_port,
                                                                            bool with_object_detection,
                                                                            std::shared_ptr< uvc_sensor > xu_host );

    private:
        std::shared_ptr< platform::uvc_device > find_od_device( std::shared_ptr< const d500_info > const &, std::string & port );

    protected:
        std::shared_ptr< stream_interface > _object_detection_stream;
        uint8_t _object_detection_device_idx = 0;
    };

    class d500_perception_sensor : public d500_depth_mapping_sensor,
                                   public perception_sensor
    {
    public:
        explicit d500_perception_sensor( d500_perception * owner, std::shared_ptr< uvc_sensor > uvc_sensor,
                                         std::shared_ptr< stream_interface > object_detection_stream );

        stream_profiles init_stream_profiles() override;
        void open( const stream_profiles & requests ) override;
        void start( rs2_frame_callback_sptr callback ) override;
        void stop() override;

        std::string get_depth_mapping_params() const override;
        void set_depth_mapping_params( const std::string & params_json_str ) const override;

    protected:
        std::shared_ptr< stream_interface > _object_detection_stream;
    };
}
