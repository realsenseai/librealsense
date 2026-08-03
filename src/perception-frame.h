// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include "frame.h"
#include "core/extension.h"
#include <librealsense2/h/rs_types.h>

namespace librealsense {

class perception_frame : public frame
{
public:
    // This class currently serves as a base type for perception frames
    perception_frame() : frame() {}

    // Currently only object detection is supported, but this can be extended in the future to support more types of perception results
    enum class type : uint8_t
    {
        OBJECT_DETECTION = 0
    };
};

MAP_EXTENSION(RS2_EXTENSION_PERCEPTION_FRAME, librealsense::perception_frame);

}  // namespace librealsense
