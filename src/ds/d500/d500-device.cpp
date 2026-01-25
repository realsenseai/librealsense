// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022-4 RealSense, Inc. All Rights Reserved.

#include "metadata-parser.h"
#include "metadata.h"
#include <src/backend.h>
#include <src/platform/platform-utils.h>

#include "d500-device.h"
#include "d500-private.h"
#include "d500-options.h"
#include "d500-info.h"
#include <src/ds/ds-options.h>
#include <src/ds/ds-timestamp.h>
#include <src/ds/ds-thermal-monitor.h>
#include "stream.h"
#include "environment.h"

#include <src/ds/features/amplitude-factor-feature.h>
#include <src/ds/features/auto-exposure-roi-feature.h>

#include "proc/depth-formats-converter.h"
#include "proc/y8i-to-y8y8.h"
#include "proc/y16i-10msb-to-y16y16.h"

#include <rsutils/type/fourcc.h>
using rs_fourcc = rsutils::type::fourcc;

#include <rsutils/string/hexdump.h>
#include <rsutils/version.h>

#include <vector>
#include <string>

#include <src/ds/d500/d500-debug-protocol-calibration-engine.h>

#ifdef HWM_OVER_XU
constexpr bool hw_mon_over_xu = true;
#else
constexpr bool hw_mon_over_xu = false;
#endif

namespace librealsense
{
    std::map<uint32_t, rs2_format> d500_depth_fourcc_to_rs2_format = {
        {rs_fourcc('Y','U','Y','2'), RS2_FORMAT_YUYV},
        {rs_fourcc('Y','U','Y','V'), RS2_FORMAT_YUYV},
        {rs_fourcc('U','Y','V','Y'), RS2_FORMAT_UYVY},
        {rs_fourcc('G','R','E','Y'), RS2_FORMAT_Y8},
        {rs_fourcc('Y','8','I',' '), RS2_FORMAT_Y8I},
        {rs_fourcc('W','1','0',' '), RS2_FORMAT_W10},
        {rs_fourcc('Y','1','6',' '), RS2_FORMAT_Y16},
        {rs_fourcc('Y','1','2','I'), RS2_FORMAT_Y12I},
        {rs_fourcc('Y','1','6','I'), RS2_FORMAT_Y16I},
        {rs_fourcc('Z','1','6',' '), RS2_FORMAT_Z16},
        {rs_fourcc('R','G','B','2'), RS2_FORMAT_BGR8},
        {rs_fourcc('M','J','P','G'), RS2_FORMAT_MJPEG},
        {rs_fourcc('B','Y','R','2'), RS2_FORMAT_RAW16}

    };
    std::map<uint32_t, rs2_stream> d500_depth_fourcc_to_rs2_stream = {
        {rs_fourcc('Y','U','Y','2'), RS2_STREAM_COLOR},
        {rs_fourcc('Y','U','Y','V'), RS2_STREAM_COLOR},
        {rs_fourcc('U','Y','V','Y'), RS2_STREAM_INFRARED},
        {rs_fourcc('G','R','E','Y'), RS2_STREAM_INFRARED},
        {rs_fourcc('Y','8','I',' '), RS2_STREAM_INFRARED},
        {rs_fourcc('W','1','0',' '), RS2_STREAM_INFRARED},
        {rs_fourcc('Y','1','6',' '), RS2_STREAM_INFRARED},
        {rs_fourcc('Y','1','2','I'), RS2_STREAM_INFRARED},
        {rs_fourcc('Y','1','6','I'), RS2_STREAM_INFRARED},
        {rs_fourcc('R','G','B','2'), RS2_STREAM_INFRARED},
        {rs_fourcc('Z','1','6',' '), RS2_STREAM_DEPTH},
        {rs_fourcc('Z','1','6','H'), RS2_STREAM_DEPTH},
        {rs_fourcc('B','Y','R','2'), RS2_STREAM_COLOR},
        {rs_fourcc('M','J','P','G'), RS2_STREAM_COLOR}
    };

    std::vector<uint8_t> d500_device::send_receive_raw_data(const std::vector<uint8_t>& input)
    {
        return _hw_monitor->send(input);
    }
    
    std::vector<uint8_t> d500_device::build_command(uint32_t opcode,
        uint32_t param1,
        uint32_t param2,
        uint32_t param3,
        uint32_t param4,
        uint8_t const * data,
        size_t dataLength) const
    {
        return _hw_monitor->build_command(opcode, param1, param2, param3, param4, data, dataLength);
    }

    void d500_device::hardware_reset()
    {
        command cmd(ds::HWRST);
        cmd.require_response = false;
        _hw_monitor->send(cmd);
    }

    void d500_device::enter_update_state() const
    {
        // preparing HWM command
        command cmd(ds::DFU);
        cmd.param1 = (_pid == ds::D585S_PID || _pid == ds::D585_PID) ? 0 : 1;
        cmd.require_response = false;

        _ds_device_common->enter_update_state(cmd);
    }

    std::vector<uint8_t> d500_device::backup_flash( rs2_update_progress_callback_sptr callback )
    {
        // No flash backup process for D500 device
        return std::vector< uint8_t >{};
    }

    void d500_device::update_flash(const std::vector<uint8_t>& image, rs2_update_progress_callback_sptr callback, int update_mode)
    {
        throw not_implemented_exception("D500 device does not support unsigned FW update");
    }

