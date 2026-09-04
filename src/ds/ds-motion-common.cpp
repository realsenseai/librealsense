// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022-4 RealSense, Inc. All Rights Reserved.

#include "ds-motion-common.h"

#include "algo.h"
#include "hid-sensor.h"
#include "environment.h"
#include "metadata.h"
#include "backend.h"
#include "platform/platform-utils.h"

#include "global_timestamp_reader.h"
#include "proc/auto-exposure-processor.h"
#include "core/roi.h"

#include "d400/d400-motion.h"
#include "d500/d500-motion.h"
#include "proc/motion-transform.h"

#include "ds-timestamp.h"
#include "ds-options.h"
#include "ds-private.h"
#include <src/stream.h>
#include <src/metadata-parser.h>

#include <rsutils/type/fourcc.h>
using rsutils::type::fourcc;
#include <cstddef>

namespace librealsense
{
    using namespace ds;

    const std::map<fourcc::value_type, rs2_format> fisheye_fourcc_to_rs2_format = {
        {fourcc('R','A','W','8'), RS2_FORMAT_RAW8},
        {fourcc('G','R','E','Y'), RS2_FORMAT_RAW8},
    };
    const std::map<fourcc::value_type, rs2_stream> fisheye_fourcc_to_rs2_stream = {
        {fourcc('R','A','W','8'), RS2_STREAM_FISHEYE},
        {fourcc('G','R','E','Y'), RS2_STREAM_FISHEYE},
    };

    void fisheye_auto_exposure_roi_method::set(const region_of_interest& roi)
    {
        _auto_exposure->update_auto_exposure_roi(roi);
        _roi = roi;
    }

    region_of_interest fisheye_auto_exposure_roi_method::get() const
    {
        return _roi;
    }

    ds_fisheye_sensor::ds_fisheye_sensor( std::shared_ptr< raw_sensor_base > const & sensor, device * owner )
        : synthetic_sensor("Wide FOV Camera", sensor, owner, fisheye_fourcc_to_rs2_format, fisheye_fourcc_to_rs2_stream),
        _owner(owner)
    {}

    const std::vector<uint8_t>& ds_fisheye_sensor::get_fisheye_calibration_table() const
    {
        if (auto dev = dynamic_cast<const d400_motion*>(_owner))
            return dev->_ds_motion_common->get_fisheye_calibration_table();
        if (auto dev = dynamic_cast<const d400_motion_uvc*>(_owner))
            return dev->_ds_motion_common->get_fisheye_calibration_table();
        if( auto dev = dynamic_cast< const d500_motion * >( _owner ) )
            return dev->_ds_motion_common->get_fisheye_calibration_table();
        throw std::runtime_error("device not referenced in the product line");
    }

    std::shared_ptr<stream_interface> ds_fisheye_sensor::get_fisheye_stream() const
    {
        if (auto dev = dynamic_cast<const d400_motion*>(_owner))
            return dev->_ds_motion_common->get_fisheye_stream();
        if (auto dev = dynamic_cast<const d400_motion_uvc*>(_owner))
            return dev->_ds_motion_common->get_fisheye_stream();
        if( auto dev = dynamic_cast< const d500_motion * >( _owner ) )
            return dev->_ds_motion_common->get_fisheye_stream();
        throw std::runtime_error("device not referenced in the product line");
    }

    rs2_intrinsics ds_fisheye_sensor::get_intrinsics(const stream_profile& profile) const
    {
        // d400 used because no fisheye in ds6
        auto fisheye_calib = get_fisheye_calibration_table();
        return get_d400_intrinsic_by_resolution(
            fisheye_calib,
            ds::d400_calibration_table_id::fisheye_calibration_id,
            profile.width, profile.height);
    }

