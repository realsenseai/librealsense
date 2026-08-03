// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#pragma once

#include <ds/d400/d400-device.h>
#include <core/advanced_mode.h>

namespace librealsense
{
    // Active means the HW includes an active projector
    class d400_mipi_device : public virtual d400_device,
                             public ds_advanced_mode_base,
                             public update_device_interface  // for signed fw update
    {
    public:
        d400_mipi_device();
        virtual ~d400_mipi_device() = default;

        void hardware_reset() override;
        void update( const void * fw_image, int fw_image_size, rs2_update_progress_callback_sptr = nullptr ) const override;
        void update_flash(const std::vector<uint8_t>& image, rs2_update_progress_callback_sptr callback, int update_mode) override;

    private:
        void update_signed_firmware(const std::vector<uint8_t>& image,
                          rs2_update_progress_callback_sptr callback);

        void update_non_const( const void * fw_image, int fw_image_size, rs2_update_progress_callback_sptr = nullptr );
    };
}
