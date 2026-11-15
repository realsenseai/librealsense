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
        // Raw counts per dps on the HID path (set_gyro_scale_factor), and its reciprocal as the default scale.
        constexpr double RAW_TO_DPS_SCALE = 10000.0;

        // First FW that sends the true int32 IMU record on GMSL, with accel at 10 ug per count on USB and GMSL.
        const firmware_version MIN_FW_INT32_IMU( "7.59.46582.16434" );

        // First FW that sends the GMSL gyro in 0.0001 dps per count, independent of the range option.
        const firmware_version MIN_FW_PHYSICAL_GYRO_GMSL( "7.59.46653.0" );
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
        return get_pid() != ds::D585S_PID
            && _fw_version >= ( _is_mipi_device ? MIN_FW_INT32_IMU
                                                : min_fw_supporting_physical_units );
    }

    bool d500_motion::supports_accel_range() const
    {
        // The FW acts on the accel level from the same release that reports accel at 10 ug per count. Not on D585S.
        return supports_physical_units() && _fw_version >= MIN_FW_INT32_IMU;
    }

    bool d500_motion::supports_physical_gyro() const
    {
        return supports_physical_units() && ( ! _is_mipi_device || _fw_version >= MIN_FW_PHYSICAL_GYRO_GMSL );
    }

    bool d500_motion::is_imu_high_accuracy() const
    {
        return supports_physical_units();
    }

    double d500_motion::get_gyro_default_scale() const
    {
        // Older GMSL FW sends the gyro as the raw BMI08x register value, so only HID and newer GMSL FW have a fixed scale.
        if( supports_physical_gyro() )
            return 1. / RAW_TO_DPS_SCALE;

        // Legacy D500 reports signed 16-bit raw samples at a fixed 125 dps assumption.
        return 125. / 32768.;
    }

    double d500_motion::get_accel_default_scale() const
    {
        // FW changed the accel unit from 1 mg to 10 ug per count (USB and GMSL) in the same release that sends the
        // true int32 record on GMSL. The range option never changes this scale.
        return supports_accel_range() ? 0.00001 : 0.001;
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
                if( ! _is_mipi_device )
                    register_gyro_sensitivity();
                if( supports_physical_units() && ! _is_mipi_device )  // HID only, MIPI scales in the processing block
                    get_raw_motion_sensor()->set_gyro_scale_factor( RAW_TO_DPS_SCALE );
                // Windows MF rebuilds FW counts from g; derive the factor from the scale the transform uses (1000 or 100000).
                // Unconditional on purpose: the legacy value, 1000, equals the backend default.
                if( ! _is_mipi_device )
                    get_raw_motion_sensor()->set_accel_scale_factor( 1.0 / get_accel_default_scale() );
            }
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

        // Register a sensitivity control only when the driver reports exactly [0, max_level] with step 1.
        auto register_mipi_sensitivity = [&]( rs2_option id, float max_level ) -> std::shared_ptr< option >
        {
            try
            {
                auto candidate = std::make_shared< d500_mipi_imu_sensitivity_option >( raw_motion_ep, id );
                const auto range = candidate->get_range();
                if( range.min == 0.f && range.max == max_level && range.step == 1.f )
                {
                    motion_ep->register_option( id, candidate );
                    return candidate;
                }
                LOG_WARNING( "MIPI " << rs2_option_to_string( id ) << " control has unexpected range ["
                             << range.min << ", " << range.max << ", " << range.step << "]" );
            }
            catch( const std::exception & e )
            {
                LOG_WARNING( "MIPI " << rs2_option_to_string( id ) << " control is unavailable: " << e.what() );
            }
            return nullptr;
        };
        auto gyro_sensitivity_option = register_mipi_sensitivity( RS2_OPTION_GYRO_SENSITIVITY, RS2_GYRO_SENSITIVITY_COUNT - 1.f );
        if( supports_accel_range() )
            register_mipi_sensitivity( RS2_OPTION_ACCEL_SENSITIVITY, RS2_ACCEL_SENSITIVITY_COUNT - 1.f );

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
        double accel_scale_factor = get_accel_default_scale();
        bool high_accuracy = is_imu_high_accuracy();
        bool raw_gyro = ! supports_physical_gyro();
        motion_ep->register_processing_block(
            { {RS2_FORMAT_MOTION_XYZ32F} },
            { {RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_ACCEL}, {RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_GYRO} },
            [mm_calib, high_accuracy, mm_correct_opt, gyro_scale_factor, accel_scale_factor, gyro_sensitivity_option, raw_gyro]()
            {
                double scale = gyro_scale_factor;
                // Older MIPI FW gyro is a raw register value, its scale follows the selected range.
                if( raw_gyro && gyro_sensitivity_option )
                {
                    try
                    {
                        scale = gyro_sensitivity_to_scale( gyro_sensitivity_option->query() );
                    }
                    catch( const std::exception & e )
                    {
                        LOG_WARNING( "Failed to read gyro sensitivity; using the fallback scale: " << e.what() );
                    }
                }
                return std::make_shared< motion_to_accel_gyro >( mm_calib, mm_correct_opt, scale, accel_scale_factor, high_accuracy );
        });

        return motion_ep;
    }

    ds_motion_sensor & d500_motion::get_motion_sensor()
    {
        return dynamic_cast< ds_motion_sensor & >( get_sensor( _motion_module_device_idx.value() ) );
    }

    std::shared_ptr< hid_sensor > d500_motion::get_raw_motion_sensor()
    {
        auto raw_sensor = get_motion_sensor().get_raw_sensor();
        return std::dynamic_pointer_cast< hid_sensor >( raw_sensor );
    }

    void d500_motion::register_gyro_sensitivity()
    {
        if( supports_physical_units() && ! _is_mipi_device && ! _has_motion_module_failed )
        {
            auto raw_motion_sensor = get_raw_motion_sensor();
            raw_motion_sensor->enable_gyro_sensitivity_range_index();
            register_feature(
                std::make_shared< gyro_sensitivity_feature >(
                    raw_motion_sensor, get_motion_sensor(), 4.f ) );
            if( supports_accel_range() )
                get_motion_sensor().register_option( RS2_OPTION_ACCEL_SENSITIVITY,
                                                     std::make_shared< d500_hid_accel_sensitivity_option >( raw_motion_sensor ) );
        }
    }

    void d500_motion::register_stream_to_extrinsic_group(const stream_interface& stream, uint32_t group_index)
    {
        device::register_stream_to_extrinsic_group(stream, group_index);
    }
}
