// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "context.h"
#include "d400-mipi-device.h"
#include <rsutils/concurrency/thread-utils.h>

namespace librealsense
{
    d400_mipi_device::d400_mipi_device()
        : ds_advanced_mode_base()
    {
        ds_advanced_mode_base::initialize_advanced_mode( this );
    }

    void d400_mipi_device::hardware_reset()
    {
        d400_device::hardware_reset();
        simulate_device_reconnect(this->get_device_info());
    }

    void d400_mipi_device::toggle_advanced_mode(bool enable)
    {
        ds_advanced_mode_base::toggle_advanced_mode(enable);
        simulate_device_reconnect(this->get_device_info());
    }

    void d400_mipi_device::simulate_device_reconnect(std::shared_ptr<const device_info> dev_info)
    {
        //limitation: the user must hold the context from which the device was created
        //creating fake notification to trigger invoke_devices_changed_callbacks, causing disconnection and connection
        auto non_const_device_info = std::const_pointer_cast<librealsense::device_info>(dev_info);
        std::vector< std::shared_ptr< device_info > > devices{ non_const_device_info };
        auto ctx = std::weak_ptr< context >(dev_info->get_context());
        rsutils::concurrency::create_thread(
            rsutils::concurrency::thread_category_device_monitoring,
            "mipi-reconn",
            [ctx, devs = std::move(devices)]()
            {
                try
                {
                    if (auto strong = ctx.lock())
                    {
                        strong->invoke_devices_changed_callbacks(devs, {});
                        // MIPI devices do not re-enumerate so we need to give them some time to restart
                        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
                    }
                    if (auto strong = ctx.lock())
                        strong->invoke_devices_changed_callbacks({}, devs);
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR(e.what());
                    return;
                }
            }).detach();
    }
}
