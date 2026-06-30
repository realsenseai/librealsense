// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "core/extension.h"

namespace librealsense {


class inference_sensor
{
public:
    virtual ~inference_sensor() = default;
};

MAP_EXTENSION( RS2_EXTENSION_INFERENCE_SENSOR, librealsense::inference_sensor );



class object_detection_sensor
{
public:
    virtual ~object_detection_sensor() = default;
};

MAP_EXTENSION( RS2_EXTENSION_OBJECT_DETECTION_SENSOR, librealsense::object_detection_sensor );


}  // namespace librealsense