    stream_profiles ds_fisheye_sensor::init_stream_profiles()
    {
        auto lock = environment::get_instance().get_extrinsics_graph().lock();

        auto results = synthetic_sensor::init_stream_profiles();
        auto fisheye_stream = get_fisheye_stream();

        for (auto p : results)
        {
            // Register stream types
            if (p->get_stream_type() == RS2_STREAM_FISHEYE)
                assign_stream(fisheye_stream, p);

            auto video = dynamic_cast<video_stream_profile_interface*>(p.get());
            if (!video)
                throw std::runtime_error("Stream profile interface is not video stream profile interface");

            auto profile = to_profile(p.get());
            std::weak_ptr<ds_fisheye_sensor> wp =
                std::dynamic_pointer_cast<ds_fisheye_sensor>(this->shared_from_this());
            video->set_intrinsics([profile, wp]()
                {
                    auto sp = wp.lock();
                    if (sp)
                        return sp->get_intrinsics(profile);
                    else
                        return rs2_intrinsics{};
                });
        }

        return results;
    }

    std::shared_ptr<uvc_sensor> ds_fisheye_sensor::get_raw_sensor()
    {
        auto uvc_raw_sensor = As<uvc_sensor, sensor_base>(synthetic_sensor::get_raw_sensor());
        return uvc_raw_sensor;
    }
    
    ds_motion_sensor::ds_motion_sensor( std::string const & name,
                                        std::shared_ptr< raw_sensor_base > const & sensor,
                                        device * owner )
        : synthetic_sensor( name, sensor, owner )
        , _owner( owner )
    {
    }

    ds_motion_sensor::ds_motion_sensor( std::string const & name,
                                        std::shared_ptr< raw_sensor_base > const & sensor,
                                        device * owner,
                                        const std::map< uint32_t, rs2_format > & motion_fourcc_to_rs2_format,
                                        const std::map< uint32_t, rs2_stream > & motion_fourcc_to_rs2_stream )
        : synthetic_sensor( name, sensor, owner, motion_fourcc_to_rs2_format, motion_fourcc_to_rs2_stream )
        , _owner( owner )
    {
    }

    rs2_motion_device_intrinsic ds_motion_sensor::get_motion_intrinsics(rs2_stream stream) const
    {
        if (auto dev = dynamic_cast<const d400_motion*>(_owner))
            return dev->get_motion_intrinsics(stream);
        if (auto dev = dynamic_cast<const d400_motion_uvc*>(_owner))
            return dev->get_motion_intrinsics(stream);
        if( auto dev = dynamic_cast< const d500_motion * >( _owner ) )
            return dev->get_motion_intrinsics( stream );
        throw std::runtime_error("device not referenced in the product line");
    }

    stream_profiles ds_motion_sensor::init_stream_profiles()
    {
        auto lock = environment::get_instance().get_extrinsics_graph().lock();
        auto results = synthetic_sensor::init_stream_profiles();

        auto accel_stream = get_accel_stream();
        auto gyro_stream = get_gyro_stream();

        for (auto p : results)
        {
            // Register stream types
            if (p->get_stream_type() == RS2_STREAM_ACCEL)
                assign_stream(accel_stream, p);
            if (p->get_stream_type() == RS2_STREAM_GYRO)
                assign_stream(gyro_stream, p);

            //set motion intrinsics
            if (p->get_stream_type() == RS2_STREAM_ACCEL || p->get_stream_type() == RS2_STREAM_GYRO)
            {
                auto motion = dynamic_cast<motion_stream_profile_interface*>(p.get());
                if (!motion)
                    throw std::runtime_error("Stream profile is not motion stream profile");

                auto st = p->get_stream_type();
                motion->set_intrinsics([this, st]() { return get_motion_intrinsics(st); });
            }
        }

        return results;
    }

    std::shared_ptr<stream_interface> ds_motion_sensor::get_accel_stream() const
    {
        if (auto dev = dynamic_cast<const d400_motion*>(_owner))
            return dev->_ds_motion_common->get_accel_stream();
        if (auto dev = dynamic_cast<const d400_motion_uvc*>(_owner))
            return dev->_ds_motion_common->get_accel_stream();
        if( auto dev = dynamic_cast< const d500_motion * >( _owner ) )
            return dev->_ds_motion_common->get_accel_stream();
        throw std::runtime_error("device not referenced in the product line");
    }

