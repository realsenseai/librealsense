// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019-2024 RealSense, Inc. All Rights Reserved.

#include "d400-fw-update-device.h"
#include "d400-private.h"

#include <librealsense2/h/rs_internal.h>

namespace librealsense
{
ds_d400_update_device::ds_d400_update_device(
    std::shared_ptr< const device_info > const & dev_info,
    std::shared_ptr< platform::usb_device > const & usb_device )
    : update_device( dev_info, usb_device, "D400" )
    {
        auto info = usb_device->get_info(); 
        _name = ds::rs400_sku_names.find(info.pid) != ds::rs400_sku_names.end() ? ds::rs400_sku_names.at(info.pid) : "unknown";          
        _serial_number = parse_serial_number(_serial_number_buffer);
    }

    ds_d400_update_device::ds_d400_update_device(
    std::shared_ptr< const device_info > const & dev_info,
    std::shared_ptr< platform::mipi_device > const & mipi_device )
    : update_device( dev_info, mipi_device, "D400" )
    {
        auto info = mipi_device->get_info();
        _name = ds::rs400_sku_names.find(info.pid) != ds::rs400_sku_names.end() ? ds::rs400_sku_names.at(info.pid) : "unknown";
        _serial_number = info.serial_number;
    }


    bool ds_d400_update_device::check_fw_compatibility(const std::vector<uint8_t>& image) const
    {
        // check if the given FW size matches the expected FW size
        if( image.size() != signed_fw_size )
            throw librealsense::invalid_value_exception(
                rsutils::string::from() << "Unsupported firmware binary image provided - " << image.size() << " bytes" );

        std::string fw_version = ds::extract_firmware_version_string(image);
        std::string const min_fw = get_firmware_min_version();
        bool result = (firmware_version(fw_version) >= firmware_version(min_fw));
        if (!result)
            LOG_ERROR("Firmware version isn't compatible" << fw_version);

        return result;
    }

    std::string ds_d400_update_device::get_firmware_min_version() const
    {
        uint16_t pid = 0;
        if( _usb_device != nullptr )
            pid = _usb_device->get_info().pid;
        else if( _mipi_device != nullptr )
            pid = _mipi_device->get_info().pid;
        auto it = ds::d400_device_to_fw_min_version.find( pid );
        if( it == ds::d400_device_to_fw_min_version.end() )
            throw librealsense::invalid_value_exception(
                rsutils::string::from()
                << "Minimum firmware version has not been defined for this device: "
                << std::hex << _pid );
        return it->second;
    }

    std::string ds_d400_update_device::parse_serial_number(const std::vector<uint8_t>& buffer) const
    {
        if (buffer.size() != sizeof(serial_number_data))
            throw std::runtime_error("DFU - failed to parse serial number!");

        std::stringstream rv;
        for (auto i = 0; i < ds::module_serial_size; i++)
            rv << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(buffer[i]);

        return rv.str();
    }
    float ds_d400_update_device::compute_progress(float progress, float start, float end, float threshold) const
    {
        return (progress*100);
    }
}
