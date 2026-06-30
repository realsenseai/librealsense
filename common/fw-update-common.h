// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// Shared firmware-update utilities used by both the realsense-viewer and
// the rs-fw-update CLI tool.  Header-only (inline) so the tool can include
// it without linking against the common/ library.

#pragma once

#include <librealsense2/rs.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace rs2
{
namespace fw_update
{

    inline bool is_mipi_device( const rs2::device & dev )
    {
        return dev.supports( RS2_CAMERA_INFO_CONNECTION_TYPE )
            && std::string( dev.get_info( RS2_CAMERA_INFO_CONNECTION_TYPE ) ) == "GMSL";
    }

    inline bool is_mipi_recovery_device( const rs2::device & dev )
    {
        return dev.supports( RS2_CAMERA_INFO_PRODUCT_ID )
            && std::string( dev.get_info( RS2_CAMERA_INFO_PRODUCT_ID ) ) == "BBCD";
    }

    // Returns false when firmware is not compatible, true otherwise.
    // Throws if the device cannot be used as updatable.
    inline bool check_fw_compatibility( const rs2::device & dev,
                                        const std::vector< uint8_t > & fw_image )
    {
        auto upd = dev.as< rs2::updatable >();
        if( ! upd )
            throw std::runtime_error( "Device could not be used as updatable device" );
        return upd.check_firmware_compatibility( fw_image );
    }

    inline std::string get_update_serial( const rs2::device & dev )
    {
        if( dev.supports( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID ) )
            return dev.get_info( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID );
        auto sensors = dev.query_sensors();
        if( sensors.size() && sensors.front().supports( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID ) )
            return sensors.front().get_info( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID );
        throw std::runtime_error( "Device does not provide a firmware update serial number" );
    }

    static const char * mipi_recovery_message =
        "For GMSL MIPI device please reboot, or reload d4xx driver\n"
        "sudo rmmod d4xx && sudo modprobe d4xx";

}  // namespace fw_update
}  // namespace rs2
