// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include "core/extension.h"
#include <string>

namespace librealsense {
    class depth_mapping_sensor
    {
    public:
        virtual ~depth_mapping_sensor() = default;

        // Occupancy grid and labeled point cloud params, as JSON
        virtual std::string get_depth_mapping_params() const = 0;
        virtual void set_depth_mapping_params( const std::string & params_json_str ) const = 0;
    };
    MAP_EXTENSION(RS2_EXTENSION_DEPTH_MAPPING_SENSOR, librealsense::depth_mapping_sensor);
}
