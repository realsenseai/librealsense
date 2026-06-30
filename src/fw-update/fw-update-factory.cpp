// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019-2024 RealSense, Inc. All Rights Reserved.

#include "fw-update-factory.h"
#include "fw-update-device.h"
#include "usb/usb-enumerator.h"
#include "ds/d400/d400-private.h"
#include "ds/d500/d500-private.h"
#include "ds/d400/d400-fw-update-device.h"
#include "ds/d500/d500-fw-update-device.h"

#include <rsutils/string/from.h>


#define FW_UPDATE_INTERFACE_NUMBER 0
#define DEFAULT_TIMEOUT 100

namespace librealsense
{
    int get_product_line(const platform::usb_device_info &usb_info)
    {
        if( ds::RS_D400_RECOVERY_PID == usb_info.pid )
            return RS2_PRODUCT_LINE_D400;
        if( ds::RS_D400_USB2_RECOVERY_PID == usb_info.pid )
            return RS2_PRODUCT_LINE_D400;
        if( ds::D555_RECOVERY_PID == usb_info.pid )
            return RS2_PRODUCT_LINE_D500;
        if( ds::D585S_RECOVERY_PID == usb_info.pid )
            return RS2_PRODUCT_LINE_D500;
        if( ds::D500_RECOVERY_PID == usb_info.pid )
            return RS2_PRODUCT_LINE_D500;
        if( ds::D500_USB2_RECOVERY_PID == usb_info.pid )
            return RS2_PRODUCT_LINE_D500;
        return 0;
    }


    std::vector< std::shared_ptr< fw_update_info > > fw_update_info::pick_recovery_devices(
        std::shared_ptr< context > ctx, const std::vector< platform::usb_device_info > & usb_devices, int mask )
    {
        std::vector< std::shared_ptr< fw_update_info > > list;
        for (auto&& usb : usb_devices)
        {
            auto pl = get_product_line(usb);
            if(pl & mask)
                list.push_back(std::make_shared<fw_update_info>(ctx, usb));
        }
        return list;
    }

    std::vector< std::shared_ptr< fw_update_info > > fw_update_info::pick_recovery_devices(
        std::shared_ptr< context > ctx, const std::vector< platform::mipi_device_info > & mipi_devices, int mask )
    {
        std::vector< std::shared_ptr< fw_update_info > > list;
        for (auto&& mipi : mipi_devices)
        {
            list.push_back(std::make_shared<fw_update_info>(ctx, mipi));
        }
        return list;
    }


    std::shared_ptr<device_interface> fw_update_info::create_device()
    {
        auto usb_devices = get_group().usb_devices;
        if (!usb_devices.empty())
        {
            auto& dfu_id = usb_devices.front().id;

            if (&dfu_id != nullptr) {
                for (auto&& info : usb_devices)
                {
                    if (info.id == dfu_id)
                    {
                        auto usb = platform::usb_enumerator::create_usb_device(info);
                        if (!usb)
                            continue;
                        switch (info.pid)
                        {
                        case ds::RS_D400_RECOVERY_PID:
                        case ds::RS_D400_USB2_RECOVERY_PID:
                            return std::make_shared< ds_d400_update_device >(shared_from_this(), usb);
                        case ds::D555_RECOVERY_PID:
                        case ds::D585S_RECOVERY_PID:
                        case ds::D500_RECOVERY_PID:
                        case ds::D500_USB2_RECOVERY_PID:
                            return std::make_shared< ds_d500_update_device >(shared_from_this(), usb);
                        default:
                            // Do nothing
                            break;
                        }
                    }
                }
            }
            throw std::runtime_error(rsutils::string::from()
                << "Failed to create FW update device, device id: " << dfu_id);
        }

        auto mipi_devices = get_group().mipi_devices;
        if (!mipi_devices.empty())
        {
            auto const& mipi_id = mipi_devices.front().id;
            for (auto&& info : mipi_devices)
            {
                auto mipi = platform::mipi_device::create_mipi_device(info);
                if (!mipi)
                    continue;
                switch (info.pid)
                {
                case ds::RS400_MIPI_RECOVERY_PID:
                    return std::make_shared< ds_d400_update_device >(shared_from_this(), mipi);
                default:
                    // Do nothing
                    break;
                }
            }
            throw std::runtime_error(rsutils::string::from()
                << "Failed to create FW update device, device id: " << mipi_id);
        }
        throw std::runtime_error(rsutils::string::from()
            << "Failed to create FW update device - device not found");
    }

}