    std::string d500_device::get_opcode_string(int opcode) const 
    {
        return _hw_monitor_response->hwmon_error2str(opcode);
    }

    d500_depth_sensor::d500_depth_sensor( d500_device * owner,std::shared_ptr<uvc_sensor> uvc_sensor)
        : synthetic_sensor(ds::DEPTH_STEREO, uvc_sensor, owner, d500_depth_fourcc_to_rs2_format, d500_depth_fourcc_to_rs2_stream)
        , _owner(owner)
        , _depth_units(-1)
    {
    }

    processing_blocks d500_depth_sensor::get_recommended_processing_blocks() const
    {
        return get_ds_depth_recommended_proccesing_blocks();
    }

    rs2_intrinsics d500_depth_sensor::get_intrinsics( const stream_profile & profile ) const
    {
        return get_d500_intrinsic_by_resolution(
            *_owner->_coefficients_table_raw,
            ds::d500_calibration_table_id::depth_calibration_id,
            profile.width, profile.height, _owner->_is_symmetrization_enabled);
    }

    void d500_depth_sensor::set_frame_metadata_modifier( on_frame_md callback )
    {
        _metadata_modifier = callback;
        auto s = get_raw_sensor().get();
        auto uvc = As< librealsense::uvc_sensor >(s);
        if(uvc)
            uvc->set_frame_metadata_modifier(callback);
    }

    void d500_depth_sensor::open( const stream_profiles & requests )
    {
        group_multiple_fw_calls(*this, [&]() {
            _depth_units = get_option(RS2_OPTION_DEPTH_UNITS).query();
            set_frame_metadata_modifier([&](frame_additional_data& data) {data.depth_units = _depth_units.load(); });

            synthetic_sensor::open(requests);

            if( _owner && _owner->_thermal_monitor )
                _owner->_thermal_monitor->update( true );
        }); //group_multiple_fw_calls
    }

    void d500_depth_sensor::close()
    {
        if( _owner && _owner->_thermal_monitor )
            _owner->_thermal_monitor->update( false );

        synthetic_sensor::close();
    }

    rs2_intrinsics d500_depth_sensor::get_color_intrinsics( const stream_profile & profile ) const
    {
        return get_d500_intrinsic_by_resolution(
            *_owner->_color_calib_table_raw,
            ds::d500_calibration_table_id::rgb_calibration_id,
            profile.width, profile.height);
    }

    /*
    Infrared profiles are initialized with the following logic:
    - If device has color sensor (D415 / D435), infrared profile is chosen with Y8 format
    - If device does not have color sensor:
        * if it is a rolling shutter device (D400 / D410 / D415 / D405), infrared profile is chosen with RGB8 format
        * for other devices (D420 / D430), infrared profile is chosen with Y8 format
    */
    stream_profiles d500_depth_sensor::init_stream_profiles()
    {
        auto lock = environment::get_instance().get_extrinsics_graph().lock();

        auto&& results = synthetic_sensor::init_stream_profiles();

        for (auto&& p : results)
        {
            // Register stream types
            if (p->get_stream_type() == RS2_STREAM_DEPTH)
            {
                assign_stream(_owner->_depth_stream, p);
            }
            else if (p->get_stream_type() == RS2_STREAM_INFRARED && p->get_stream_index() < 2)
            {
                assign_stream(_owner->_left_ir_stream, p);
            }
            else if (p->get_stream_type() == RS2_STREAM_INFRARED  && p->get_stream_index() == 2)
            {
                assign_stream(_owner->_right_ir_stream, p);
            }
            else if (p->get_stream_type() == RS2_STREAM_COLOR)
            {
                assign_stream(_owner->_color_stream, p);
            }
            auto&& vid_profile = dynamic_cast<video_stream_profile_interface*>(p.get());

            // used when color stream comes from depth sensor (as in D405)
            if (p->get_stream_type() == RS2_STREAM_COLOR)
            {
                const auto&& profile = to_profile(p.get());
                std::weak_ptr<d500_depth_sensor> wp =
                    std::dynamic_pointer_cast<d500_depth_sensor>(this->shared_from_this());
                vid_profile->set_intrinsics([profile, wp]()
                    {
                        auto sp = wp.lock();
                        if (sp)
                            return sp->get_color_intrinsics(profile);
                        else
                            return rs2_intrinsics{};
                    });
            }
            // Register intrinsics
            else if (p->get_format() != RS2_FORMAT_Y16) // Y16 format indicate unrectified images, no intrinsics are available for these
            {
                const auto&& profile = to_profile(p.get());
                std::weak_ptr<d500_depth_sensor> wp =
                    std::dynamic_pointer_cast<d500_depth_sensor>(this->shared_from_this());
                vid_profile->set_intrinsics([profile, wp]()
                {
                    auto sp = wp.lock();
                    if (sp)
                        return sp->get_intrinsics(profile);
                    else
                        return rs2_intrinsics{};
                });
            }
        }

        return results;
    }

    float d500_depth_sensor::get_depth_scale() const
    {
        if (_depth_units < 0)
            _depth_units = get_option(RS2_OPTION_DEPTH_UNITS).query();
        return _depth_units;
    }

    void d500_depth_sensor::set_depth_scale( float val )
    {
        _depth_units = val;
        set_frame_metadata_modifier([&](frame_additional_data& data) {data.depth_units = _depth_units.load(); });
    }

