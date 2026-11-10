// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "calibration-change-tracker.h"
#include <src/core/frame-interface.h>
#include <src/core/sensor-interface.h>
#include <src/core/device-interface.h>
#include <src/device-calibration.h>


namespace librealsense
{
    bool calibration_change_tracker::changed( const rs2::frame & f )
    {
        auto sensor = reinterpret_cast< frame_interface * >( f.get() )->get_sensor();
        auto dev = sensor ? dynamic_cast< calibration_change_device const * >( &sensor->get_device() ) : nullptr;
        if( ! dev )
            return false;

        unsigned const count = dev->get_calibration_change_count();
        bool const changed = dev == _device && count != _count;
        _device = dev;
        _count = count;
        return changed;
    }
}
