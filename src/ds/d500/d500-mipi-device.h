// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "fw-update/fw-update-device-interface.h"
#include "ds/ds-mipi-device.h"

#include <memory>
#include <string>

namespace librealsense
{
    class ds_device_common;
    class polling_error_handler;

    // Update-device implementation for operational D5xx GMSL. d500_device holds an
    // instance and returns it from extend_to(), so rs2::device::as<update_device>()
    // succeeds only on MIPI. D5xx uses the same class for USB and GMSL, so it
    // cannot inherit update_device_interface unconditionally the way
    // d400_mipi_device does. The shared MIPI DFU write flow is delegated to
    // ds_mipi_device (src/ds/ds-mipi-device.h) — the same helper d400_mipi_device
    // uses — so any DFU-flow change lands in one place.
    class d500_mipi_device : public update_device_interface
    {
    public:
        d500_mipi_device( const std::string & dfu_device_path,
                          std::shared_ptr< ds_device_common > device_common,
                          std::shared_ptr< polling_error_handler > error_poller );

        void update( const void * fw_image, int fw_image_size,
                     rs2_update_progress_callback_sptr = nullptr ) const override;

        // Matches ds_d500_update_device: D5xx GMSL relies on the caller-side
        // fw_update::check_fw_compatibility() (common/fw-update-common.cpp) rather
        // than the update_device_interface hook.
        bool check_fw_compatibility( const std::vector< uint8_t > & image ) const override { return true; }

    private:
        void hardware_reset() const;

        std::string _dfu_device_path;
        std::shared_ptr< ds_device_common > _device_common;
        ds_mipi_device _mipi;
    };
}