    float d500_depth_sensor::get_stereo_baseline_mm() const
    {
        return _owner->get_stereo_baseline_mm();
    }

    float d500_depth_sensor::get_preset_max_value() const
    {
        return static_cast<float>(RS2_RS400_VISUAL_PRESET_MEDIUM_DENSITY);
    }

    bool d500_device::is_camera_in_advanced_mode() const
    {
        return _ds_device_common->is_camera_in_advanced_mode();
    }

    float d500_device::get_stereo_baseline_mm() const // to be d500 adapted
    {
        using namespace ds;
        float baseline = 100.0f; // so we will have a non zero value if cannot read from table
        try
        {
            auto table = check_calib<d500_coefficients_table>(*_coefficients_table_raw);
            baseline = fabs(table->baseline);
        }
        catch( const std::exception &e )
        {
            LOG_ERROR("Failed reading stereo baseline, using default value --> " << e.what() );
        }

        return baseline;
    }

    std::vector<uint8_t> d500_device::get_d500_raw_calibration_table(ds::d500_calibration_table_id table_id) const // to be d500 adapted
    {
        using namespace ds;
        command cmd(GET_HKR_CONFIG_TABLE, 
            static_cast<int>(d500_calib_location::d500_calib_flash_memory),
            static_cast<int>(table_id),
            static_cast<int>(d500_calib_type::d500_calib_dynamic));
        return _hw_monitor->send(cmd);
    }

    std::vector<uint8_t> d500_device::get_new_calibration_table() const // to be d500 adapted
    {
        command cmd(ds::RECPARAMSGET);
        return _hw_monitor->send(cmd);
    }

    ds::ds_caps d500_device::parse_device_capabilities( const std::vector<uint8_t> &gvd_buf ) const 
    {
        using namespace ds;

        ds_caps val{ds_caps::CAP_UNDEFINED};
        if( gvd_buf[d500_gvd_offsets::active_projector] )
            val |= ds_caps::CAP_ACTIVE_PROJECTOR;
        if( gvd_buf[d500_gvd_offsets::rgb_sensor] )
            val |= ds_caps::CAP_RGB_SENSOR;
        if( gvd_buf[d500_gvd_offsets::imu_sensor] )
            val |= ds_caps::CAP_IMU_SENSOR;
            
        // assuming always true for d500 devices
        val |= ds_caps::CAP_GLOBAL_SHUTTER;
        val |= ds_caps::CAP_INTERCAM_HW_SYNC;

        return val;
    }

    std::shared_ptr<synthetic_sensor> d500_device::create_depth_device(std::shared_ptr<context> ctx,
        const std::vector<platform::uvc_device_info>& all_device_infos)
    {
        using namespace ds;

        std::vector<std::shared_ptr<platform::uvc_device>> depth_devices;
        auto depth_devs_info = filter_by_mi( all_device_infos, 0 );

        for (auto&& info : depth_devs_info) // Filter just mi=0, DEPTH
        {
            auto depth_uvc_device = get_backend()->create_uvc_device(info);
            if (depth_uvc_device)
                depth_devices.push_back(depth_uvc_device);
        }

        if (depth_devs_info.empty() || depth_devices.empty())
        {
            throw backend_exception("cannot access depth sensor", RS2_EXCEPTION_TYPE_BACKEND);
        }

        std::unique_ptr< frame_timestamp_reader > timestamp_reader_backup( new ds_timestamp_reader() );
        std::unique_ptr<frame_timestamp_reader> timestamp_reader_metadata(new ds_timestamp_reader_from_metadata(std::move(timestamp_reader_backup)));
        auto enable_global_time_option = std::shared_ptr<global_time_option>(new global_time_option());
        auto raw_depth_ep = std::make_shared<uvc_sensor>("Raw Depth Sensor", std::make_shared<platform::multi_pins_uvc_device>(depth_devices),
            std::unique_ptr<frame_timestamp_reader>(new global_timestamp_reader(std::move(timestamp_reader_metadata), _tf_keeper, enable_global_time_option)), this);

        raw_depth_ep->register_xu(depth_xu); // make sure the XU is initialized every time we power the camera

        auto depth_ep = std::make_shared<d500_depth_sensor>(this, raw_depth_ep);

        depth_ep->register_info(RS2_CAMERA_INFO_PHYSICAL_PORT, filter_by_mi(all_device_infos, 0).front().device_path);

        depth_ep->register_option(RS2_OPTION_GLOBAL_TIME_ENABLED, enable_global_time_option);

        depth_ep->register_processing_block(processing_block_factory::create_id_pbf(RS2_FORMAT_Y8, RS2_STREAM_INFRARED, 1));
        depth_ep->register_processing_block(processing_block_factory::create_id_pbf(RS2_FORMAT_Z16, RS2_STREAM_DEPTH));

        depth_ep->register_processing_block({ {RS2_FORMAT_W10} }, { {RS2_FORMAT_RAW10, RS2_STREAM_INFRARED, 1} }, []() { return std::make_shared<w10_converter>(RS2_FORMAT_RAW10); });
        depth_ep->register_processing_block({ {RS2_FORMAT_W10} }, { {RS2_FORMAT_Y10BPACK, RS2_STREAM_INFRARED, 1} }, []() { return std::make_shared<w10_converter>(RS2_FORMAT_Y10BPACK); });

        return depth_ep;
    }

