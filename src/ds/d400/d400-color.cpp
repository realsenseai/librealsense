// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016-24 RealSense, Inc. All Rights Reserved.

#include <cstddef>
#include "metadata.h"

#include <src/ds/ds-timestamp.h>
#include <src/ds/ds-thermal-monitor.h>
#include "proc/color-formats-converter.h"
#include "d400-color.h"
#include "d400-info.h"
#include <src/backend.h>
#include <src/platform/platform-utils.h>

#include <src/ds/features/auto-exposure-roi-feature.h>

#include <rsutils/string/from.h>
#include <rsutils/type/fourcc.h>
using rs_fourcc = rsutils::type::fourcc;

namespace librealsense
{
    std::map<rs_fourcc::value_type, rs2_format> d400_color_fourcc_to_rs2_format = {
         {rs_fourcc('Y','U','Y','2'), RS2_FORMAT_YUYV},
         {rs_fourcc('Y','U','Y','V'), RS2_FORMAT_YUYV},
         {rs_fourcc('U','Y','V','Y'), RS2_FORMAT_UYVY},
         {rs_fourcc('M','J','P','G'), RS2_FORMAT_MJPEG},
         {rs_fourcc('R','W','1','6'), RS2_FORMAT_RAW16},
         {rs_fourcc('B','Y','R','2'), RS2_FORMAT_RAW16}
    };
    std::map<rs_fourcc::value_type, rs2_stream> d400_color_fourcc_to_rs2_stream = {
        {rs_fourcc('Y','U','Y','2'), RS2_STREAM_COLOR},
        {rs_fourcc('Y','U','Y','V'), RS2_STREAM_COLOR},
        {rs_fourcc('U','Y','V','Y'), RS2_STREAM_COLOR},
        {rs_fourcc('R','W','1','6'), RS2_STREAM_COLOR},
        {rs_fourcc('B','Y','R','2'), RS2_STREAM_COLOR},
        {rs_fourcc('M','J','P','G'), RS2_STREAM_COLOR}
    };

    d400_color::d400_color( std::shared_ptr< const d400_info > const & dev_info )
        : d400_device(dev_info), device(dev_info),
          _color_stream(new stream(RS2_STREAM_COLOR)),
          _separate_color(true)
    {
        try
        {
            create_color_device( dev_info->get_context(), dev_info->get_group() );
            init();
        } 
        catch (const std::exception& e)
        {
            LOG_ERROR("Color Sensor Failure! " << e.what());
        }
    }

