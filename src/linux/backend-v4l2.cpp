// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015-2024 RealSense, Inc. All Rights Reserved.

#include "backend-v4l2.h"

#include "backend-hid.h"
#include "v4l-enumerator.h"        // foreach_uvc_device()
#include "v4l-enumerator-mipi.h"   // foreach_mipi_device()
#include "v4l-mipi-device.h"
#include "v4l-uvc-device.h"
#include "v4l-uvc-meta-device.h"
#if defined(USING_UDEV)
#include "udev-device-watcher.h"
#else
#include "../polling-device-watcher.h"
#endif
#include "usb/usb-enumerator.h"
#include "usb/usb-device.h"
#include <src/platform/command-transfer.h>
#include "types.h"

#include <memory>
#include <vector>

namespace librealsense
{
    namespace platform
    {
        std::shared_ptr<uvc_device> v4l_backend::create_uvc_device(uvc_device_info info) const
        {
            bool mipi_device = info.is_mipi;

            auto v4l_uvc_dev =        mipi_device ?         std::make_shared<v4l_mipi_device>(info) :
                              ((!info.has_metadata_node) ?  std::make_shared<v4l_uvc_device>(info) :
                                                            std::make_shared<v4l_uvc_meta_device>(info));

            return std::make_shared<platform::retry_controls_work_around>(v4l_uvc_dev);
        }

        std::vector<uvc_device_info> v4l_backend::query_uvc_devices() const
        {
            std::vector<uvc_device_info> uvc_nodes;

            v4l_enum::foreach_uvc_device(
            [&uvc_nodes](const uvc_device_info& i, const std::string&)
            {
                uvc_nodes.push_back(i);
            });

            return uvc_nodes;
        }

        std::shared_ptr<command_transfer> v4l_backend::create_usb_device(usb_device_info info) const
        {
            auto dev = usb_enumerator::create_usb_device(info);
             if(dev)
                 return std::make_shared<platform::command_transfer_usb>(dev);
             return nullptr;
        }

        std::vector<usb_device_info> v4l_backend::query_usb_devices() const
        {
            auto device_infos = usb_enumerator::query_devices_info();
            return device_infos;
        }

        std::vector<mipi_device_info> v4l_backend::query_mipi_devices() const
        {
            std::vector<mipi_device_info> mipi_nodes;

            v4l_enum::foreach_mipi_device(
            [&mipi_nodes](const mipi_device_info& i, const std::string&)
            {
                mipi_nodes.push_back(i);
            });

            return mipi_nodes;
        }
        std::shared_ptr<hid_device> v4l_backend::create_hid_device(hid_device_info info) const
        {
            return std::make_shared<v4l_hid_device>(info);
        }

        std::vector<hid_device_info> v4l_backend::query_hid_devices() const
        {
            std::vector<hid_device_info> results;
            v4l_hid_device::foreach_hid_device([&](const hid_device_info& hid_dev_info){
                results.push_back(hid_dev_info);
            });
            return results;
        }

        std::shared_ptr<device_watcher> v4l_backend::create_device_watcher() const
        {
#if defined(USING_UDEV)
            return std::make_shared< udev_device_watcher >( this );
#else
            return std::make_shared< polling_device_watcher >( this );
#endif
        }

        std::shared_ptr<backend> create_backend()
        {
            return std::make_shared<v4l_backend>();
        }
    }
}
