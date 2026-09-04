// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "context.h"
#include <string>
#include "d400-mipi-device.h"
#include "ds/ds-mipi-device.h"
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
        _ds_device_common->hardware_reset( std::chrono::seconds( 5 ) );
    }

    void d400_mipi_device::update_signed_firmware(const std::vector<uint8_t>& image,
                                                  rs2_update_progress_callback_sptr callback)
    {
        LOG_INFO("Burning Signed Firmware on MIPI device");

        bool is_mipi_recovery = _pid == ds::RS400_MIPI_RECOVERY_PID;
        rs2_camera_info _dfu_port_info = (is_mipi_recovery)?
                    (RS2_CAMERA_INFO_PHYSICAL_PORT):(RS2_CAMERA_INFO_DFU_DEVICE_PATH);

        // Delegate the DFU chardev write + options-watcher pause to the shared
        // helper at src/ds/ds-mipi-device.cpp. D457 burn is ~95 s.
        std::string dfu_path = get_info(_dfu_port_info);
        ds_mipi_device( _ds_device_common ).perform_dfu_write(
            dfu_path, image.data(), image.size(), callback, 95 );

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
        // but here we call a non-const method to keep _pid usage explicit.
        const_cast<d400_mipi_device*>(this)->update_non_const(fw_image, fw_image_size, progress_callback);
    }

    void d400_mipi_device::update_non_const( const void * fw_image, int fw_image_size, rs2_update_progress_callback_sptr progress_callback )
    {
        // ds_mipi_device::perform_dfu_write() and hardware_reset() each pause the
        // options watchers over their own critical section; no outer guard needed.
        std::vector<uint8_t> fw_image_vec (static_cast<const uint8_t*>(fw_image), static_cast<const uint8_t*>(fw_image) + fw_image_size);
        update_signed_firmware(fw_image_vec, progress_callback);
    }

    void d400_mipi_device::update_flash(const std::vector<uint8_t>& image, rs2_update_progress_callback_sptr callback, int update_mode)
    {
        options_watcher_pause_guard guard(*_ds_device_common);
        d400_device::update_flash(image, callback, update_mode);
    }
}