    void d400_color::create_color_device(std::shared_ptr<context> ctx, const platform::backend_device_group& group)
    {
        using namespace ds;

        _color_calib_table_raw = [this]()
        {
            return get_d400_raw_calibration_table(d400_calibration_table_id::rgb_calibration_id);
        };

        _color_extrinsic = std::make_shared< rsutils::lazy< rs2_extrinsics > >(
            [this]() { return from_pose( get_d400_color_stream_extrinsic( *_color_calib_table_raw ) ); } );
        environment::get_instance().get_extrinsics_graph().register_extrinsics(*_color_stream, *_depth_stream, _color_extrinsic);
        register_stream_to_extrinsic_group(*_color_stream, 0);

        std::vector<platform::uvc_device_info> color_devs_info;
        // end point 3 is used for color sensor
        // except for D405 and D401_GMSL, in which the color is part of the depth unit
        // and it will then been found in end point 0 (the depth's one)
        auto color_devs_info_mi3 = filter_by_mi(group.uvc_devices, 3);
        if (color_devs_info_mi3.size() == 1 || (_is_mipi_device && _pid != ds::RS401_GMSL_PID) )
        {
            // means color end point in part of a separate color sensor (e.g. D435)
            if (_is_mipi_device)
                color_devs_info = group.uvc_devices;
            else
                color_devs_info = color_devs_info_mi3;
            std::unique_ptr< frame_timestamp_reader > d400_timestamp_reader_backup( new ds_timestamp_reader() );
            frame_timestamp_reader* timestamp_reader_from_metadata;
            if (!_is_mipi_device)
                timestamp_reader_from_metadata = new ds_timestamp_reader_from_metadata(std::move(d400_timestamp_reader_backup));
            else
                timestamp_reader_from_metadata = new ds_timestamp_reader_from_metadata_mipi_color(std::move(d400_timestamp_reader_backup));
            
            std::unique_ptr<frame_timestamp_reader> ds_timestamp_reader_metadata(timestamp_reader_from_metadata);

            auto enable_global_time_option = std::shared_ptr<global_time_option>(new global_time_option());
            platform::uvc_device_info info;
            if (_is_mipi_device)
                info = color_devs_info[1];
            else
                info = color_devs_info.front();
            auto uvcd = get_backend()->create_uvc_device( info );
            //auto ftr = std::unique_ptr<frame_timestamp_reader>(new global_timestamp_reader(std::move(d400_timestamp_reader_metadata), _tf_keeper, enable_global_time_option));
            auto raw_color_ep = std::make_shared<uvc_sensor>("Raw RGB Camera",
                uvcd,
                std::unique_ptr<frame_timestamp_reader>(new global_timestamp_reader(std::move(ds_timestamp_reader_metadata), _tf_keeper, enable_global_time_option)),
                this);

            auto color_ep = std::make_shared<d400_color_sensor>(this,
                raw_color_ep,
                d400_color_fourcc_to_rs2_format,
                d400_color_fourcc_to_rs2_stream);

            color_ep->register_option(RS2_OPTION_GLOBAL_TIME_ENABLED, enable_global_time_option);

            color_ep->register_info(RS2_CAMERA_INFO_PHYSICAL_PORT, color_devs_info.front().device_path);

            _color_device_idx = add_sensor(color_ep);
        }
        else
        {
            auto color_devs_info_mi0 = filter_by_mi(group.uvc_devices, 0);
            // one uvc device is seen over Windows and 3 uvc devices are seen over linux
            if (color_devs_info_mi0.size() == 1 || color_devs_info_mi0.size() == 3)
            {
                // means color end point is part of the depth sensor (e.g. D405, D401_GMSL)
                color_devs_info = color_devs_info_mi0;
                _color_device_idx = _depth_device_idx;
                d400_device::_color_stream = _color_stream;
                _separate_color = false;
            }
            else
                throw invalid_value_exception( rsutils::string::from() << "RS4XX: RGB modules inconsistency - "
                                                                         << color_devs_info_mi0.size() << " found" );
        }
    }

    void d400_color::register_color_features()
    {
        firmware_version fw_ver = firmware_version( get_info( RS2_CAMERA_INFO_FIRMWARE_VERSION ) );

        if( fw_ver >= firmware_version( 5, 10, 9, 0 ) && _pid != ds::RS405_PID && _pid != ds::RS401_GMSL_PID ) //D405, D401_GMSL do not support an actual RGB sensor
            register_feature( std::make_shared< auto_exposure_roi_feature >( get_color_sensor(), _hw_monitor, true ) );
    }

    rs2_format d400_color::get_color_format() const
    {
        auto const format_conversion = get_format_conversion();
        rs2_format const color_format
            = (format_conversion == format_conversion::full) ? RS2_FORMAT_RGB8 : RS2_FORMAT_YUYV;
        return color_format;
    }

    void d400_color::init()
    {
        auto& color_ep = get_color_sensor();
        auto raw_color_ep = get_raw_color_sensor();
    
        _ds_color_common = std::make_shared<ds_color_common>(raw_color_ep, color_ep, _fw_version, _hw_monitor, this);

        register_color_features();
        register_options();

        if (!_is_mipi_device)
        {
            register_metadata(color_ep);
        }
        else
        {
            register_metadata_mipi(color_ep);
        }
        register_processing_blocks();

        auto_calibrated::add_color_write_observer( [this]() {  _color_calib_table_raw.reset(); } );
    }

