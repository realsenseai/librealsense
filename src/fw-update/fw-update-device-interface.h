// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.
#pragma once

#include <src/core/extension.h>
#include <src/librealsense-exception.h>
#include <librealsense2/hpp/rs_types.hpp>
#include <cstdint>
#include <string>
#include <vector>


namespace librealsense
{
    class firmware_check_interface
    {
    public:
        virtual bool check_fw_compatibility(const std::vector<uint8_t>& image) const = 0;
        // Minimum FW version supported by this device's SKU (e.g. "5.10.0.17").
        // Default implementation throws not_implemented_exception; only devices that
        // define such a minimum (currently D400) override.
        virtual std::string get_firmware_min_version() const
        {
            throw not_implemented_exception( "This device does not define a minimum firmware version" );
        }
    };

    // Regular devices inherit to enable entering DFU state or implementing unsigned FW update.
    class updatable : public virtual firmware_check_interface
    {
    public:
        // Places the device in DFU (recovery) mode, where the DFU process can continue with update_device_interface.
        // Restarts the device!
        virtual void enter_update_state() const = 0;
        // Returns a backup of the current flash image. Optional: return an empty buffer if unsupported
        virtual std::vector< uint8_t > backup_flash( rs2_update_progress_callback_sptr callback ) = 0;

        // Unsigned FW update. When this is called, we're not in an "update state".
        virtual void update_flash( const std::vector< uint8_t > & image,
                                   rs2_update_progress_callback_sptr callback,
                                   int update_mode )
            = 0;
    };

    MAP_EXTENSION( RS2_EXTENSION_UPDATABLE, updatable );

    // Recovery devices implement this to perform DFU with signed FW.
    class update_device_interface : public virtual firmware_check_interface
    {
    public:
        // Signed FW update
        virtual void update( const void * fw_image, int fw_image_size, rs2_update_progress_callback_sptr = nullptr ) const = 0;
    };

    MAP_EXTENSION(RS2_EXTENSION_UPDATE_DEVICE, update_device_interface);
}
