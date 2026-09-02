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
#include <src/context.h>
#include <src/backend.h>
#include <src/platform/platform-utils.h>
#include "ds/ds-timestamp.h"
#include "ds/ds-options.h"
#include "ds/ds-private.h"
#include "d500-info.h"
#include "d500-options.h"
#include "stream.h"
#include "proc/motion-transform.h"
#include "proc/auto-exposure-processor.h"
#include "backend.h"
#include <src/metadata-parser.h>
#include <src/hid-sensor.h>
#include <src/ds/features/gyro-sensitivity-feature.h>

#include <rsutils/type/fourcc.h>
using rsutils::type::fourcc;

using namespace librealsense;
namespace librealsense
{
    namespace
    {
        constexpr double RAW_TO_DPS_SCALE = 10000.0;

        double gyro_scale_from_sensitivity( const option & sensitivity )
        {
            static constexpr double full_scale_dps[] = { 2000., 1000., 500., 250., 125. };
            const float value = sensitivity.query();
            if( value != value || value < 0.f || value > 4.f )
                throw invalid_value_exception( "Invalid MIPI gyro sensitivity value" );
            const auto index = static_cast< size_t >( value );
            if( value != static_cast< float >( index ) )
                throw invalid_value_exception( "Invalid MIPI gyro sensitivity value" );
            return full_scale_dps[index] / 32768.;
        }
    }

    const std::map<fourcc::value_type, rs2_format> d500_motion_fourcc_to_rs2_format = {
        {fourcc('G','R','E','Y'), RS2_FORMAT_MOTION_XYZ32F},
    };
    const std::map<fourcc::value_type, rs2_stream> d500_motion_fourcc_to_rs2_stream = {
        {fourcc('G','R','E','Y'), RS2_STREAM_ACCEL},
    };

    rs2_motion_device_intrinsic d500_motion::get_motion_intrinsics(rs2_stream stream) const
    {
        if( _has_motion_module_failed )
            throw std::runtime_error( "Motion module is not available on this device" );
        return _ds_motion_common->get_motion_intrinsics(stream);
    }

    bool d500_motion::supports_physical_units() const
    {
        static const firmware_version min_fw_supporting_physical_units( "7.58.40672.12546" );
        return get_pid() != ds::D585S_PID && ! _is_mipi_device
            && _fw_version >= min_fw_supporting_physical_units;
    }

    bool d500_motion::is_imu_high_accuracy() const
    {
        return supports_physical_units();
    }