    void d400_color::register_options()
    {
        auto& color_ep = get_color_sensor();

        if (!_is_mipi_device)
        {
            _ds_color_common->register_color_options();
            color_ep.register_pu(RS2_OPTION_BACKLIGHT_COMPENSATION);

            auto raw_color_ep = get_raw_color_sensor();
            color_ep.register_option(RS2_OPTION_POWER_LINE_FREQUENCY,
                std::make_shared<uvc_pu_option>(raw_color_ep, RS2_OPTION_POWER_LINE_FREQUENCY,
                    std::map<float, std::string>{ { 0.f, "Disabled"},
                    { 1.f, "50Hz" },
                    { 2.f, "60Hz" },
                    { 3.f, "Auto" }, }));
        }

        if (_separate_color)
        {
            // Currently disabled for certain sensors
            if (!_is_mipi_device)
            {
                color_ep.register_pu(RS2_OPTION_AUTO_EXPOSURE_PRIORITY);
            }

            _ds_color_common->register_standard_options();

            // Register for tracking of thermal compensation changes
            if (val_in_range(_pid, { ds::RS455_PID }))
            {
                if (_thermal_monitor)
                    _thermal_monitor->add_observer([&](float) {
                    _color_calib_table_raw.reset(); });
            }
        }

        // Currently disabled for certain sensors
        if (!_is_mipi_device)
        {
            color_ep.register_pu(RS2_OPTION_HUE);
        }
    }

    void d400_color::register_metadata(const synthetic_sensor& color_ep) const
    {
        color_ep.register_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP, make_uvc_header_parser(&platform::uvc_header::timestamp));

