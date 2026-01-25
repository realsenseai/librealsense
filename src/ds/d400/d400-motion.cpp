// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016-24 RealSense, Inc. All Rights Reserved.

#include "d400-motion.h"

#include <mutex>
#include <chrono>
#include <vector>
#include <map>
#include <iterator>
#include <cstddef>

#include <src/backend.h>
#include <src/platform/platform-utils.h>
#include <src/metadata.h>
#include "ds/ds-timestamp.h"
#include "d400-options.h"
#include "d400-info.h"
#include "stream.h"
#include "proc/motion-transform.h"
#include "proc/auto-exposure-processor.h"
#include <src/metadata-parser.h>
#include <src/hid-sensor.h>

#include <rsutils/type/fourcc.h>
using rsutils::type::fourcc;


namespace librealsense
{
    // D457 development
    const std::map<fourcc::value_type, rs2_format> motion_fourcc_to_rs2_format = {
        {fourcc('G','R','E','Y'), RS2_FORMAT_MOTION_XYZ32F},
    };
    const std::map<fourcc::value_type, rs2_stream> motion_fourcc_to_rs2_stream = {
        {fourcc('G','R','E','Y'), RS2_STREAM_ACCEL},
    };

    rs2_motion_device_intrinsic d400_motion_base::get_motion_intrinsics(rs2_stream stream) const
    {
        return _ds_motion_common->get_motion_intrinsics(stream);
    }

    bool d400_motion_base::is_imu_high_accuracy() const
    {
        // D400 FW 5.16 and above use 32 bits in the struct, instead of 16.
        return _fw_version >= firmware_version( 5, 16, 0, 0 ); 
    }

    double d400_motion_base::get_gyro_default_scale() const
    {
        // FW scale in the HID feature report was 10 up to FW version 5.16, changed to 1000 to support gyro sensitivity option.
        // D400 FW performs conversion from raw to physical, we get [deg/sec] values.
        return _fw_version >= firmware_version( 5, 16, 0, 0 ) ? 0.0001 : 0.1;
    }

    std::shared_ptr<synthetic_sensor> d400_motion_uvc::create_uvc_device(std::shared_ptr<context> ctx,
                                                  const std::vector<platform::uvc_device_info>& all_uvc_infos,
                                                  const firmware_version& camera_fw_version)
    {
        if (all_uvc_infos.empty())
        {
            LOG_WARNING("No UVC info provided, IMU is disabled");
            return nullptr;
        }

        std::vector<std::shared_ptr<platform::uvc_device>> imu_devices;
        for (auto&& info : filter_by_mi(all_uvc_infos, 4)) // Filter just mi=4, IMU
            imu_devices.push_back( get_backend()->create_uvc_device( info ) );

        std::unique_ptr< frame_timestamp_reader > timestamp_reader_backup( new ds_timestamp_reader() );
        std::unique_ptr<frame_timestamp_reader> timestamp_reader_metadata(new ds_timestamp_reader_from_metadata_mipi_motion(std::move(timestamp_reader_backup)));

        auto enable_global_time_option = std::shared_ptr<global_time_option>(new global_time_option());

        auto raw_motion_ep = std::make_shared<uvc_sensor>("Raw IMU Sensor", std::make_shared<platform::multi_pins_uvc_device>(imu_devices),
             std::unique_ptr<frame_timestamp_reader>(new global_timestamp_reader(std::move(timestamp_reader_metadata), _tf_keeper, enable_global_time_option)), this);

        auto motion_ep = std::make_shared<ds_motion_sensor>("Motion Module", raw_motion_ep, this,
                                                            motion_fourcc_to_rs2_format, motion_fourcc_to_rs2_stream);

        motion_ep->register_option(RS2_OPTION_GLOBAL_TIME_ENABLED, enable_global_time_option);

        // register pre-processing
        std::shared_ptr<enable_motion_correction> mm_correct_opt = nullptr;

        auto mm_calib = _ds_motion_common->get_calib_handler();
        //  Motion intrinsic calibration presents is a prerequisite for motion correction.
        try
        {
            if (mm_calib)
            {
                mm_correct_opt = std::make_shared<enable_motion_correction>(motion_ep.get(),
                    option_range{ 0, 1, 1, 1 });
                motion_ep->register_option(RS2_OPTION_ENABLE_MOTION_CORRECTION, mm_correct_opt);
            }
        }
        catch (...) {}

        double gyro_scale_factor = get_gyro_default_scale();
        bool high_accuracy = is_imu_high_accuracy();
        motion_ep->register_processing_block(
            { {RS2_FORMAT_MOTION_XYZ32F} },
            { {RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_ACCEL}, {RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_GYRO} },
            [&, mm_calib, high_accuracy, mm_correct_opt, gyro_scale_factor]()
            { return std::make_shared< motion_to_accel_gyro >( mm_calib, mm_correct_opt, gyro_scale_factor, high_accuracy );
        });

        return motion_ep;
    }


    std::shared_ptr<synthetic_sensor> d400_motion::create_hid_device( std::shared_ptr<context> ctx,
                                                                      const std::vector<platform::hid_device_info>& all_hid_infos )
    {
        return _ds_motion_common->create_hid_device( ctx, all_hid_infos, _tf_keeper );
    }

    d400_motion_base::d400_motion_base( std::shared_ptr< const d400_info > const & dev_info )
        : device(dev_info),
        d400_device(dev_info),
        _accel_stream(new stream(RS2_STREAM_ACCEL)),
        _gyro_stream(new stream(RS2_STREAM_GYRO))
    {
        try
        {
            _ds_motion_common = std::make_shared<ds_motion_common>(this, _fw_version,
                                                                   _device_capabilities, _hw_monitor);
        } catch (...)
        {
            LOG_ERROR("Base Motion Sensor Failure!");
        }

    }

