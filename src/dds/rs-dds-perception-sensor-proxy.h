// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "rs-dds-sensor-proxy.h"
#include <src/perception-sensor.h>


namespace librealsense {

// For cases when checking if this is< perception_sensor >
class dds_perception_sensor_proxy
    : public dds_sensor_proxy
    , public virtual perception_sensor
{
public:
    dds_perception_sensor_proxy( std::string const & sensor_name,
                                 software_device * owner,
                                 std::shared_ptr< realdds::dds_device > const & dev )
        : dds_sensor_proxy( sensor_name, owner, dev )
    {
    }
};

}  // namespace librealsense
