// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "context.h"
#include <string>
#include "d400-mipi-device.h"

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
        std::thread fake_notification(
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
            });
        fake_notification.detach();
    }

    void d400_mipi_device::update_signed_firmware(const std::vector<uint8_t>& image,
                                                  rs2_update_progress_callback_sptr callback)
    {
        LOG_INFO("Burning Signed Firmware on MIPI device");

        bool is_mipi_recovery = _pid == ds::RS400_MIPI_RECOVERY_PID;
        rs2_camera_info _dfu_port_info = (is_mipi_recovery)?
                    (RS2_CAMERA_INFO_PHYSICAL_PORT):(RS2_CAMERA_INFO_DFU_DEVICE_PATH);

        // Write signed firmware to appropriate file descriptor
        std::ofstream fw_path_in_device(get_info(_dfu_port_info), std::ios::binary);
        if (fw_path_in_device)
        {
            bool burn_done = false;
            std::thread show_progress_thread(
                [&]()
                {
                    for( int i = 0; i < 95 && !burn_done; ++i ) // Show percentage [0-100]
                    {
                        if (callback)
                            callback->on_update_progress(i);
                        std::this_thread::sleep_for( std::chrono::milliseconds( 1020 ) );
                    }
                } );

            fw_path_in_device.write(reinterpret_cast<const char*>(image.data()), image.size());
            burn_done = true;
            show_progress_thread.join();
        }
        else
        {
            LOG_WARNING("Firmware Update failed - wrong path or permissions missing");
            return;
        }
        LOG_INFO("FW update process completed successfully.");
        fw_path_in_device.close();

        if (callback)
            callback->on_update_progress(95);
        if (is_mipi_recovery)
        {
            LOG_INFO("For GMSL MIPI device please reboot, or reload d4xx driver\n"\
                     "sudo rmmod d4xx && sudo modprobe d4xx\n"\
                     "and restart the realsense-viewer");
        }
        // Restart the device to reconstruct with the new version information
        hardware_reset();
        std::this_thread::sleep_for( std::chrono::seconds( 2 ) );
        if (callback)
            callback->on_update_progress(100);
    }

    void d400_mipi_device::update( const void * fw_image, int fw_image_size, rs2_update_progress_callback_sptr progress_callback) const
    {
        const_cast<d400_mipi_device*>(this)->update_non_const(fw_image, fw_image_size, progress_callback);
    }

    void d400_mipi_device::update_non_const( const void * fw_image, int fw_image_size, rs2_update_progress_callback_sptr progress_callback )
    {
        // first pausing the options watcher (if running)
        pause_options_watchers();

        try{
            std::vector<uint8_t> fw_image_vec (static_cast<const uint8_t*>(fw_image), static_cast<const uint8_t*>(fw_image) + fw_image_size);
            update_signed_firmware(fw_image_vec, progress_callback);
        }
        catch(...)
        {
            unpause_options_watchers();
            throw;
        }

        // finally, unpausing the options watcher
        unpause_options_watchers();
    }

    void d400_mipi_device::pause_options_watchers()
    {
        for( auto& sensor_index : _sensors_indices)
        {
            auto& synthetic_sensor_ref = dynamic_cast<synthetic_sensor&>(get_sensor(sensor_index));
            synthetic_sensor_ref.pause_options_watcher();
        }
    }

    void d400_mipi_device::unpause_options_watchers()
    {
        for( auto& sensor_index : _sensors_indices)
        {
            auto& synthetic_sensor_ref = dynamic_cast<synthetic_sensor&>(get_sensor(sensor_index));
            synthetic_sensor_ref.unpause_options_watcher();
        }
    }
}