    double d500_motion::get_gyro_default_scale() const
    {
        if( supports_physical_units() )
            return 1. / RAW_TO_DPS_SCALE;

        // Legacy D500 reports signed 16-bit raw samples at a fixed 125 dps assumption.
        return 125. / 32768.;
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
            if (get_info(RS2_CAMERA_INFO_IMU_TYPE) == "IMU_Unknown")
                throw std::runtime_error("Motion Sensor Failure - IMU type not recognized");

            _ds_motion_common = std::make_shared<ds_motion_common>(this, _fw_version,
                                                                   _device_capabilities, _hw_monitor);

            using namespace ds;

#if !defined(__APPLE__) // Motion sensors not supported on macOS
            std::shared_ptr<synthetic_sensor> sensor_ep;
            if( _is_mipi_device )
            {
                // IMU is a UVC-based V4L2 node at mi=4. uvc_infos holds all UVC nodes, so gate motion on it specifically.
                // When absent, degrade to a motionless device, mirroring the HID path's nullptr-on-empty behavior.
                std::vector<platform::uvc_device_info> uvc_infos = dev_info->get_group().uvc_devices;
                bool no_imu = filter_by_mi(uvc_infos, 4).empty();
                _ds_motion_common->init_motion(no_imu, *_depth_stream);
                if (no_imu)
                    LOG_WARNING("No IMU node (mi=4) found, IMU is disabled");
                else
                    sensor_ep = create_uvc_device(dev_info->get_context(), uvc_infos);
            }
            else
            {
                // IMU is a HID device.
                std::vector<platform::hid_device_info> hid_infos = dev_info->get_group().hid_devices;
                _ds_motion_common->init_motion(hid_infos.empty(), *_depth_stream);
                sensor_ep = create_hid_device( dev_info->get_context(), hid_infos );
            }

            if (sensor_ep)
            {
                _motion_module_device_idx = static_cast<uint8_t>(add_sensor(sensor_ep));
                sensor_ep->get_raw_sensor()->register_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP, make_hid_header_parser(&hid_header::timestamp));
                register_gyro_sensitivity();
                if( supports_physical_units() )
                    get_raw_motion_sensor()->set_gyro_scale_factor( RAW_TO_DPS_SCALE );
            }
#endif
        }
        catch (const std::exception& e)
        {
            _has_motion_module_failed = true;
            auto device_name = get_info( RS2_CAMERA_INFO_NAME );
            auto serial = get_info( RS2_CAMERA_INFO_SERIAL_NUMBER );
            if( ! ds::is_partial_device_allowed( dev_info->get_context() ) )
            {
                LOG_ERROR( device_name << " #" << serial << " - Motion Sensor Failure! " << e.what() );
                throw;
            }
            LOG_WARNING( device_name << " #" << serial << " - Motion Sensor Failure (continuing as partial device): " << e.what() );
        }
    }

    std::shared_ptr<synthetic_sensor> d500_motion::create_uvc_device( std::shared_ptr<context> ctx,
        const std::vector<platform::uvc_device_info>& all_uvc_infos )
    {
        if (all_uvc_infos.empty())
        {
            LOG_WARNING("No UVC info provided, IMU is disabled");
            return nullptr;
        }

        std::vector<std::shared_ptr<platform::uvc_device>> imu_devices;
        for (auto&& info : filter_by_mi(all_uvc_infos, 4)) // Filter just mi=4, IMU
            imu_devices.push_back( get_backend()->create_uvc_device( info ) );

        if (imu_devices.empty())
            throw backend_exception("cannot access IMU sensor");

        std::unique_ptr< frame_timestamp_reader > timestamp_reader_backup( new ds_timestamp_reader() );
        std::unique_ptr<frame_timestamp_reader> timestamp_reader_metadata(new ds_timestamp_reader_from_metadata_mipi_motion(std::move(timestamp_reader_backup)));

        auto enable_global_time_option = std::shared_ptr<global_time_option>(new global_time_option());

        auto raw_motion_ep = std::make_shared<uvc_sensor>("Raw IMU Sensor", std::make_shared<platform::multi_pins_uvc_device>(imu_devices),
             std::unique_ptr<frame_timestamp_reader>(new global_timestamp_reader(std::move(timestamp_reader_metadata), _tf_keeper, enable_global_time_option)), this);

        auto motion_ep = std::make_shared<ds_motion_sensor>("Motion Module", raw_motion_ep, this,
                                                            d500_motion_fourcc_to_rs2_format, d500_motion_fourcc_to_rs2_stream);

        motion_ep->register_option(RS2_OPTION_GLOBAL_TIME_ENABLED, enable_global_time_option);

        std::shared_ptr< option > gyro_sensitivity_option;
        try
        {
            const std::map< float, std::string > descriptions = {
                { 0.f, "61.0 mDeg/Sec" },
                { 1.f, "30.5 mDeg/Sec" },
                { 2.f, "15.3 mDeg/Sec" },
                { 3.f, "7.6 mDeg/Sec" },
                { 4.f, "3.8 mDeg/Sec" },
            };
            auto candidate = std::make_shared< d500_mipi_gyro_sensitivity_option >(
                raw_motion_ep, descriptions );
            const auto range = candidate->get_range();
            if( range.min == 0.f && range.max == 4.f && range.step == 1.f
                && range.def == 4.f )
            {
                (void)gyro_scale_from_sensitivity( *candidate );
                gyro_sensitivity_option = candidate;
                motion_ep->register_option( RS2_OPTION_GYRO_SENSITIVITY, candidate );
            }
        }
        catch( const std::exception & e )
        {
            LOG_DEBUG( "MIPI gyro sensitivity control is unavailable: " << e.what() );
        }

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
            [mm_calib, high_accuracy, mm_correct_opt, gyro_scale_factor, gyro_sensitivity_option]()
            {
                const double scale = gyro_sensitivity_option
                    ? gyro_scale_from_sensitivity( *gyro_sensitivity_option )
                    : gyro_scale_factor;
                return std::make_shared< motion_to_accel_gyro >( mm_calib, mm_correct_opt, scale, high_accuracy );
        });

        return motion_ep;
    }

    ds_motion_sensor & d500_motion::get_motion_sensor()
    {
#if defined(__APPLE__)
        throw std::runtime_error( "Motion sensors are not supported on macOS" );
#else
        return dynamic_cast< ds_motion_sensor & >( get_sensor( _motion_module_device_idx.value() ) );
#endif
    }

    std::shared_ptr< hid_sensor > d500_motion::get_raw_motion_sensor()
    {
#if defined(__APPLE__)
        return nullptr;
#else
        auto raw_sensor = get_motion_sensor().get_raw_sensor();
        return std::dynamic_pointer_cast< hid_sensor >( raw_sensor );
#endif
    }

    void d500_motion::register_gyro_sensitivity()
    {
        if( supports_physical_units() && ! _has_motion_module_failed )
        {
            auto raw_motion_sensor = get_raw_motion_sensor();
            raw_motion_sensor->enable_gyro_sensitivity_range_index();
            register_feature(
                std::make_shared< gyro_sensitivity_feature >(
                    raw_motion_sensor, get_motion_sensor(), 4.f ) );
        }
    }

    void d500_motion::register_stream_to_extrinsic_group(const stream_interface& stream, uint32_t group_index)
    {
        device::register_stream_to_extrinsic_group(stream, group_index);
    }
}
