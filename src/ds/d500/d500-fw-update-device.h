// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include "fw-update/fw-update-device.h"

namespace librealsense
{
    class ds_d500_update_device : public update_device
    {
    public:
        ds_d500_update_device( std::shared_ptr< const device_info > const &,
                          std::shared_ptr< platform::usb_device > const & usb_device );
        virtual ~ds_d500_update_device() = default;

        virtual bool check_fw_compatibility(const std::vector<uint8_t>& image) const override;
        virtual void update(const void* fw_image, int fw_image_size, rs2_update_progress_callback_sptr = nullptr) const override;
        bool wait_for_manifest_completion(std::shared_ptr<platform::usb_messenger> messenger, const rs2_dfu_state state,
            std::chrono::seconds timeout_seconds, rs2_update_progress_callback_sptr update_progress_callback) const;
        virtual void dfu_manifest_phase(const platform::rs_usb_messenger& messenger, rs2_update_progress_callback_sptr update_progress_callback) const override;

    private:
        void report_progress_and_wait_for_fw_burn(rs2_update_progress_callback_sptr update_progress_callback, int required_dfu_time) const;
        // The following data member _is_dfu_monitoring_enabled is needed to perform DFU when
        // the DFU monitoring is not enabled by FW, for D500 device.
        // Its value may be changed to false after the process starts as if the DFU monitoring was
        // enabled and the progress feedback that should come from the FW remains 0 after few iterations.
        mutable bool _is_dfu_monitoring_enabled = true;
    };
}