    d500_device::d500_device( std::shared_ptr< const d500_info > const & dev_info )
        : backend_device(dev_info), global_time_interface(),
          d500_auto_calibrated(std::make_shared<d500_debug_protocol_calibration_engine>(this), this),
          _device_capabilities(ds::ds_caps::CAP_UNDEFINED),
          _depth_stream(new stream(RS2_STREAM_DEPTH)),
          _left_ir_stream(new stream(RS2_STREAM_INFRARED, 1)),
          _right_ir_stream(new stream(RS2_STREAM_INFRARED, 2)),
          _color_stream(nullptr),
          _hw_monitor_response(std::make_shared<ds::d500_hwmon_response>())
    {
        _depth_device_idx
            = add_sensor( create_depth_device( dev_info->get_context(), dev_info->get_group().uvc_devices ) );
        init( dev_info->get_context(), dev_info->get_group() );
    }

    void d500_device::init(std::shared_ptr<context> ctx,
        const platform::backend_device_group& group)
    {
        using namespace ds;

        auto raw_sensor = get_raw_depth_sensor();
        _pid = group.uvc_devices.front().pid;

        _color_calib_table_raw = [this]()
        {
            return get_d500_raw_calibration_table(d500_calibration_table_id::rgb_calibration_id);
        };

        if (hw_mon_over_xu || (!group.usb_devices.size()))
        {
            _hw_monitor = std::make_shared<hw_monitor_extended_buffers>(
                std::make_shared<locked_transfer>(
                    std::make_shared<command_transfer_over_xu>( *raw_sensor, depth_xu, DS5_HWMONITOR ),
                    raw_sensor), _hw_monitor_response);
        }
        else
        {
            _hw_monitor = std::make_shared< hw_monitor_extended_buffers >(
                std::make_shared< locked_transfer >( get_backend()->create_usb_device( group.usb_devices.front() ),
                                                     raw_sensor ), _hw_monitor_response);
        }

        _ds_device_common = std::make_shared<ds_device_common>(this, _hw_monitor);

        // Define Left-to-Right extrinsics calculation (lazy)
        // Reference CS - Right-handed; positive [X,Y,Z] point to [Left,Up,Forward] accordingly.
        _left_right_extrinsics = std::make_shared< rsutils::lazy< rs2_extrinsics > >(
            [this]()
            {
                rs2_extrinsics ext = identity_matrix();
                auto table = check_calib<d500_coefficients_table>(*_coefficients_table_raw);
                ext.translation[0] = -0.001f * table->baseline; // mm to meters
                return ext;
            });

        environment::get_instance().get_extrinsics_graph().register_same_extrinsics(*_depth_stream, *_left_ir_stream);
        environment::get_instance().get_extrinsics_graph().register_extrinsics(*_depth_stream, *_right_ir_stream, _left_right_extrinsics);

        register_stream_to_extrinsic_group(*_depth_stream, 0);
        register_stream_to_extrinsic_group(*_left_ir_stream, 0);
        register_stream_to_extrinsic_group(*_right_ir_stream, 0);

        _coefficients_table_raw = [this]() { return get_d500_raw_calibration_table(d500_calibration_table_id::depth_calibration_id); };
        _new_calib_table_raw = [this]() { return get_new_calibration_table(); };

        std::string device_name = (rs500_sku_names.end() != rs500_sku_names.find(_pid)) ? rs500_sku_names.at(_pid) : "RS5xx";

        std::vector<uint8_t> gvd_buff(HW_MONITOR_BUFFER_SIZE);

        auto& depth_sensor = get_depth_sensor();
        auto raw_depth_sensor = get_raw_depth_sensor();

        d500_auto_calibrated::set_depth_sensor( &depth_sensor );

        using namespace platform;

        std::string pid_hex_str, usb_type_str;
        d500_gvd_parsed_fields gvd_parsed_fields;
        bool advanced_mode = false;
        bool usb_modality = true;
        group_multiple_fw_calls(depth_sensor, [&]() {
            
            // D500 device can get enumerated before the whole HW in the camera is ready.
            // Since GVD gather all information from all the HW, it might need some more time to finish all hand shakes.
            // on this case it will return HW_NOT_READY error code.
            // Note: D500 error codes list is different than D400.

            const std::set< int32_t > gvd_retry_errors{ _hw_monitor_response->HW_NOT_READY };

            _hw_monitor->get_gvd( gvd_buff.size(),
                                  gvd_buff.data(),
                                  ds::fw_cmd::GVD,
                                  &gvd_retry_errors );

            get_gvd_details(gvd_buff, &gvd_parsed_fields);
            
            _device_capabilities = parse_device_capabilities( gvd_buff );

            _fw_version = rsutils::version(gvd_parsed_fields.fw_version);

            auto _usb_mode = usb3_type;
            usb_type_str = usb_spec_names.at(_usb_mode);
            _usb_mode = raw_depth_sensor->get_usb_specification();
            if (usb_spec_names.count(_usb_mode) && (usb_undefined != _usb_mode))
                usb_type_str = usb_spec_names.at(_usb_mode);
            else  // Backend fails to provide USB descriptor  - occurs with RS3 build. Requires further work
                usb_modality = false;

            set_imu_type( gvd_buff, &gvd_parsed_fields );

            _is_symmetrization_enabled = check_symmetrization_enabled();

            depth_sensor.register_processing_block(
                { {RS2_FORMAT_Y8I} },
                { {RS2_FORMAT_Y8, RS2_STREAM_INFRARED, 1} , {RS2_FORMAT_Y8, RS2_STREAM_INFRARED, 2} },
                []() { return std::make_shared<y8i_to_y8y8>(); }
            ); // L+R

            depth_sensor.register_processing_block(
                { RS2_FORMAT_Y16I },
                { {RS2_FORMAT_Y16, RS2_STREAM_INFRARED, 1}, {RS2_FORMAT_Y16, RS2_STREAM_INFRARED, 2} },
                []() {return std::make_shared<y16i_10msb_to_y16y16>(); }
            );
                
            pid_hex_str = rsutils::string::from() << std::uppercase << rsutils::string::hexdump( _pid );

            _is_locked = _ds_device_common->is_locked( gvd_buff.data(), d500_gvd_offsets::is_camera_locked_offset );


            //EXPOSURE AND GAIN - preparing uvc options
            auto exposure_option = std::make_shared<uvc_xu_option<uint32_t>>(raw_depth_sensor,
                depth_xu,
                DS5_EXPOSURE,
                "Depth Exposure (usec)");
            auto gain_option = std::make_shared<uvc_pu_option>(raw_depth_sensor, RS2_OPTION_GAIN);

            //AUTO EXPOSURE
            auto enable_auto_exposure = std::make_shared<uvc_xu_option<uint8_t>>(raw_depth_sensor,
                depth_xu,
                DS5_ENABLE_AUTO_EXPOSURE,
                "Enable Auto Exposure");
            depth_sensor.register_option(RS2_OPTION_ENABLE_AUTO_EXPOSURE, enable_auto_exposure);

            //EXPOSURE
            depth_sensor.register_option(RS2_OPTION_EXPOSURE,
                std::make_shared<auto_disabling_control>(
                    exposure_option,
                    enable_auto_exposure));

            //GAIN
            depth_sensor.register_option(RS2_OPTION_GAIN,
                std::make_shared<auto_disabling_control>(
                    gain_option,
                    enable_auto_exposure));

            if ((_device_capabilities & ds_caps::CAP_INTERCAM_HW_SYNC) == ds_caps::CAP_INTERCAM_HW_SYNC)
            {
                std::map< float, std::string > description_per_value = { { 0.f, "No Sync" },
                                                                         { 1.f, "RGB master" },
                                                                         { 2.f, "PWM master" },
                                                                         { 3.f, "External master" } };
                depth_sensor.register_option( RS2_OPTION_INTER_CAM_SYNC_MODE,
                                              std::make_shared< d500_external_sync_mode >( *_hw_monitor,
                                                                                           raw_depth_sensor,
                                                                                           description_per_value ) );
            }

            depth_sensor.register_option(RS2_OPTION_STEREO_BASELINE, std::make_shared<const_value_option>("Distance in mm between the stereo imagers",
                    rsutils::lazy< float >( [this]() { return get_stereo_baseline_mm(); } ) ) );

            if (advanced_mode)
            {
                auto depth_scale = std::make_shared<depth_scale_option>(*_hw_monitor);
                auto depth_sensor = As<d500_depth_sensor, synthetic_sensor>(&get_depth_sensor());
                assert(depth_sensor);

                depth_scale->add_observer([depth_sensor](float val)
                {
                    depth_sensor->set_depth_scale(val);
                });

                depth_sensor->register_option(RS2_OPTION_DEPTH_UNITS, depth_scale);
            }
            else
            {
                float default_depth_units = 0.001f; //meters
                depth_sensor.register_option(RS2_OPTION_DEPTH_UNITS, std::make_shared<const_value_option>("Number of meters represented by a single depth unit",
                        rsutils::lazy< float >( [default_depth_units]() { return default_depth_units; } ) ) );
            }

            // defining the temperature options
            auto pvt_temperature = std::make_shared< temperature_xu_option >(raw_depth_sensor,
                depth_xu,
                DS5_HKR_PVT_TEMPERATURE,
                "PVT Temperature");

            auto ohm_temperature = std::make_shared< temperature_xu_option >(raw_depth_sensor,
                depth_xu,
                DS5_HKR_OHM_TEMPERATURE,
                "OHM Temperature");

            // registering the temperature options
            depth_sensor.register_option(RS2_OPTION_SOC_PVT_TEMPERATURE, pvt_temperature);
            depth_sensor.register_option(RS2_OPTION_OHM_TEMPERATURE, ohm_temperature);

            if (_pid == D585S_PID)
            {
                auto proj_temperature = std::make_shared< temperature_xu_option >(raw_depth_sensor,
                    depth_xu,
                    DS5_HKR_PROJECTOR_TEMPERATURE,
                    "Projector Temperature");
                depth_sensor.register_option(RS2_OPTION_PROJECTOR_TEMPERATURE, proj_temperature);
            }

            auto error_control = std::make_shared< uvc_xu_option< uint8_t > >( raw_depth_sensor,
                                                                               depth_xu,
                                                                               DS5_ERROR_REPORTING,
                                                                               "Error reporting" );

            _polling_error_handler = std::make_shared< polling_error_handler >(
                1000,
                error_control,
                raw_depth_sensor->get_notifications_processor(),
                std::make_shared< ds_notification_decoder >( d500_fw_error_report ) );

            depth_sensor.register_option( RS2_OPTION_ERROR_POLLING_ENABLED,
                                          std::make_shared< polling_errors_disable >( _polling_error_handler ) );

        }); //group_multiple_fw_calls

        // attributes of md_capture_timing
        auto md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_depth_mode, depth_y_mode) +
            offsetof(md_depth_y_normal_mode, intel_capture_timing);
        