    std::shared_ptr<stream_interface> ds_motion_sensor::get_gyro_stream() const
    {
        if (auto dev = dynamic_cast<const d400_motion*>(_owner))
            return dev->_ds_motion_common->get_gyro_stream();
        if (auto dev = dynamic_cast<const d400_motion_uvc*>(_owner))
            return dev->_ds_motion_common->get_gyro_stream();
        if( auto dev = dynamic_cast< const d500_motion * >( _owner ) )
            return dev->_ds_motion_common->get_gyro_stream();
        throw std::runtime_error("device not referenced in the product line");
    }

    std::shared_ptr<auto_exposure_mechanism> ds_motion_common::register_auto_exposure_options(synthetic_sensor* ep, const platform::extension_unit* fisheye_xu)
    {
        auto uvc_raw_sensor = As<uvc_sensor, sensor_base>(ep->get_raw_sensor());
        auto gain_option = std::make_shared<uvc_pu_option>(uvc_raw_sensor, RS2_OPTION_GAIN);

        auto exposure_option = std::make_shared<uvc_xu_option<uint16_t>>(uvc_raw_sensor,
            *fisheye_xu,
            librealsense::ds::FISHEYE_EXPOSURE, "Exposure time of Fisheye camera");

        auto ae_state = std::make_shared<auto_exposure_state>();
        auto auto_exposure = std::make_shared<auto_exposure_mechanism>(*gain_option, *exposure_option, *ae_state);

        auto auto_exposure_option = std::make_shared<enable_auto_exposure_option>(ep,
            auto_exposure,
            ae_state,
            option_range{ 0, 1, 1, 1 });

        ep->register_option(RS2_OPTION_ENABLE_AUTO_EXPOSURE, auto_exposure_option);

        ep->register_option(RS2_OPTION_AUTO_EXPOSURE_MODE,
            std::make_shared<auto_exposure_mode_option>(auto_exposure,
                ae_state,
                option_range{ 0, 2, 1, 0 },
                std::map<float, std::string>{ {0.f, "Static"},
                { 1.f, "Anti-Flicker" },
                { 2.f, "Hybrid" }}));
        ep->register_option(RS2_OPTION_AUTO_EXPOSURE_CONVERGE_STEP,
            std::make_shared<auto_exposure_step_option>(auto_exposure,
                ae_state,
                option_range{ 0.1f, 1.0f, 0.1f, ae_step_default_value }));
        ep->register_option(RS2_OPTION_POWER_LINE_FREQUENCY,
            std::make_shared<auto_exposure_antiflicker_rate_option>(auto_exposure,
                ae_state,
                option_range{ 50, 60, 10, 60 },
                std::map<float, std::string>{ {50.f, "50Hz"},
                { 60.f, "60Hz" }}));

        ep->register_option(RS2_OPTION_GAIN,
            std::make_shared<auto_disabling_control>(
                gain_option,
                auto_exposure_option));

        ep->register_option(RS2_OPTION_EXPOSURE,
            std::make_shared<auto_disabling_control>(
                exposure_option,
                auto_exposure_option));

        ep->register_processing_block(
            { {RS2_FORMAT_RAW8} },
            { {RS2_FORMAT_RAW8, RS2_STREAM_FISHEYE} },
            [auto_exposure_option]() {
                return std::make_shared<auto_exposure_processor>(RS2_STREAM_FISHEYE, *auto_exposure_option);
            }
        );
        return auto_exposure;
    }

