// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "context.h"
#include <string>
#include <fstream>
#include "d400-mipi-device.h"
#include "librealsense-exception.h"

namespace librealsense
{
    d400_mipi_device::d400_mipi_device()
        : ds_advanced_mode_base()
    {
        ds_advanced_mode_base::initialize_advanced_mode( this );
    }

    void d400_mipi_device::hardware_reset()
    {
        options_watcher_pause_guard guard(*this);
        d400_device::hardware_reset();
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
                        std::this_thread::sleep_for(std::chrono::seconds(5));
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
        std::string dfu_path = get_info(_dfu_port_info);
        std::ofstream fw_path_in_device(dfu_path, std::ios::binary);
        if (fw_path_in_device)
        {
            // Progress thread runs for the full ~95 seconds to give the device
            // time to process the firmware. The write may return instantly (OS
            // buffering) but the device still needs time to burn.
            std::thread show_progress_thread(
                [&]()
                {
                    for( int i = 0; i < 95; ++i ) // Show percentage [0-95]
                    {
                        if (callback)
                            callback->on_update_progress(static_cast<float>(i) / 100.f);
                        // needed a little more than 1 second * num of iterations to complete the burning
                        std::this_thread::sleep_for( std::chrono::milliseconds( 1020 ) );
                    }
                } );

            fw_path_in_device.write(reinterpret_cast<const char*>(image.data()), image.size());
            fw_path_in_device.flush();
            show_progress_thread.join();

            if( ! fw_path_in_device.good() )
                throw librealsense::io_exception( "Firmware write to DFU path failed: " + dfu_path );
        }
        else
        {
            throw librealsense::io_exception("Firmware Update failed - DFU path: " + dfu_path
                + " - wrong path or permissions missing");
        }

        fw_path_in_device.close();
        if( ! fw_path_in_device )
            throw librealsense::io_exception( "Firmware flush/close failed on DFU path: " + dfu_path );

        LOG_INFO("FW update process completed successfully.");

        if (callback)
            callback->on_update_progress(0.95f);
        if (is_mipi_recovery)
        {
            LOG_INFO("For GMSL MIPI device please reboot, or reload d4xx driver\n"\
                     "sudo rmmod d4xx && sudo modprobe d4xx\n"\
                     "and restart the realsense-viewer");
        }
        // Restart the device to reconstruct with the new version information
        // simulate_device_reconnect takes 5 seconds to fake the reconnect cycle
        hardware_reset();
        std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
        if (callback)
            callback->on_update_progress(1.f);
    }

    void d400_mipi_device::update( const void * fw_image, int fw_image_size, rs2_update_progress_callback_sptr progress_callback) const
    {
        // fw update usually do not change any data member in the sdk
        // but here we need to pause the options watchers which are non-const methods
        const_cast<d400_mipi_device*>(this)->update_non_const(fw_image, fw_image_size, progress_callback);
    }

    void d400_mipi_device::update_non_const( const void * fw_image, int fw_image_size, rs2_update_progress_callback_sptr progress_callback )
    {
        options_watcher_pause_guard guard(*this);
        std::vector<uint8_t> fw_image_vec (static_cast<const uint8_t*>(fw_image), static_cast<const uint8_t*>(fw_image) + fw_image_size);
        update_signed_firmware(fw_image_vec, progress_callback);
    }

    void d400_mipi_device::update_flash(const std::vector<uint8_t>& image, rs2_update_progress_callback_sptr callback, int update_mode)
    {
        options_watcher_pause_guard guard(*this);
        d400_device::update_flash(image, callback, update_mode);
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