        auto md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_rgb_mode, rgb_mode) +
            offsetof(md_rgb_normal_mode, intel_capture_timing);

        color_ep.register_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP, make_rs400_sensor_ts_parser(make_uvc_header_parser(&platform::uvc_header::timestamp),
            make_attribute_parser(&md_capture_timing::sensor_timestamp, md_capture_timing_attributes::sensor_timestamp_attribute, md_prop_offset)));

        if (_separate_color)
        {
            auto md_prop_offset = metadata_raw_mode_offset +
                offsetof(md_rgb_mode, rgb_mode) +
                offsetof(md_rgb_normal_mode, intel_rgb_control);

            color_ep.register_metadata(RS2_FRAME_METADATA_AUTO_EXPOSURE, make_attribute_parser(&md_rgb_control::ae_mode, md_rgb_control_attributes::ae_mode_attribute, md_prop_offset,
                [](rs2_metadata_type param) { return (param != 1); })); // OFF value via UVC is 1 (ON is 8)
        }
        else
        {
            // attributes of md_rgb_control
            auto md_prop_offset = metadata_raw_mode_offset +
                offsetof(md_rgb_mode, rgb_mode) +
                offsetof(md_rgb_normal_mode, intel_rgb_control);

            color_ep.register_metadata(RS2_FRAME_METADATA_AUTO_EXPOSURE, make_attribute_parser(&md_rgb_control::ae_mode, md_rgb_control_attributes::ae_mode_attribute, md_prop_offset));
        }

        _ds_color_common->register_metadata();
    }

    void d400_color::register_processing_blocks()
    {
        // attributes of md_capture_stats
        auto& color_ep = get_color_sensor();
        // attributes of md_rgb_control
        auto raw_color_ep = get_raw_color_sensor();

        if (!_is_mipi_device)
        {
            color_ep.register_processing_block(processing_block_factory::create_pbf_vector<yuy2_converter>(RS2_FORMAT_YUYV, map_supported_color_formats(RS2_FORMAT_YUYV), RS2_STREAM_COLOR));
            color_ep.register_processing_block(processing_block_factory::create_id_pbf(RS2_FORMAT_RAW16, RS2_STREAM_COLOR));
        }
        else
        {
            auto uvc_dev = raw_color_ep->get_uvc_device();
            if (uvc_dev->is_platform_jetson())
            {
                // Work-around for discrepancy between the RGB YUYV descriptor and the parser. Use UYUV parser instead.
                // Bytes are reveiced swapped, so YUYV format is received as UYVY.
                color_ep.register_processing_block( processing_block_factory::create_pbf_vector< uyvy_converter >( RS2_FORMAT_YUYV,
                                                                                                                   map_supported_color_formats( RS2_FORMAT_YUYV, false ),
                                                                                                                   RS2_STREAM_COLOR ) );
                color_ep.register_processing_block( { { RS2_FORMAT_YUYV } },
                                                    { { RS2_FORMAT_YUYV, RS2_STREAM_COLOR } },
                                                    []() { return std::make_shared< uyvy_to_yuyv >(); } );
            }
            else
            {
                color_ep.register_processing_block(processing_block_factory::create_pbf_vector<yuy2_converter>(RS2_FORMAT_YUYV, map_supported_color_formats(RS2_FORMAT_YUYV), RS2_STREAM_COLOR));
            }
        }        
    }

    void d400_color::register_metadata_mipi(const synthetic_sensor &color_ep) const
    {
        // for mipi device
        color_ep.register_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP, make_uvc_header_parser(&platform::uvc_header::timestamp));

        // frame counter
        color_ep.register_metadata(RS2_FRAME_METADATA_FRAME_COUNTER, make_uvc_header_parser(&platform::uvc_header_mipi::frame_counter));

        // attributes of md_mipi_rgb_control structure
        color_ep.register_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::hw_timestamp, md_mipi_rgb_control_attributes::hw_timestamp_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_BRIGHTNESS,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::brightness, md_mipi_rgb_control_attributes::brightness_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_CONTRAST, 
            create_color_md_mipi_parser(&md_mipi_rgb_mode::contrast, md_mipi_rgb_control_attributes::contrast_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_SATURATION,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::saturation, md_mipi_rgb_control_attributes::saturation_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_SHARPNESS, 
            create_color_md_mipi_parser(&md_mipi_rgb_mode::sharpness, md_mipi_rgb_control_attributes::sharpness_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_AUTO_WHITE_BALANCE_TEMPERATURE,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::auto_white_balance_temp, md_mipi_rgb_control_attributes::auto_white_balance_temp_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_GAMMA,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::gamma, md_mipi_rgb_control_attributes::gamma_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::manual_exposure, md_mipi_rgb_control_attributes::manual_exposure_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_MANUAL_WHITE_BALANCE,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::manual_white_balance, md_mipi_rgb_control_attributes::manual_white_balance_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_AUTO_EXPOSURE,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::auto_exposure_mode, 
                md_mipi_rgb_control_attributes::auto_exposure_mode_attribute,
                [](rs2_metadata_type param) { return (param != 1); })); // OFF value via UVC is 1 (ON is 8)
        
        color_ep.register_metadata(RS2_FRAME_METADATA_GAIN_LEVEL,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::gain, md_mipi_rgb_control_attributes::gain_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_BACKLIGHT_COMPENSATION,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::backlight_compensation, md_mipi_rgb_control_attributes::backlight_compensation_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_HUE,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::hue, md_mipi_rgb_control_attributes::hue_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_POWER_LINE_FREQUENCY,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::power_line_frequency, md_mipi_rgb_control_attributes::power_line_frequency_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_LOW_LIGHT_COMPENSATION,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::low_light_compensation, md_mipi_rgb_control_attributes::low_light_compensation_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_INPUT_WIDTH,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::input_width, md_mipi_rgb_control_attributes::input_width_attribute));
        
        color_ep.register_metadata(RS2_FRAME_METADATA_INPUT_HEIGHT,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::input_height, md_mipi_rgb_control_attributes::input_height_attribute));
       
        color_ep.register_metadata(RS2_FRAME_METADATA_CRC,
            create_color_md_mipi_parser(&md_mipi_rgb_mode::crc, md_mipi_rgb_control_attributes::crc_attribute));
    }
    
    void d400_color::register_stream_to_extrinsic_group(const stream_interface& stream, uint32_t group_index)
    {
        device::register_stream_to_extrinsic_group(stream, group_index);
    }

    rs2_intrinsics d400_color_sensor::get_intrinsics(const stream_profile& profile) const
    {
        return get_d400_intrinsic_by_resolution(
            *_owner->_color_calib_table_raw,
            ds::d400_calibration_table_id::rgb_calibration_id,
            profile.width, profile.height);
    }

    stream_profiles d400_color_sensor::init_stream_profiles()
    {
        auto lock = environment::get_instance().get_extrinsics_graph().lock();
        auto&& results = synthetic_sensor::init_stream_profiles();

        for (auto&& p : results)
        {
            // Register stream types
            if (p->get_stream_type() == RS2_STREAM_COLOR)
            {
                assign_stream(_owner->_color_stream, p);
            }

            auto&& video = dynamic_cast<video_stream_profile_interface*>(p.get());
            const auto&& profile = to_profile(p.get());

            std::weak_ptr<d400_color_sensor> wp =
                std::dynamic_pointer_cast<d400_color_sensor>(this->shared_from_this());
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

    processing_blocks d400_color_sensor::get_recommended_processing_blocks() const
    {
        return get_color_recommended_proccesing_blocks();
    }
}