    ds_motion_common::ds_motion_common( backend_device * owner,
        firmware_version fw_version,
        const ds::ds_caps& device_capabilities,
        std::shared_ptr<hw_monitor> hwm) :
        _owner(owner),
        _fw_version(fw_version),
        _device_capabilities(device_capabilities),
        _hw_monitor(hwm),
        _fisheye_stream(new stream(RS2_STREAM_FISHEYE)),
        _accel_stream(new stream(RS2_STREAM_ACCEL)),
        _gyro_stream(new stream(RS2_STREAM_GYRO))
    {
        _sensor_name_and_hid_profiles =
        { { gyro_sensor_name,     {RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_GYRO, 0, 1, 1, int(odr::IMU_FPS_200)}},
          { gyro_sensor_name,     {RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_GYRO, 0, 1, 1, int(odr::IMU_FPS_400)}} };

        _fps_and_sampling_frequency_per_rs2_stream =
        { { RS2_STREAM_GYRO,     {{unsigned(odr::IMU_FPS_200),  hid_fps_translation.at(odr::IMU_FPS_200)},
                                 { unsigned(odr::IMU_FPS_400),  hid_fps_translation.at(odr::IMU_FPS_400)}}} };
    }

    rs2_motion_device_intrinsic ds_motion_common::get_motion_intrinsics(rs2_stream stream) const
    {
        if (stream == RS2_STREAM_ACCEL)
            return ds::create_motion_intrinsics(**_accel_intrinsic);

        if (stream == RS2_STREAM_GYRO)
            return ds::create_motion_intrinsics(**_gyro_intrinsic);

        throw std::runtime_error(rsutils::string::from() << "Motion Intrinsics unknown for stream " << rs2_stream_to_string(stream) << "!");
    }

    std::vector<platform::uvc_device_info> ds_motion_common::filter_device_by_capability(const std::vector<platform::uvc_device_info>& devices,
        ds_caps caps)
    {
        if (auto dev = dynamic_cast<const d400_motion*>(_owner))
            return filter_d400_device_by_capability(devices, ds::ds_caps::CAP_FISHEYE_SENSOR);
        if (auto dev = dynamic_cast<const d400_motion_uvc*>(_owner))
            return filter_d400_device_by_capability(devices, ds::ds_caps::CAP_FISHEYE_SENSOR);
        if( auto dev = dynamic_cast< const d500_motion * >( _owner ) )
            return std::vector< platform::uvc_device_info >();
        throw std::runtime_error("device not referenced in the product line");
    }

    std::vector<platform::uvc_device_info> ds_motion_common::init_fisheye(const platform::backend_device_group& group, bool& is_fisheye_available)
    {
        auto fisheye_infos = filter_by_mi(group.uvc_devices, 3);
        fisheye_infos = filter_device_by_capability(fisheye_infos, ds::ds_caps::CAP_FISHEYE_SENSOR);

        bool fe_dev_present = (fisheye_infos.size() == 1);
        bool fe_capability = (ds::ds_caps::CAP_UNDEFINED == _device_capabilities) ?
            true : !!(static_cast<uint32_t>(_device_capabilities & ds::ds_caps::CAP_FISHEYE_SENSOR));

        // Motion module w/o FishEye sensor
        if (!(fe_dev_present | fe_capability))
            return fisheye_infos;

        // Inconsistent FW
        if (fe_dev_present ^ fe_capability)
            throw invalid_value_exception(rsutils::string::from()
                << "Inconsistent HW/FW setup, FW FishEye capability = " << fe_capability
                << ", FishEye devices " << std::dec << fisheye_infos.size()
                << " while expecting " << fe_capability);
        
        is_fisheye_available = true;

        _fisheye_calibration_table_raw = [this]()
        {
            return _mm_calib->get_fisheye_calib_raw();
        };

        return fisheye_infos;
    }

    void ds_motion_common::assign_fisheye_ep(std::shared_ptr<uvc_sensor> raw_fisheye_ep,
        std::shared_ptr<synthetic_sensor> fisheye_ep,
        std::shared_ptr<global_time_option> enable_global_time_option)
    {
        _raw_fisheye_ep = raw_fisheye_ep;
        _fisheye_ep = fisheye_ep;
        _enable_global_time_option = enable_global_time_option;
    }

