// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/device.h>
#include <memory>


namespace librealsense {


namespace platform {
class backend;
}


// Common base class for all backend devices (i.e., those that require a platform backend)
//
class backend_device : public virtual device
{
    typedef device super;

protected:
    backend_device( std::shared_ptr< const device_info > const & dev_info, bool device_changed_notifications = true )
        : super( dev_info, device_changed_notifications )
    {
    }

public:
    uint16_t get_pid() const { return _pid; }
    std::shared_ptr< platform::backend > get_backend();

    // Some FW versions use 16 bit imu register values and some use 32 bit for higher accuracy.
    virtual bool is_imu_high_accuracy() const { return false; }
    // Gyro scale factor (raw to physical) can be different between product lines and FW versions.
    virtual double get_gyro_default_scale() const { return 0.1; }

    // FW accel unit in g per count: legacy FW sends integer milli-g (0.001 g)
    virtual double get_accel_default_scale() const { return 0.001; }

protected:
    uint16_t _pid = 0;
};


}  // namespace librealsense
