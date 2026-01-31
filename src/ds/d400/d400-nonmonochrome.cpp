// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016 RealSense, Inc. All Rights Reserved.

#include <mutex>
#include <chrono>
#include <vector>
#include <iterator>
#include <cstddef>

#include "device.h"
#include "image.h"
#include "metadata-parser.h"

#include "d400-nonmonochrome.h"
#include "d400-private.h"
#include "d400-options.h"
#include "d400-info.h"
#include "ds/ds-timestamp.h"
#include "proc/color-formats-converter.h"
#include "proc/depth-formats-converter.h"

namespace librealsense
{
    d400_nonmonochrome::d400_nonmonochrome( std::shared_ptr< const d400_info > const & dev_info )
        : device(dev_info), d400_device(dev_info)
    {
        using namespace ds;

        auto pid = dev_info->get_group().uvc_devices.front().pid;
        auto& depth_ep = get_depth_sensor();

        // RGB for D455 from Left Imager is available with FW 5.12.8.100
        if ((val_in_range(pid, { RS455_PID })) && (_fw_version < firmware_version("5.12.8.100")))
            return;

        if ((_fw_version >= firmware_version("5.5.8.0")) && (!val_in_range(pid, { RS_USB2_PID })))
        {
            if (!val_in_range(pid, { RS405_PID, RS455_PID, RS401_GMSL_PID }))
            {
                depth_ep.register_option(RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE,
                    std::make_shared<uvc_xu_option<uint8_t>>(get_raw_depth_sensor(),
                                                         depth_xu,
                                                         DS5_ENABLE_AUTO_WHITE_BALANCE,
                                                         "Enable Auto White Balance"));

                depth_ep.register_processing_block({ {RS2_FORMAT_W10} }, { {RS2_FORMAT_RAW10, RS2_STREAM_INFRARED, 1} }, []() { return std::make_shared<w10_converter>(RS2_FORMAT_RAW10); });
                depth_ep.register_processing_block({ {RS2_FORMAT_W10} }, { {RS2_FORMAT_Y10BPACK, RS2_STREAM_INFRARED, 1} }, []() { return std::make_shared<w10_converter>(RS2_FORMAT_Y10BPACK); });
            }

            // RS400 rolling-shutter SKUs allow to get low-quality color image from the same viewport as the depth
            depth_ep.register_processing_block({ {RS2_FORMAT_BGR8} }, { {RS2_FORMAT_RGB8, RS2_STREAM_INFRARED} }, []() { return std::make_shared<bgr_to_rgb>(); });
        }

        if( _is_mipi_device )
        {
            // Work-around for discrepancy between the RGB YUYV descriptor and the parser. Use UYUV parser instead.
            // Bytes are received swapped, so YUYV format is received as UYVY.
            depth_ep.register_processing_block( processing_block_factory::create_pbf_vector< uyvy_converter >(
                RS2_FORMAT_YUYV,
                map_supported_color_formats( RS2_FORMAT_YUYV, false ),
                RS2_STREAM_INFRARED ) );
            depth_ep.register_processing_block( { { RS2_FORMAT_YUYV } },
                                                { { RS2_FORMAT_YUYV, RS2_STREAM_INFRARED } },
                                                []() { return std::make_shared< uyvy_to_yuyv >(); } );
        }
        else
        {
            depth_ep.register_processing_block( processing_block_factory::create_pbf_vector< uyvy_converter >(
                RS2_FORMAT_UYVY,
                map_supported_color_formats( RS2_FORMAT_UYVY ),
                RS2_STREAM_INFRARED ) );
        }

        if (!val_in_range(pid, { RS405_PID , RS455_PID }))
            get_depth_sensor().unregister_option(RS2_OPTION_EMITTER_ON_OFF);

        if ((_fw_version >= firmware_version("5.9.13.6") &&
             _fw_version < firmware_version("5.9.15.1")))
        {
            get_depth_sensor().register_option(RS2_OPTION_INTER_CAM_SYNC_MODE,
                std::make_shared<external_sync_mode>(*_hw_monitor));
        }
    }
}