    void ds_motion_common::set_roi_method()
    {
        auto fisheye_auto_exposure = register_auto_exposure_options(_fisheye_ep.get(), &ds::fisheye_xu);
        if (auto fisheye_sensor = dynamic_cast<ds_fisheye_sensor*>(_fisheye_ep.get()))
            fisheye_sensor->set_roi_method(std::make_shared<fisheye_auto_exposure_roi_method>(fisheye_auto_exposure));
        else
            throw std::runtime_error("device not referenced in the product line");
    }

    void ds_motion_common::register_streams_to_extrinsic_groups()
    {
        if (auto dev = dynamic_cast<d400_motion*>(_owner))
        {
            dev->register_stream_to_extrinsic_group(*_gyro_stream, 0);
            dev->register_stream_to_extrinsic_group(*_accel_stream, 0);
        }
        else if (auto dev = dynamic_cast<d400_motion_uvc*>(_owner))
        {
            dev->register_stream_to_extrinsic_group(*_gyro_stream, 0);
            dev->register_stream_to_extrinsic_group(*_accel_stream, 0);
        }
        else if( auto dev = dynamic_cast< d500_motion * >( _owner ) )
        {
            dev->register_stream_to_extrinsic_group( *_gyro_stream, 0 );
            dev->register_stream_to_extrinsic_group( *_accel_stream, 0 );
        }
        else
            throw std::runtime_error("device not referenced in the product line");
    }

    void ds_motion_common::register_fisheye_options()
    {
        _fisheye_ep->register_option(RS2_OPTION_GLOBAL_TIME_ENABLED, _enable_global_time_option);
        _raw_fisheye_ep->register_xu(ds::fisheye_xu); // make sure the XU is initialized everytime we power the camera

        if (_fw_version >= firmware_version("5.6.3.0")) // Create Auto Exposure controls from FW version 5.6.3.0
        {
            set_roi_method();
        }
        else
        {
            _fisheye_ep->register_option(RS2_OPTION_GAIN,
                std::make_shared<uvc_pu_option>(_raw_fisheye_ep,
                    RS2_OPTION_GAIN));
            _fisheye_ep->register_option(RS2_OPTION_EXPOSURE,
                std::make_shared<uvc_xu_option<uint16_t>>(_raw_fisheye_ep,
                    ds::fisheye_xu,
                    librealsense::ds::FISHEYE_EXPOSURE,
                    "Exposure time of Fisheye camera"));
        }
    }

    void ds_motion_common::register_fisheye_metadata()
    {
        // Metadata registration
        _raw_fisheye_ep->register_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP, make_uvc_header_parser(&platform::uvc_header::timestamp));
        _raw_fisheye_ep->register_metadata(RS2_FRAME_METADATA_AUTO_EXPOSURE, make_additional_data_parser(&frame_additional_data::fisheye_ae_mode));