    d400_motion::d400_motion( std::shared_ptr< const d400_info > const & dev_info )
        : device(dev_info), 
        d400_device(dev_info),
        d400_motion_base(dev_info)
    {
        try {
            using namespace ds;

            std::vector<platform::hid_device_info> hid_infos = dev_info->get_group().hid_devices;

            _ds_motion_common->init_motion(hid_infos.empty(), *_depth_stream);

            initialize_fisheye_sensor( dev_info->get_context(), dev_info->get_group() );

#if !defined(__APPLE__) // Motion sensors not supported on macOS
            // Try to add HID endpoint
            auto hid_ep = create_hid_device( dev_info->get_context(), dev_info->get_group().hid_devices );
            if (hid_ep)
            {
                _motion_module_device_idx = static_cast<uint8_t>(add_sensor(hid_ep));

                // HID metadata attributes
                hid_ep->get_raw_sensor()->register_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP, make_hid_header_parser(&hid_header::timestamp));
            }
            //for FW >=5.16 the scale factor changes to 1000.0 since FW sends 32bit
            if (_fw_version >= firmware_version( 5, 16, 0, 0))
                get_raw_motion_sensor()->set_gyro_scale_factor( 10000.0 );
#endif
        }
        catch (...) {
            LOG_ERROR("HID Motion Sensor Failure!");
        }
    }


    ds_motion_sensor & d400_motion::get_motion_sensor()
    {
#if defined(__APPLE__)
        throw std::runtime_error("Motion sensors are not supported on macOS");
#else
        return dynamic_cast< ds_motion_sensor & >( get_sensor( _motion_module_device_idx.value() ) );
#endif
    }

    std::shared_ptr<hid_sensor> d400_motion::get_raw_motion_sensor()
    {
#if defined(__APPLE__)
        return nullptr;
#else
        auto raw_sensor = get_motion_sensor().get_raw_sensor();
        return std::dynamic_pointer_cast< hid_sensor >( raw_sensor );
#endif
    }

    d400_motion_uvc::d400_motion_uvc( std::shared_ptr< const d400_info > const & dev_info )
        : device(dev_info),
          d400_device(dev_info),
          d400_motion_base(dev_info)
    {
        try
        {
            using namespace ds;

            std::vector<platform::uvc_device_info> uvc_infos = dev_info->get_group().uvc_devices;

            _ds_motion_common->init_motion(uvc_infos.empty(), *_depth_stream);

            if (!uvc_infos.empty())
            {
                // product id - D457 dev - check - must not be the front of uvc_infos vector
                _pid = uvc_infos.front().pid;
            }

#if !defined(__APPLE__) // Motion sensors not supported on macOS
            // Try to add HID endpoint
            std::shared_ptr<synthetic_sensor> sensor_ep;
            sensor_ep = create_uvc_device(dev_info->get_context(), dev_info->get_group().uvc_devices, _fw_version);
            if (sensor_ep)
            {
                _motion_module_device_idx = static_cast<uint8_t>(add_sensor(sensor_ep));

                // HID metadata attributes - D457 dev - check metadata parser
                sensor_ep->get_raw_sensor()->register_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP, make_hid_header_parser(&hid_header::timestamp));
            }
#endif
        } catch (...)
        {
            LOG_ERROR("UVC Motion Sensor Failure!");
        }

    }

    void d400_motion::initialize_fisheye_sensor(std::shared_ptr<context> ctx, const platform::backend_device_group& group)
    {
        using namespace ds;

        bool is_fisheye_avaialable = false;
        auto fisheye_infos = _ds_motion_common->init_fisheye(group, is_fisheye_avaialable);
        if (!is_fisheye_avaialable)
            return;

        std::unique_ptr< frame_timestamp_reader > ds_timestamp_reader_backup( new ds_timestamp_reader() );
        std::unique_ptr<frame_timestamp_reader> ds_timestamp_reader_metadata(new ds_timestamp_reader_from_metadata(std::move(ds_timestamp_reader_backup)));
        auto enable_global_time_option = std::shared_ptr<global_time_option>(new global_time_option());
        auto raw_fisheye_ep
            = std::make_shared< uvc_sensor >( "FishEye Sensor",
                                              get_backend()->create_uvc_device( fisheye_infos.front() ),
                                              std::unique_ptr< frame_timestamp_reader >( new global_timestamp_reader(
                                                  std::move( ds_timestamp_reader_metadata ),
                                                  _tf_keeper,
                                                  enable_global_time_option ) ),
                                              this );
        auto fisheye_ep = std::make_shared<ds_fisheye_sensor>(raw_fisheye_ep, this);
        
        _ds_motion_common->assign_fisheye_ep(raw_fisheye_ep, fisheye_ep, enable_global_time_option);
        
        register_fisheye_options();

        register_fisheye_metadata();

        // Add fisheye endpoint
        _fisheye_device_idx = add_sensor(fisheye_ep);
    }

    void d400_motion::register_fisheye_options()
    {
        _ds_motion_common->register_fisheye_options();
    }

    void d400_motion::register_fisheye_metadata()
    {
        _ds_motion_common->register_fisheye_metadata();
    }

    void d400_motion::register_stream_to_extrinsic_group(const stream_interface& stream, uint32_t group_index)
    {
        device::register_stream_to_extrinsic_group(stream, group_index);
    }
}
