// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "rs-dds-sensor-proxy.h"
#include <src/inference-sensor.h>


namespace librealsense {

// For cases when checking if this is< inference_sensor >
class dds_inference_sensor_proxy
    : public dds_sensor_proxy
    , public inference_sensor
{
public:
    dds_inference_sensor_proxy( std::string const & sensor_name,
                                software_device * owner,
                                std::shared_ptr< realdds::dds_device > const & dev )
        : dds_sensor_proxy( sensor_name, owner, dev )
    {
    }
};

// For cases when checking if this is< object_detection_sensor > or is< inference_sensor >
class dds_object_detection_sensor_proxy
    : public dds_inference_sensor_proxy
    , public object_detection_sensor
{
public:
    dds_object_detection_sensor_proxy( std::string const & sensor_name,
                                       software_device * owner,
                                       std::shared_ptr< realdds::dds_device > const & dev )
        : dds_inference_sensor_proxy( sensor_name, owner, dev )
    {
    }
};

}  // namespace librealsense