        // attributes of md_capture_timing
        auto md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_fisheye_mode, fisheye_mode) +
            offsetof(md_fisheye_normal_mode, intel_capture_timing);

        _raw_fisheye_ep->register_metadata(RS2_FRAME_METADATA_FRAME_COUNTER, make_attribute_parser(&md_capture_timing::frame_counter, md_capture_timing_attributes::frame_counter_attribute, md_prop_offset));
        _raw_fisheye_ep->register_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP, make_rs400_sensor_ts_parser(make_uvc_header_parser(&platform::uvc_header::timestamp),
            make_attribute_parser(&md_capture_timing::sensor_timestamp, md_capture_timing_attributes::sensor_timestamp_attribute, md_prop_offset)));

        // attributes of md_capture_stats
        md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_fisheye_mode, fisheye_mode) +
            offsetof(md_fisheye_normal_mode, intel_capture_stats);

        // attributes of md_capture_stats
        md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_fisheye_mode, fisheye_mode) +
            offsetof(md_fisheye_normal_mode, intel_configuration);

        _raw_fisheye_ep->register_metadata((rs2_frame_metadata_value)RS2_FRAME_METADATA_HW_TYPE, make_attribute_parser(&md_configuration::hw_type, md_configuration_attributes::hw_type_attribute, md_prop_offset));
        _raw_fisheye_ep->register_metadata((rs2_frame_metadata_value)RS2_FRAME_METADATA_SKU_ID, make_attribute_parser(&md_configuration::sku_id, md_configuration_attributes::sku_id_attribute, md_prop_offset));
        _raw_fisheye_ep->register_metadata((rs2_frame_metadata_value)RS2_FRAME_METADATA_FORMAT, make_attribute_parser(&md_configuration::format, md_configuration_attributes::format_attribute, md_prop_offset));
        _raw_fisheye_ep->register_metadata((rs2_frame_metadata_value)RS2_FRAME_METADATA_WIDTH, make_attribute_parser(&md_configuration::width, md_configuration_attributes::width_attribute, md_prop_offset));
        _raw_fisheye_ep->register_metadata((rs2_frame_metadata_value)RS2_FRAME_METADATA_HEIGHT, make_attribute_parser(&md_configuration::height, md_configuration_attributes::height_attribute, md_prop_offset));

        // attributes of md_fisheye_control
        md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_fisheye_mode, fisheye_mode) +
            offsetof(md_fisheye_normal_mode, intel_fisheye_control);

        _raw_fisheye_ep->register_metadata(RS2_FRAME_METADATA_GAIN_LEVEL, make_attribute_parser(&md_fisheye_control::manual_gain, md_depth_control_attributes::gain_attribute, md_prop_offset));
        _raw_fisheye_ep->register_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE, make_attribute_parser(&md_fisheye_control::manual_exposure, md_depth_control_attributes::exposure_attribute, md_prop_offset));

    }

    std::shared_ptr<synthetic_sensor> ds_motion_common::create_hid_device( std::shared_ptr<context> ctx, 
                                                                           const std::vector<platform::hid_device_info>& all_hid_infos, 
                                                                           std::shared_ptr<time_diff_keeper> tf_keeper )
    {
        if (all_hid_infos.empty())
        {
            LOG_WARNING("No HID info provided, IMU is disabled");
            return nullptr;
        }

        std::unique_ptr<frame_timestamp_reader> iio_hid_ts_reader(new iio_hid_timestamp_reader());
        std::unique_ptr<frame_timestamp_reader> custom_hid_ts_reader(new ds_custom_hid_timestamp_reader());
        auto enable_global_time_option = std::shared_ptr<global_time_option>(new global_time_option());

        // Dynamically populate the supported HID profiles according to the selected IMU module
        std::vector<odr> accel_fps_rates;
        std::map<unsigned, unsigned> fps_and_frequency_map;
        if ((_device_capabilities & ds::ds_caps::CAP_BMI_085) != ds::ds_caps::CAP_UNDEFINED || 
            (_device_capabilities & ds::ds_caps::CAP_BMI_088) != ds::ds_caps::CAP_UNDEFINED)
        {
               accel_fps_rates = { odr::IMU_FPS_100, odr::IMU_FPS_200 };
            if( _owner->supports_info( RS2_CAMERA_INFO_PRODUCT_LINE )
                && _owner->get_info( RS2_CAMERA_INFO_PRODUCT_LINE ) == "D400" )
            {
                // D400 new BMI's also support rate of 400 for the accelerometer
                accel_fps_rates.push_back( odr::IMU_FPS_400 );
            }
        }
        else // Applies to BMI_055 and unrecognized sensors
            accel_fps_rates = { odr::IMU_FPS_63,odr::IMU_FPS_250 };

        for (auto&& elem : accel_fps_rates)
        {
            _sensor_name_and_hid_profiles.push_back({ accel_sensor_name, { RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_ACCEL, 0, 1, 1, static_cast<uint16_t>(elem)} });
            fps_and_frequency_map.emplace(unsigned(elem), hid_fps_translation.at(elem));
        }
        _fps_and_sampling_frequency_per_rs2_stream[RS2_STREAM_ACCEL] = fps_and_frequency_map;

        auto raw_hid_ep = std::make_shared< hid_sensor >(
            _owner->get_backend()->create_hid_device( all_hid_infos.front() ),
            std::unique_ptr< frame_timestamp_reader >( new global_timestamp_reader( std::move( iio_hid_ts_reader ),
                                                                                    tf_keeper, enable_global_time_option ) ),
            std::unique_ptr< frame_timestamp_reader >( new global_timestamp_reader( std::move( custom_hid_ts_reader ),
                                                                                    tf_keeper,
                                                                                    enable_global_time_option ) ),
            _fps_and_sampling_frequency_per_rs2_stream,
            _sensor_name_and_hid_profiles,
            _owner );

        auto hid_ep = std::make_shared<ds_motion_sensor>("Motion Module", raw_hid_ep, _owner);

        hid_ep->register_option(RS2_OPTION_GLOBAL_TIME_ENABLED, enable_global_time_option);

        // register pre-processing
        std::shared_ptr<enable_motion_correction> mm_correct_opt = nullptr;

        //  Motion intrinsic calibration presents is a prerequisite for motion correction.
        try
        {
            if (_mm_calib)
            {
                mm_correct_opt = std::make_shared<enable_motion_correction>(hid_ep.get(),
                    option_range{ 0, 1, 1, 1 });
                hid_ep->register_option(RS2_OPTION_ENABLE_MOTION_CORRECTION, mm_correct_opt);
            }
        }
        catch (...) {}

        bool high_accuracy = _owner->is_imu_high_accuracy();    
        double accel_scale_factor = _owner->get_accel_default_scale();
        hid_ep->register_processing_block(
            { {RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_ACCEL} },
            { {RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_ACCEL} },
            [&, mm_correct_opt, accel_scale_factor, high_accuracy]()
            { return std::make_shared< acceleration_transform >( _mm_calib, mm_correct_opt, accel_scale_factor, high_accuracy );
            });

        double gyro_scale_factor = _owner->get_gyro_default_scale();
        hid_ep->register_processing_block(
            { {RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_GYRO} },
            { {RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_GYRO} },
            [&, mm_correct_opt, gyro_scale_factor, high_accuracy]()
            { return std::make_shared< gyroscope_transform >( _mm_calib, mm_correct_opt, gyro_scale_factor, high_accuracy );
            });

        return hid_ep;
    }

    void ds_motion_common::init_motion(bool is_infos_empty, const stream_interface& depth_stream)
    {
        if (!is_infos_empty)
        {
            // motion correction
            _mm_calib = std::make_shared< mm_calib_handler >( _hw_monitor, _owner->get_pid() );

            _accel_intrinsic = std::make_shared< rsutils::lazy< ds::imu_intrinsic > >(
                [this]() { return _mm_calib->get_intrinsic( RS2_STREAM_ACCEL ); } );
            _gyro_intrinsic = std::make_shared< rsutils::lazy< ds::imu_intrinsic > >(
                [this]() { return _mm_calib->get_intrinsic( RS2_STREAM_GYRO ); } );

            // use predefined extrinsics
            _depth_to_imu = std::make_shared< rsutils::lazy< rs2_extrinsics > >(
                [this]() { return _mm_calib->get_extrinsic( RS2_STREAM_ACCEL ); } );
        }

        // Make sure all MM streams are positioned with the same extrinsics
        environment::get_instance().get_extrinsics_graph().register_extrinsics(depth_stream, *_accel_stream, _depth_to_imu);
        environment::get_instance().get_extrinsics_graph().register_same_extrinsics(*_accel_stream, *_gyro_stream);
        register_streams_to_extrinsic_groups();
    }

    const std::vector<uint8_t>& ds_motion_common::get_fisheye_calibration_table() const
    {
        return *_fisheye_calibration_table_raw;
    }
} // namespace librealsense
