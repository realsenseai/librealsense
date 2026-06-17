// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d500-device.h"
#include "stream.h"

#include <memory>


namespace librealsense
{
    // Supports two RGB streams over USB endpoints (pins) of the depth interface, instead of through a dedicated sensor
    class d500_dual_rgb : public virtual d500_device
    {
    public:
        d500_dual_rgb( std::shared_ptr< const d500_info > const & dev_info );

    protected:
        std::shared_ptr< stream_interface > _color_stream_1;
        std::shared_ptr< stream_interface > _color_stream_2;
    };
}
