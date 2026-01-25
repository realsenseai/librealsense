// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include "d500-motion.h"

#include <mutex>
#include <chrono>
#include <vector>
#include <map>
#include <iterator>
#include <cstddef>

#include <src/metadata.h>
#include "ds/ds-timestamp.h"
#include "ds/ds-options.h"
#include "d500-info.h"
#include "stream.h"
#include "proc/motion-transform.h"
#include "proc/auto-exposure-processor.h"
#include "backend.h"
#include <src/metadata-parser.h>

using namespace librealsense;
namespace librealsense
{
    

    rs2_motion_device_intrinsic d500_motion::get_motion_intrinsics(rs2_stream stream) const
    {
        return _ds_motion_common->get_motion_intrinsics(stream);
    }

    double d500_motion::get_gyro_default_scale() const
    {
        // D585S outputs raw 16 bit register value, dynamic range +/-125 [deg/sec] --> 250/65536=0.003814697265625 [deg/sec/LSB]
        return 0.003814697265625;
    }

    std::shared_ptr<synthetic_sensor> d500_motion::create_hid_device( std::shared_ptr<context> ctx,
                                                                      const std::vector<platform::hid_device_info>& all_hid_infos )
    {
        return _ds_motion_common->create_hid_device( ctx, all_hid_infos, _tf_keeper );
    }

    d500_motion::d500_motion( std::shared_ptr< const d500_info > const & dev_info )
        : device( dev_info )
        , d500_device( dev_info )
    {
        try
        {
            using namespace ds;

            std::vector<platform::hid_device_info> hid_infos = dev_info->get_group().hid_devices;

            _ds_motion_common = std::make_shared<ds_motion_common>(this, _fw_version,
                                                                   _device_capabilities, _hw_monitor);
            _ds_motion_common->init_motion(hid_infos.empty(), *_depth_stream);

#if !defined(__APPLE__) // Motion sensors not supported on macOS
            // Try to add HID endpoint
            auto hid_ep = create_hid_device( dev_info->get_context(), dev_info->get_group().hid_devices );
            if (hid_ep)
            {
                _motion_module_device_idx = static_cast<uint8_t>(add_sensor(hid_ep));

                // HID metadata attributes
                hid_ep->get_raw_sensor()->register_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP, make_hid_header_parser(&hid_header::timestamp));
            }
#endif
        }
        catch (...)
        {
            LOG_ERROR("HID Motion Sensor Failure!");
        }

    }

    void d500_motion::register_stream_to_extrinsic_group(const stream_interface& stream, uint32_t group_index)
    {
        device::register_stream_to_extrinsic_group(stream, group_index);
    }
}
