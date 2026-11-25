// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "core/extension.h"
#include "librealsense-exception.h"
#include <string>

namespace librealsense {


class perception_sensor
{
public:
    // Shared names for the perception sensor and its stream profile, used by both the USB and DDS paths
    static constexpr const char * SENSOR_NAME = "Perception";
    static constexpr const char * STREAM_NAME = "Object Detection";

    virtual ~perception_sensor() = default;

    // Occupancy grid and labeled point cloud params, as JSON
    virtual std::string get_depth_mapping_params() const
    {
        throw not_implemented_exception( "Depth mapping params are not supported on this sensor" );
    }
    virtual void set_depth_mapping_params( const std::string & ) const
    {
        throw not_implemented_exception( "Depth mapping params are not supported on this sensor" );
    }
};

MAP_EXTENSION( RS2_EXTENSION_PERCEPTION_SENSOR, librealsense::perception_sensor );


}  // namespace librealsense
