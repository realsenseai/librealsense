// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#pragma once

#include <ds/d400/d400-device.h>
#include <core/advanced_mode.h>

namespace librealsense
{
    // Active means the HW includes an active projector
    class d400_mipi_device : public virtual d400_device,
                             public ds_advanced_mode_base
    {
    public:
        d400_mipi_device();
        virtual ~d400_mipi_device() = default;

        void hardware_reset() override;
        void toggle_advanced_mode(bool enable) override;
        void update_flash(const std::vector<uint8_t>& image,
                          rs2_update_progress_callback_sptr callback, int update_mode) override;

    private:
        void update_signed_firmware(const std::vector<uint8_t>& image,
                          rs2_update_progress_callback_sptr callback);
        static void simulate_device_reconnect(std::shared_ptr<const device_info> dev_info);
    };
}