        // attributes of md_capture_stats
       auto md_prop_offset_stats = metadata_raw_mode_offset +
            offsetof(md_depth_mode, depth_y_mode) +
            offsetof(md_depth_y_normal_mode, intel_capture_stats);

        depth_sensor.register_metadata(RS2_FRAME_METADATA_FRAME_COUNTER, make_attribute_parser(&md_capture_timing::frame_counter, md_capture_timing_attributes::frame_counter_attribute, md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP, 
            make_rs400_sensor_ts_parser(make_attribute_parser(&md_capture_stats::hw_timestamp, md_capture_stat_attributes::hw_timestamp_attribute, md_prop_offset_stats),
                make_attribute_parser(&md_capture_timing::sensor_timestamp, md_capture_timing_attributes::sensor_timestamp_attribute, md_prop_offset)));

        depth_sensor.register_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP, make_attribute_parser(&md_capture_stats::hw_timestamp, md_capture_stat_attributes::hw_timestamp_attribute, md_prop_offset_stats));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_WHITE_BALANCE, make_attribute_parser(&md_capture_stats::white_balance, md_capture_stat_attributes::white_balance_attribute, md_prop_offset_stats));

        // attributes of md_depth_control
        md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_depth_mode, depth_y_mode) +
            offsetof(md_depth_y_normal_mode, intel_depth_control);

        depth_sensor.register_metadata(RS2_FRAME_METADATA_GAIN_LEVEL, make_attribute_parser(&md_depth_control::manual_gain, md_depth_control_attributes::gain_attribute, md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE, make_attribute_parser(&md_depth_control::manual_exposure, md_depth_control_attributes::exposure_attribute, md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_AUTO_EXPOSURE, make_attribute_parser(&md_depth_control::auto_exposure_mode, md_depth_control_attributes::ae_mode_attribute, md_prop_offset));

        depth_sensor.register_metadata(RS2_FRAME_METADATA_FRAME_LASER_POWER, make_attribute_parser(&md_depth_control::laser_power, md_depth_control_attributes::laser_pwr_attribute, md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_FRAME_LASER_POWER_MODE, make_attribute_parser(&md_depth_control::emitterMode, md_depth_control_attributes::emitter_mode_attribute, md_prop_offset,
            [](const rs2_metadata_type& param) { return param == 1 ? 1 : 0; })); // starting at version 2.30.1 this control is superceeded by RS2_FRAME_METADATA_FRAME_EMITTER_MODE
        depth_sensor.register_metadata(RS2_FRAME_METADATA_EXPOSURE_PRIORITY, make_attribute_parser(&md_depth_control::exposure_priority, md_depth_control_attributes::exposure_priority_attribute, md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_EXPOSURE_ROI_LEFT, make_attribute_parser(&md_depth_control::exposure_roi_left, md_depth_control_attributes::roi_attribute, md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_EXPOSURE_ROI_RIGHT, make_attribute_parser(&md_depth_control::exposure_roi_right, md_depth_control_attributes::roi_attribute, md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_EXPOSURE_ROI_TOP, make_attribute_parser(&md_depth_control::exposure_roi_top, md_depth_control_attributes::roi_attribute, md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_EXPOSURE_ROI_BOTTOM, make_attribute_parser(&md_depth_control::exposure_roi_bottom, md_depth_control_attributes::roi_attribute, md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_FRAME_EMITTER_MODE, make_attribute_parser(&md_depth_control::emitterMode, md_depth_control_attributes::emitter_mode_attribute, md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_FRAME_LED_POWER, make_attribute_parser(&md_depth_control::ledPower, md_depth_control_attributes::led_power_attribute, md_prop_offset));

        // md_configuration - will be used for internal validation only
        md_prop_offset = metadata_raw_mode_offset + offsetof(md_depth_mode, depth_y_mode) + offsetof(md_depth_y_normal_mode, intel_configuration);

        depth_sensor.register_metadata((rs2_frame_metadata_value)RS2_FRAME_METADATA_HW_TYPE, make_attribute_parser(&md_configuration::hw_type, md_configuration_attributes::hw_type_attribute, md_prop_offset));
        depth_sensor.register_metadata((rs2_frame_metadata_value)RS2_FRAME_METADATA_SKU_ID, make_attribute_parser(&md_configuration::sku_id, md_configuration_attributes::sku_id_attribute, md_prop_offset));
        depth_sensor.register_metadata((rs2_frame_metadata_value)RS2_FRAME_METADATA_FORMAT, make_attribute_parser(&md_configuration::format, md_configuration_attributes::format_attribute, md_prop_offset));
        depth_sensor.register_metadata((rs2_frame_metadata_value)RS2_FRAME_METADATA_WIDTH, make_attribute_parser(&md_configuration::width, md_configuration_attributes::width_attribute, md_prop_offset));
        depth_sensor.register_metadata((rs2_frame_metadata_value)RS2_FRAME_METADATA_HEIGHT, make_attribute_parser(&md_configuration::height, md_configuration_attributes::height_attribute, md_prop_offset));
        depth_sensor.register_metadata((rs2_frame_metadata_value)RS2_FRAME_METADATA_ACTUAL_FPS,  std::make_shared<ds_md_attribute_actual_fps> ());

        depth_sensor.register_metadata(RS2_FRAME_METADATA_GPIO_INPUT_DATA, make_attribute_parser(&md_configuration::gpioInputData, md_configuration_attributes::gpio_input_data_attribute, md_prop_offset));

        // attributes of md_capture_timing
        md_prop_offset = metadata_raw_mode_offset + offsetof(md_depth_mode, depth_y_mode) + offsetof(md_depth_y_normal_mode, intel_configuration);

        depth_sensor.register_metadata(RS2_FRAME_METADATA_SEQUENCE_SIZE,
            make_attribute_parser(&md_configuration::sub_preset_info,
                md_configuration_attributes::sub_preset_info_attribute, md_prop_offset,
                [](const rs2_metadata_type& param) {
                    // bit mask and offset used to get data from bitfield
                    return (param & md_configuration::SUB_PRESET_BIT_MASK_SEQUENCE_SIZE)
                        >> md_configuration::SUB_PRESET_BIT_OFFSET_SEQUENCE_SIZE;
                }));

        depth_sensor.register_metadata(RS2_FRAME_METADATA_SEQUENCE_ID,
            make_attribute_parser(&md_configuration::sub_preset_info,
                md_configuration_attributes::sub_preset_info_attribute, md_prop_offset,
                [](const rs2_metadata_type& param) {
                    // bit mask and offset used to get data from bitfield
                    return (param & md_configuration::SUB_PRESET_BIT_MASK_SEQUENCE_ID)
                        >> md_configuration::SUB_PRESET_BIT_OFFSET_SEQUENCE_ID;
                }));

        depth_sensor.register_metadata(RS2_FRAME_METADATA_SEQUENCE_NAME,
            make_attribute_parser(&md_configuration::sub_preset_info,
                md_configuration_attributes::sub_preset_info_attribute, md_prop_offset,
                [](const rs2_metadata_type& param) {
                    // bit mask and offset used to get data from bitfield
                    return (param & md_configuration::SUB_PRESET_BIT_MASK_ID)
                        >> md_configuration::SUB_PRESET_BIT_OFFSET_ID;
                }));


        register_info(RS2_CAMERA_INFO_NAME, device_name);
        register_info(RS2_CAMERA_INFO_SERIAL_NUMBER, gvd_parsed_fields.optical_module_sn);
        register_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID, gvd_parsed_fields.optical_module_sn);
        register_info(RS2_CAMERA_INFO_FIRMWARE_VERSION, gvd_parsed_fields.fw_version);        
        register_info(RS2_CAMERA_INFO_PHYSICAL_PORT, group.uvc_devices.front().device_path);
        register_info(RS2_CAMERA_INFO_DEBUG_OP_CODE, std::to_string(static_cast<int>(fw_cmd::GET_FW_LOGS)));
        register_info(RS2_CAMERA_INFO_ADVANCED_MODE, ((advanced_mode) ? "YES" : "NO"));
        register_info(RS2_CAMERA_INFO_PRODUCT_ID, pid_hex_str);
        register_info(RS2_CAMERA_INFO_PRODUCT_LINE, "D500");
        // Uncomment once D500 recommended FW exist
        //register_info(RS2_CAMERA_INFO_RECOMMENDED_FIRMWARE_VERSION, _recommended_fw_version);
        register_info(RS2_CAMERA_INFO_CAMERA_LOCKED, _is_locked ? "YES" : "NO");

        if (_pid == D585S_PID)
        {
            register_info(RS2_CAMERA_INFO_SMCU_FW_VERSION, gvd_parsed_fields.safety_sw_suite_version);
        }

        if (usb_modality)
        {
            register_info(RS2_CAMERA_INFO_CONNECTION_TYPE, "USB");
            register_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR, usb_type_str);
        }
        register_info( RS2_CAMERA_INFO_IMU_TYPE, gvd_parsed_fields.imu_type );

        register_features();

        d500_auto_calibrated::add_depth_write_observer( [this]()
        {
            _coefficients_table_raw.reset();
            _new_calib_table_raw.reset();
        } );
    }

    void d500_device::register_features()
    {
        register_feature( std::make_shared< amplitude_factor_feature >() );

        register_feature( std::make_shared< auto_exposure_roi_feature >( get_depth_sensor(), _hw_monitor ) );
    }

    platform::usb_spec d500_device::get_usb_spec() const
    {
        if(!supports_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR))
            return platform::usb_undefined;
        auto str = get_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR);
        for (auto u : platform::usb_spec_names)
        {
            if (u.second.compare(str) == 0)
                return u.first;
        }
        return platform::usb_undefined;
    }


    double d500_device::get_device_time_ms()
    {
        //// TODO: Refactor the following query with an extension.
        //if (dynamic_cast<const platform::playback_backend*>(&(get_context()->get_backend())) != nullptr)
        //{
        //    throw not_implemented_exception("device time not supported for backend.");
        //}

        if (!_hw_monitor)
            throw wrong_api_call_sequence_exception("_hw_monitor is not initialized yet");

        command cmd(ds::MRD, ds::REGISTER_CLOCK_0, ds::REGISTER_CLOCK_0 + 4);
        auto res = _hw_monitor->send(cmd);

        if (res.size() < sizeof(uint32_t))
        {
            LOG_DEBUG("size(res):" << res.size());
            throw std::runtime_error("Not enough bytes returned from the firmware!");
        }
        uint32_t dt = *(uint32_t*)res.data();
        double ts = dt * TIMESTAMP_USEC_TO_MSEC;
        return ts;
    }

    command d500_device::get_firmware_logs_command() const
    {
        return command{ ds::GET_FW_LOGS };
    }

    bool d500_device::check_symmetrization_enabled() const
    {
        // The following try catch block has been added to avoid
        // device's constructor failure for users working with new librealsense
        // version and old fw version (which would not have the stream pipe config table)
        // Only the content of the try statements should be kept, after some time.
        try
        {
            using namespace ds;
            command cmd(GET_HKR_CONFIG_TABLE,
                static_cast<int>(d500_calib_location::d500_calib_ram_memory),
                static_cast<int>(d500_calibration_table_id::stream_pipe_config_id),
                static_cast<int>(d500_calib_type::d500_calib_dynamic));
            auto res = _hw_monitor->send(cmd);
       
            if (res.size() != sizeof(d500_stream_pipe_config_table))
                throw invalid_value_exception("Stream Config table has unexpected length");
            auto stream_pipe_config_table = check_calib<d500_stream_pipe_config_table>(res);
            return stream_pipe_config_table->is_depth_symmetrization_enabled == 1;
        }
        catch (...)
        {
            command cmd{ ds::MRD, 0x80000004, 0x80000008 };
            auto res = _hw_monitor->send(cmd);
            uint32_t val = *reinterpret_cast<uint32_t*>(res.data());
            return val == 1;
        }
    }
    
    void d500_device::get_gvd_details(const std::vector<uint8_t>& gvd_buff, ds::d500_gvd_parsed_fields* parsed_fields) const
    {
        using namespace ds::d500_gvd_offsets;
        parsed_fields->gvd_version[0] = *reinterpret_cast<const uint8_t*>(gvd_buff.data() + version_offset);
        parsed_fields->gvd_version[1] = *reinterpret_cast<const uint8_t*>(gvd_buff.data() + version_offset + sizeof(uint8_t));

        parsed_fields->payload_size = *reinterpret_cast<const uint32_t*>(gvd_buff.data() + payload_size_offset);
        parsed_fields->crc32 = *reinterpret_cast<const uint32_t*>(gvd_buff.data() + crc32_offset);
        parsed_fields->optical_module_sn = _hw_monitor->get_module_serial_string(gvd_buff, optical_module_serial_offset);
        parsed_fields->mb_module_sn = _hw_monitor->get_module_serial_string(gvd_buff, mb_module_serial_offset);
        parsed_fields->fw_version = _hw_monitor->get_firmware_version_string<uint16_t>(gvd_buff, fw_version_offset, 4, false);
        if (_pid == ds::D585S_PID)
        {
            parsed_fields->safety_sw_suite_version = _hw_monitor->get_firmware_version_string<uint8_t>(gvd_buff, safety_sw_suite_version_offset, 4, false);
        }

        constexpr size_t gvd_header_size = 8;
        auto gvd_payload_data = gvd_buff.data() + gvd_header_size;
        auto computed_crc = rsutils::number::calc_crc32( gvd_payload_data, parsed_fields->payload_size );
        LOG_DEBUG( "D500 GVD version is: " << static_cast< int >( parsed_fields->gvd_version[0] ) << "."
                                           << static_cast< int >( parsed_fields->gvd_version[1] )
                                           << "\n\tD500 GVD payload_size is: " << parsed_fields->payload_size );

        if( computed_crc != parsed_fields->crc32 )
        {
            LOG_ERROR( "CRC mismatch in D500 GVD - received CRC = "
                << parsed_fields->crc32 << ", computed CRC = " << computed_crc );
        }

    }

    void d500_device::set_imu_type( const std::vector< uint8_t > & gvd_buf, ds::d500_gvd_parsed_fields * parsed_fields )
    {
        // setting imu type in gvd parsed fields
        if ((_device_capabilities & ds::ds_caps::CAP_IMU_SENSOR) == ds::ds_caps::CAP_IMU_SENSOR)
        {
            const char * imu_type_char = reinterpret_cast< const char * >( gvd_buf.data() + ds::d500_gvd_offsets::imu_type );
            parsed_fields->imu_type.assign( imu_type_char, strnlen( imu_type_char, 8 ) );
        }
        else
        {
            parsed_fields->imu_type = "IMU_Unknown";
            LOG_ERROR("IMU type could not be recognized!");
        }

        // updating device capabilities based on imu type
        if(parsed_fields->imu_type == "BMI055")
            _device_capabilities |= ds::ds_caps::CAP_BMI_055;
        else if (parsed_fields->imu_type == "BMI085")
            _device_capabilities |= ds::ds_caps::CAP_BMI_085;
        else if( parsed_fields->imu_type == "BMI088" )
            _device_capabilities |= ds::ds_caps::CAP_BMI_088;
    }
}
