// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015-2024 RealSense, Inc. All Rights Reserved.

#pragma once

#include "backend.h"

#include <memory>
#include <vector>

namespace librealsense
{
    namespace platform
    {
        class v4l_backend : public backend
        {
        public:
            std::shared_ptr<uvc_device> create_uvc_device(uvc_device_info info) const override;
            std::vector<uvc_device_info> query_uvc_devices() const override;

            std::shared_ptr<command_transfer> create_usb_device(usb_device_info info) const override;
            std::vector<usb_device_info> query_usb_devices() const override;

            std::shared_ptr<hid_device> create_hid_device(hid_device_info info) const override;
            std::vector<hid_device_info> query_hid_devices() const override;

            std::vector<mipi_device_info> query_mipi_devices() const override;

            std::shared_ptr<device_watcher> create_device_watcher() const override;
        };
    }
}
