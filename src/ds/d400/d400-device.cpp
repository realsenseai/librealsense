// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016-24 RealSense, Inc. All Rights Reserved.

#include <librealsense2/h/rs_internal.h>
#include <src/device.h>
#include <src/image.h>
#include <src/metadata-parser.h>
#include <src/metadata.h>
#include <src/backend.h>

#include "d400-device.h"
#include "d400-info.h"
#include "ds/ds-timestamp.h"
#include <src/stream.h>
#include <src/environment.h>
#include <src/platform/platform-utils.h>

#include <src/ds/features/amplitude-factor-feature.h>
#include <src/ds/features/emitter-frequency-feature.h>
#include <src/ds/features/auto-exposure-roi-feature.h>
#include <src/ds/features/remove-ir-pattern-feature.h>

#include <src/proc/depth-formats-converter.h>
#include <src/proc/y8i-to-y8y8.h>
#include <src/proc/y8i-to-y8y8-mipi.h>
#include <src/proc/y12i-to-y16y16.h>
#include <src/proc/y12i-to-y16y16-mipi.h>
#include <src/proc/color-formats-converter.h>

#include <src/hdr-config.h>
#include <src/ds/ds-thermal-monitor.h>
#include <common/fw/firmware-version.h>
#include <src/fw-update/fw-update-unsigned.h>

#include <rsutils/lazy.h>
#include <rsutils/type/fourcc.h>
using rsutils::type::fourcc;

#include <rsutils/string/hexdump.h>
#include <regex>
#include <iterator>

#include <src/ds/features/auto-exposure-limit-feature.h>
#include <src/ds/features/gain-limit-feature.h>

#ifdef HWM_OVER_XU
constexpr bool hw_mon_over_xu = true;
#else
constexpr bool hw_mon_over_xu = false;
#endif

namespace librealsense
{
    std::map<fourcc::value_type, rs2_format> d400_depth_fourcc_to_rs2_format = {
        {fourcc('Y','U','Y','2'), RS2_FORMAT_YUYV},
        {fourcc('Y','U','Y','V'), RS2_FORMAT_YUYV},
        {fourcc('U','Y','V','Y'), RS2_FORMAT_UYVY},
        {fourcc('G','R','E','Y'), RS2_FORMAT_Y8},
        {fourcc('Y','8','I',' '), RS2_FORMAT_Y8I},
        {fourcc('W','1','0',' '), RS2_FORMAT_W10},
        {fourcc('Y','1','6',' '), RS2_FORMAT_Y16},
        {fourcc('Y','1','2','I'), RS2_FORMAT_Y12I},
        {fourcc('Z','1','6',' '), RS2_FORMAT_Z16},
        {fourcc('R','G','B','2'), RS2_FORMAT_BGR8},
        {fourcc('M','J','P','G'), RS2_FORMAT_MJPEG},
        {fourcc('B','Y','R','2'), RS2_FORMAT_RAW16}

    };
    std::map<fourcc::value_type, rs2_stream> d400_depth_fourcc_to_rs2_stream = {
        {fourcc('Y','U','Y','2'), RS2_STREAM_COLOR},
        {fourcc('Y','U','Y','V'), RS2_STREAM_COLOR},
        {fourcc('U','Y','V','Y'), RS2_STREAM_INFRARED},
        {fourcc('G','R','E','Y'), RS2_STREAM_INFRARED},
        {fourcc('Y','8','I',' '), RS2_STREAM_INFRARED},
        {fourcc('W','1','0',' '), RS2_STREAM_INFRARED},
        {fourcc('Y','1','6',' '), RS2_STREAM_INFRARED},
        {fourcc('Y','1','2','I'), RS2_STREAM_INFRARED},
        {fourcc('R','G','B','2'), RS2_STREAM_INFRARED},
        {fourcc('Z','1','6',' '), RS2_STREAM_DEPTH},
        {fourcc('Z','1','6','H'), RS2_STREAM_DEPTH},
        {fourcc('B','Y','R','2'), RS2_STREAM_COLOR},
        {fourcc('M','J','P','G'), RS2_STREAM_COLOR}
    };

    std::vector<uint8_t> d400_device::send_receive_raw_data(const std::vector<uint8_t>& input)
    {
        return _hw_monitor->send(input);
    }
    
    std::vector<uint8_t> d400_device::build_command(uint32_t opcode,
        uint32_t param1,
        uint32_t param2,
        uint32_t param3,
        uint32_t param4,
        uint8_t const * data,
        size_t dataLength) const
    {
        return _hw_monitor->build_command(opcode, param1, param2, param3, param4, data, dataLength);
    }

    void d400_device::hardware_reset()
    {
        command cmd(ds::HWRST);
        cmd.require_response = false;
        _hw_monitor->send(cmd);
    }

    void d400_device::enter_update_state() const
    {
        // preparing HWM command
        command cmd(ds::DFU);
        cmd.param1 = 1;
        cmd.require_response = false;

        _ds_device_common->enter_update_state(cmd);
    }

    std::vector<uint8_t> d400_device::backup_flash( rs2_update_progress_callback_sptr callback )
    {
        return _ds_device_common->backup_flash(callback);
    }

    void d400_device::update_flash(const std::vector<uint8_t>& image, rs2_update_progress_callback_sptr callback, int update_mode)
    {
        _ds_device_common->update_flash(image, callback, update_mode);
    }

    bool d400_device::check_fw_compatibility( const std::vector< uint8_t > & image ) const
    {
        // check if the given FW size matches the expected FW size
        if( ( image.size() != signed_fw_size ) )
            throw librealsense::invalid_value_exception(
                rsutils::string::from() << "Unsupported firmware binary image provided - " << image.size() << " bytes" );

        std::string fw_version = ds::extract_firmware_version_string( image );

        auto it = ds::d400_device_to_fw_min_version.find( _pid );
        if( it == ds::d400_device_to_fw_min_version.end() )
        {
            throw librealsense::invalid_value_exception(
                rsutils::string::from()
                << "Min and Max firmware versions have not been defined for this device: "
                << std::hex << _pid );
        }
        bool result = ( firmware_version( fw_version ) >= firmware_version( it->second ) );
        if( ! result )
            LOG_ERROR( "Firmware version isn't compatible " << fw_version );

        return result;
    }

    std::string d400_device::get_opcode_string(int opcode) const
    {
        return ds::d400_hwmon_response().hwmon_error2str(opcode);
    }

    d400_depth_sensor::d400_depth_sensor( d400_device * owner, std::shared_ptr<uvc_sensor> uvc_sensor)
        : synthetic_sensor(ds::DEPTH_STEREO, uvc_sensor, owner, d400_depth_fourcc_to_rs2_format, d400_depth_fourcc_to_rs2_stream)
        , _owner(owner)
        , _depth_units(-1)
        , _hdr_cfg(nullptr)
    {
    }

    processing_blocks d400_depth_sensor::get_recommended_processing_blocks() const
    {
        return get_ds_depth_recommended_proccesing_blocks();
    }

    rs2_intrinsics d400_depth_sensor::get_intrinsics( const stream_profile & profile ) const
    {
        rs2_intrinsics result;

        if (ds::try_get_d400_intrinsic_by_resolution_new(*_owner->_new_calib_table_raw,
            profile.width, profile.height, &result))
        {
            return result;
        }
        else 
        {
            return get_d400_intrinsic_by_resolution(
                *_owner->_coefficients_table_raw,
                ds::d400_calibration_table_id::coefficients_table_id,
                profile.width, profile.height);
        }
    }

    void d400_depth_sensor::set_frame_metadata_modifier( on_frame_md callback )
    {
        _metadata_modifier = callback;
        auto s = get_raw_sensor().get();
        auto uvc = As< librealsense::uvc_sensor >(s);
        if(uvc)
            uvc->set_frame_metadata_modifier(callback);
    }

    void d400_depth_sensor::open( const stream_profiles & requests )
    {
        group_multiple_fw_calls(*this, [&]() {
            _depth_units = get_option(RS2_OPTION_DEPTH_UNITS).query();
            set_frame_metadata_modifier([&](frame_additional_data& data) {data.depth_units = _depth_units.load(); });

            synthetic_sensor::open(requests);

            // Activate Thermal Compensation tracking
            if (supports_option(RS2_OPTION_THERMAL_COMPENSATION))
                _owner->_thermal_monitor->update(true);
            }); //group_multiple_fw_calls
    }

    void d400_depth_sensor::close()
    {
        // Deactivate Thermal Compensation tracking
        if (supports_option(RS2_OPTION_THERMAL_COMPENSATION))
            _owner->_thermal_monitor->update(false);

        synthetic_sensor::close();
    }

    rs2_intrinsics d400_depth_sensor::get_color_intrinsics( const stream_profile & profile ) const
    {
        if( _owner->_pid == ds::RS405_PID )
            return ds::get_d405_color_stream_intrinsic( *_owner->_color_calib_table_raw,
                                                        profile.width,
                                                        profile.height );

        return get_d400_intrinsic_by_resolution( *_owner->_color_calib_table_raw,
                                                    ds::d400_calibration_table_id::rgb_calibration_id,
                                                    profile.width,
                                                    profile.height );
    }

    //Infrared profiles are initialized with the following logic:
    //- If device has color sensor (D415 / D435), infrared profile is chosen with Y8 format
    //- If device does not have color sensor:
    //    * if it is a rolling shutter device (D400 / D410 / D415 / D405), infrared profile is chosen with RGB8 format
    //    * for other devices (D420 / D430), infrared profile is chosen with Y8 format
    stream_profiles d400_depth_sensor::init_stream_profiles()
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
                std::weak_ptr<d400_depth_sensor> wp =
                    std::dynamic_pointer_cast<d400_depth_sensor>(this->shared_from_this());
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
                std::weak_ptr<d400_depth_sensor> wp =
                    std::dynamic_pointer_cast<d400_depth_sensor>(this->shared_from_this());
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

    float d400_depth_sensor::get_depth_scale() const
    {
        if (_depth_units < 0)
            _depth_units = get_option(RS2_OPTION_DEPTH_UNITS).query();
        return _depth_units;
    }

    void d400_depth_sensor::set_depth_scale( float val )
    {
        _depth_units = val;
        set_frame_metadata_modifier([&](frame_additional_data& data) {data.depth_units = _depth_units.load(); });
    }

    void d400_depth_sensor::init_hdr_config( const option_range & exposure_range, const option_range & gain_range )
    {
        _hdr_cfg = std::make_shared<hdr_config>(*(_owner->_hw_monitor), get_raw_sensor(),
            exposure_range, gain_range, ds::d400_hwmon_response::opcodes::NO_DATA_TO_RETURN);
    }

    float d400_depth_sensor::get_stereo_baseline_mm() const
    {
        return _owner->get_stereo_baseline_mm();
    }

    float d400_depth_sensor::get_preset_max_value() const
    {
        float preset_max_value = RS2_RS400_VISUAL_PRESET_COUNT - 1;
        switch (_owner->_pid)
        {
        case ds::RS400_PID:
        case ds::RS410_PID:
        case ds::RS415_PID:
        case ds::RS460_PID:
            preset_max_value = static_cast<float>(RS2_RS400_VISUAL_PRESET_REMOVE_IR_PATTERN);
            break;
        default:
            preset_max_value = static_cast<float>(RS2_RS400_VISUAL_PRESET_MEDIUM_DENSITY);
        }
        return preset_max_value;
    }

    class ds5u_depth_sensor : public d400_depth_sensor
    {
    public:
        explicit ds5u_depth_sensor(ds5u_device* owner,
            std::shared_ptr<uvc_sensor> uvc_sensor)
            : d400_depth_sensor(owner, uvc_sensor), _owner(owner)
        {}

        stream_profiles init_stream_profiles() override
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
                auto&& video = dynamic_cast<video_stream_profile_interface*>(p.get());

                // Register intrinsics
                if (p->get_format() != RS2_FORMAT_Y16) // Y16 format indicate unrectified images, no intrinsics are available for these
                {
                    const auto&& profile = to_profile(p.get());
                    std::weak_ptr<d400_depth_sensor> wp = std::dynamic_pointer_cast<d400_depth_sensor>(this->shared_from_this());
                    video->set_intrinsics([profile, wp]()
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

    private:
        const ds5u_device* _owner;
    };

    bool d400_device::is_camera_in_advanced_mode() const
    {
        return _ds_device_common->is_camera_in_advanced_mode();
    }

    float d400_device::get_stereo_baseline_mm() const
    {
        using namespace ds;
        auto table = check_calib<d400_coefficients_table>(*_coefficients_table_raw);
        return fabs(table->baseline);
    }

    std::vector<uint8_t> d400_device::get_d400_raw_calibration_table(ds::d400_calibration_table_id table_id) const
    {
        command cmd(ds::GETINTCAL, static_cast<int>(table_id));
        return _hw_monitor->send(cmd);
    }

    std::vector<uint8_t> d400_device::get_new_calibration_table() const
    {
        if (_fw_version >= firmware_version("5.11.9.5"))
        {
            command cmd(ds::RECPARAMSGET);
            return _hw_monitor->send(cmd);
        }
        return {};
    }

    ds::ds_caps d400_device::parse_device_capabilities( const std::vector<uint8_t> &gvd_buf ) const
    {
        using namespace ds;

        // Opaque retrieval
        ds_caps val{ds_caps::CAP_UNDEFINED};
        if (gvd_buf[d400_gvd::d400_gvd_offsets::active_projector])  // DepthActiveMode
            val |= ds_caps::CAP_ACTIVE_PROJECTOR;
        if (gvd_buf[d400_gvd::d400_gvd_offsets::rgb_sensor])                           // WithRGB
            val |= ds_caps::CAP_RGB_SENSOR;
        if (gvd_buf[d400_gvd::d400_gvd_offsets::imu_sensor])
        {
            val |= ds_caps::CAP_IMU_SENSOR;
            if (gvd_buf[d400_gvd::d400_gvd_offsets::imu_acc_chip_id] == I2C_IMU_BMI055_ID_ACC)
            {
                val |= ds_caps::CAP_BMI_055;
                LOG_DEBUG("The IMU sensor is for PID " << std::hex << _pid << " has been identified as BMI055" << std::dec);
            }
            else if (gvd_buf[d400_gvd::d400_gvd_offsets::imu_acc_chip_id] == I2C_IMU_BMI085_ID_ACC)
            {
                val |= ds_caps::CAP_BMI_085;
                LOG_DEBUG("The IMU sensor is for PID " << std::hex << _pid << " has been identified as BMI085" << std::dec);
            }
            else if (gvd_buf[d400_gvd::d400_gvd_offsets::imu_acc_chip_id] == I2C_IMU_BMI088_ID_ACC)
            {
                val |= ds_caps::CAP_BMI_088;
                LOG_DEBUG("The IMU sensor is for PID " << std::hex << _pid << " has been identified as BMI088" << std::dec);
            }
            else if (d400_hid_bmi_055_pid.end() != d400_hid_bmi_055_pid.find(_pid))
            {
                val |= ds_caps::CAP_BMI_055;
                LOG_DEBUG("The IMU sensor is for PID " << std::hex << _pid << " has been identified as BMI055" << std::dec);
            }
            else if (d400_hid_bmi_085_pid.end() != d400_hid_bmi_085_pid.find(_pid))
            {
                val |= ds_caps::CAP_BMI_085;
                LOG_DEBUG("The IMU sensor is for PID " << std::hex << _pid << " has been identified as BMI085" << std::dec);
            }
            else
                LOG_WARNING("The IMU sensor is undefined for PID " << std::hex << _pid << " and imu_chip_id: " << gvd_buf[d400_gvd::d400_gvd_offsets::imu_acc_chip_id] << std::dec);
        }
        if( 0xFF != ( gvd_buf[d400_gvd::d400_gvd_offsets::fisheye_sensor_lb] & gvd_buf[d400_gvd::d400_gvd_offsets::fisheye_sensor_hb] ) )
            val |= ds_caps::CAP_FISHEYE_SENSOR;
        if( 0x1 == gvd_buf[d400_gvd::d400_gvd_offsets::depth_sensor_type] )
            val |= ds_caps::CAP_ROLLING_SHUTTER;  // e.g. ASRC
        if( 0x2 == gvd_buf[d400_gvd::d400_gvd_offsets::depth_sensor_type] )
            val |= ds_caps::CAP_GLOBAL_SHUTTER;   // e.g. AWGC
        // Option INTER_CAM_SYNC_MODE is not enabled in D405
        if (_pid != ds::RS405_PID)
            val |= ds_caps::CAP_INTERCAM_HW_SYNC;
        if (gvd_buf[d400_gvd::d400_gvd_offsets::ip65_sealed_offset] == 0x1)
            val |= ds_caps::CAP_IP65;
        if (gvd_buf[d400_gvd::d400_gvd_offsets::ir_filter_offset] == 0x1)
            val |= ds_caps::CAP_IR_FILTER;
        return val;
    }

    std::shared_ptr<synthetic_sensor> d400_device::create_depth_device(std::shared_ptr<context> ctx,
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
        frame_timestamp_reader* timestamp_reader_from_metadata;
        if (!(ds::d400_mipi_device_pid.count(all_device_infos.front().pid) > 0))
            timestamp_reader_from_metadata = new ds_timestamp_reader_from_metadata(std::move(timestamp_reader_backup));
        else
            timestamp_reader_from_metadata = new ds_timestamp_reader_from_metadata_mipi(std::move(timestamp_reader_backup));
        
        std::unique_ptr<frame_timestamp_reader> timestamp_reader_metadata(timestamp_reader_from_metadata);
        auto enable_global_time_option = std::shared_ptr<global_time_option>(new global_time_option());

        auto raw_depth_ep = std::make_shared<uvc_sensor>("Raw Depth Sensor", std::make_shared<platform::multi_pins_uvc_device>(depth_devices),
            std::unique_ptr<frame_timestamp_reader>(new global_timestamp_reader(std::move(timestamp_reader_metadata), _tf_keeper, enable_global_time_option)), this);

        raw_depth_ep->register_xu(depth_xu); // make sure the XU is initialized every time we power the camera

        auto depth_ep = std::make_shared<d400_depth_sensor>(this, raw_depth_ep);

        depth_ep->register_info(RS2_CAMERA_INFO_PHYSICAL_PORT, filter_by_mi(all_device_infos, 0).front().device_path);

        depth_ep->register_option(RS2_OPTION_GLOBAL_TIME_ENABLED, enable_global_time_option);

        depth_ep->register_processing_block(processing_block_factory::create_id_pbf(RS2_FORMAT_Y8, RS2_STREAM_INFRARED, 1));
        depth_ep->register_processing_block(processing_block_factory::create_id_pbf(RS2_FORMAT_Z16, RS2_STREAM_DEPTH));

        depth_ep->register_processing_block({ {RS2_FORMAT_W10} }, { {RS2_FORMAT_RAW10, RS2_STREAM_INFRARED, 1} }, []() { return std::make_shared<w10_converter>(RS2_FORMAT_RAW10); });
        depth_ep->register_processing_block({ {RS2_FORMAT_W10} }, { {RS2_FORMAT_Y10BPACK, RS2_STREAM_INFRARED, 1} }, []() { return std::make_shared<w10_converter>(RS2_FORMAT_Y10BPACK); });

        return depth_ep;
    }

    d400_device::d400_device( std::shared_ptr< const d400_info > const & dev_info )
        : backend_device(dev_info), global_time_interface(),
          auto_calibrated(),
          _device_capabilities(ds::ds_caps::CAP_UNDEFINED),
          _depth_stream(new stream(RS2_STREAM_DEPTH)),
          _left_ir_stream(new stream(RS2_STREAM_INFRARED, 1)),
          _right_ir_stream(new stream(RS2_STREAM_INFRARED, 2)),
          _color_stream(nullptr)
    {
        _depth_device_idx = add_sensor( create_depth_device( dev_info->get_context(), dev_info->get_group().uvc_devices ) );
        init( dev_info->get_context(), dev_info->get_group() );
    }

    void d400_device::init(std::shared_ptr<context> ctx,
        const platform::backend_device_group& group)
    {
        using namespace ds;

        auto raw_sensor = get_raw_depth_sensor();
        _pid = group.uvc_devices.front().pid;

        _is_mipi_device = (ds::d400_mipi_device_pid.count(_pid) > 0);

        _color_calib_table_raw = [this]()
        {
            return get_d400_raw_calibration_table(d400_calibration_table_id::rgb_calibration_id);
        };

        if (((hw_mon_over_xu) && (RS400_IMU_PID != _pid)) || (!group.usb_devices.size()))
        {
            _hw_monitor = std::make_shared<hw_monitor>(
                std::make_shared<locked_transfer>(
                    std::make_shared<command_transfer_over_xu>( *raw_sensor, depth_xu, DS5_HWMONITOR ),
                    raw_sensor ), std::make_shared<ds::d400_hwmon_response>());
        }
        else
        {
            if( !_is_mipi_device)
                _hw_monitor = std::make_shared< hw_monitor >(
                    std::make_shared< locked_transfer >(
                        get_backend()->create_usb_device( group.usb_devices.front() ),
                        raw_sensor ), std::make_shared<ds::d400_hwmon_response>());
        }
        set_hw_monitor_for_auto_calib(_hw_monitor);


        _ds_device_common = std::make_shared<ds_device_common>(this, _hw_monitor, (_is_mipi_device) ? true : false);
        

        // Define Left-to-Right extrinsics calculation (lazy)
        // Reference CS - Right-handed; positive [X,Y,Z] point to [Left,Up,Forward] accordingly.
        _left_right_extrinsics = std::make_shared< rsutils::lazy< rs2_extrinsics > >(
            [this]()
            {
                rs2_extrinsics ext = identity_matrix();
                auto table = check_calib<d400_coefficients_table>(*_coefficients_table_raw);
                ext.translation[0] = 0.001f * table->baseline; // mm to meters
                return ext;
            });

        environment::get_instance().get_extrinsics_graph().register_same_extrinsics(*_depth_stream, *_left_ir_stream);
        environment::get_instance().get_extrinsics_graph().register_extrinsics(*_depth_stream, *_right_ir_stream, _left_right_extrinsics);

        register_stream_to_extrinsic_group(*_depth_stream, 0);
        register_stream_to_extrinsic_group(*_left_ir_stream, 0);
        register_stream_to_extrinsic_group(*_right_ir_stream, 0);

        _coefficients_table_raw = [this]() { return get_d400_raw_calibration_table(d400_calibration_table_id::coefficients_table_id); };
        _new_calib_table_raw = [this]() { return get_new_calibration_table(); };

        std::string device_name = (rs400_sku_names.end() != rs400_sku_names.find(_pid)) ? rs400_sku_names.at(_pid) : "RS4xx";

        std::vector<uint8_t> gvd_buff(HW_MONITOR_BUFFER_SIZE);

        auto& depth_sensor = get_depth_sensor();
        auto raw_depth_sensor = get_raw_depth_sensor();

        using namespace platform;

        // minimal firmware version in which hdr feature is supported
        firmware_version hdr_firmware_version("5.12.8.100");

        std::string optic_serial, asic_serial, pid_hex_str, usb_type_str;
        bool advanced_mode, usb_modality;
        group_multiple_fw_calls(depth_sensor, [&]() {

            _hw_monitor->get_gvd(gvd_buff.size(), gvd_buff.data(), GVD);

            std::string fwv;
            _ds_device_common->get_fw_details( gvd_buff, optic_serial, asic_serial, fwv );

            _fw_version = firmware_version(fwv);

            _recommended_fw_version = firmware_version(D4XX_RECOMMENDED_FIRMWARE_VERSION);
            if (_fw_version >= firmware_version("5.10.4.0"))
                _device_capabilities = parse_device_capabilities( gvd_buff );
        
            set_imu_type();

            //D457 Development
            advanced_mode = is_camera_in_advanced_mode();
            auto _usb_mode = usb3_type;
            usb_type_str = usb_spec_names.at(_usb_mode);
            usb_modality = (_fw_version >= firmware_version("5.9.8.0"));
            if (usb_modality)
            {
                _usb_mode = raw_depth_sensor->get_usb_specification();
                if (usb_spec_names.count(_usb_mode) && (usb_undefined != _usb_mode))
                    usb_type_str = usb_spec_names.at(_usb_mode);
                else  // Backend fails to provide USB descriptor  - occurs with RS3 build. Requires further work
                    usb_modality = false;
            }

            if (!_is_mipi_device)
            {
                depth_sensor.register_processing_block(
                    { {RS2_FORMAT_Y8I} },
                    { {RS2_FORMAT_Y8, RS2_STREAM_INFRARED, 1} , {RS2_FORMAT_Y8, RS2_STREAM_INFRARED, 2} },
                    []() { return std::make_shared<y8i_to_y8y8>(); }
                ); // L+R

                depth_sensor.register_processing_block(
                    { RS2_FORMAT_Y12I },
                    { {RS2_FORMAT_Y16, RS2_STREAM_INFRARED, 1}, {RS2_FORMAT_Y16, RS2_STREAM_INFRARED, 2} },
                    []() {return std::make_shared<y12i_to_y16y16>(); }
                );
            }
            else
            {
                depth_sensor.register_processing_block(
                    { {RS2_FORMAT_Y8I} },
                    { {RS2_FORMAT_Y8, RS2_STREAM_INFRARED, 1} , {RS2_FORMAT_Y8, RS2_STREAM_INFRARED, 2} },
                    []() { return std::make_shared<y8i_to_y8y8_mipi>(); }
                ); // L+R

                 depth_sensor.register_processing_block(
                    { RS2_FORMAT_Y12I },
                    { {RS2_FORMAT_Y16, RS2_STREAM_INFRARED, 1}, {RS2_FORMAT_Y16, RS2_STREAM_INFRARED, 2} },
                    []() {return std::make_shared<y12i_to_y16y16_mipi>(); }
                );
            }
            

            pid_hex_str = rsutils::string::from() << std::uppercase << rsutils::string::hexdump( _pid );

            if ((_pid == RS416_PID || _pid == RS416_RGB_PID) && _fw_version >= firmware_version("5.12.0.1"))
            {
                depth_sensor.register_option(RS2_OPTION_HARDWARE_PRESET,
                    std::make_shared<uvc_xu_option<uint8_t>>( raw_depth_sensor, depth_xu, DS5_HARDWARE_PRESET,
                        "Hardware pipe configuration" ) );
                depth_sensor.register_option(RS2_OPTION_LED_POWER,
                    std::make_shared<uvc_xu_option<uint16_t>>( raw_depth_sensor, depth_xu, DS5_LED_PWR,
                        "Set the power level of the LED, with 0 meaning LED off"));
            }

            if (_fw_version >= firmware_version("5.6.3.0"))
            {
                _is_locked = _ds_device_common->is_locked( gvd_buff.data(), d400_gvd::d400_gvd_offsets::is_camera_locked_offset );
            }

            if (_fw_version >= firmware_version("5.5.8.0"))
            //if hw_monitor was created by usb replace it with xu
            // D400_IMU will remain using USB interface due to HW limitations
            {
                if (_is_mipi_device)
                {
                    depth_sensor.register_option(RS2_OPTION_ASIC_TEMPERATURE,
                        std::make_shared<asic_temperature_option_mipi>(_hw_monitor,
                            RS2_OPTION_ASIC_TEMPERATURE));
                }
                else
                {
                    depth_sensor.register_option(RS2_OPTION_OUTPUT_TRIGGER_ENABLED,
                        std::make_shared<uvc_xu_option<uint8_t>>( raw_depth_sensor, depth_xu, DS5_EXT_TRIGGER,
                            "Generate trigger from the camera to external device once per frame"));

                    depth_sensor.register_option(RS2_OPTION_ASIC_TEMPERATURE,
                        std::make_shared< asic_and_projector_temperature_options >( raw_depth_sensor,
                                                                                    RS2_OPTION_ASIC_TEMPERATURE ) );

                    // D457 dev - get_xu fails for D457 - error polling id not defined
                    auto error_control = std::make_shared<uvc_xu_option<uint8_t>>( raw_depth_sensor, depth_xu,
                                                                                   DS5_ERROR_REPORTING, "Error reporting");

                    _polling_error_handler = std::make_shared<polling_error_handler>(1000,
                        error_control,
                        raw_depth_sensor->get_notifications_processor(),
                        std::make_shared< ds_notification_decoder >( d400_fw_error_report ) );

                    depth_sensor.register_option(RS2_OPTION_ERROR_POLLING_ENABLED, std::make_shared<polling_errors_disable>(_polling_error_handler));
                }
            }

            if ((val_in_range(_pid, { RS455_PID })) && (_fw_version >= firmware_version("5.12.11.0")))
            {
                auto thermal_compensation_toggle = std::make_shared<protected_xu_option<uint8_t>>( raw_depth_sensor, depth_xu,
                    ds::DS5_THERMAL_COMPENSATION, "Toggle Thermal Compensation Mechanism");

                auto temperature_sensor = depth_sensor.get_option_handler(RS2_OPTION_ASIC_TEMPERATURE);

                _thermal_monitor = std::make_shared<ds_thermal_monitor>(temperature_sensor, thermal_compensation_toggle);

                depth_sensor.register_option(RS2_OPTION_THERMAL_COMPENSATION,
                std::make_shared<thermal_compensation>(_thermal_monitor,thermal_compensation_toggle));
            }

            auto ir_filter_mask = ds_caps::CAP_IR_FILTER;
            if (val_in_range(_pid, { RS435_RGB_PID, RS435I_PID, RS455_PID }) &&
                (_device_capabilities & ir_filter_mask) == ir_filter_mask &&
                is_capability_supports(ds_caps::CAP_IR_FILTER, gvd_buff[d400_gvd::d400_gvd_offsets::gvd_version_offset]))
            {
                update_device_name(device_name, ds_caps::CAP_IR_FILTER);
            }

            auto ip65_mask = ds_caps::CAP_IP65;
            if (val_in_range(_pid, { RS455_PID })&&
                (_device_capabilities & ip65_mask) == ip65_mask &&
                is_capability_supports(ds_caps::CAP_IP65, gvd_buff[d400_gvd::d400_gvd_offsets::gvd_version_offset]))
            {
                update_device_name(device_name, ds_caps::CAP_IP65);
            }

            std::shared_ptr<option> exposure_option = nullptr;
            std::shared_ptr<option> gain_option = nullptr;
            std::shared_ptr<hdr_option> hdr_enabled_option = nullptr;

            //EXPOSURE AND GAIN - preparing uvc options
            auto uvc_xu_exposure_option = std::make_shared<uvc_xu_option<uint32_t>>( raw_depth_sensor,
                depth_xu,
                DS5_EXPOSURE,
                "Depth Exposure (usec)");
            option_range exposure_range = uvc_xu_exposure_option->get_range();
            auto uvc_pu_gain_option = std::make_shared<uvc_pu_option>( raw_depth_sensor, RS2_OPTION_GAIN);
            option_range gain_range = uvc_pu_gain_option->get_range();

            //AUTO EXPOSURE
            auto enable_auto_exposure = std::make_shared<uvc_xu_option<uint8_t>>( raw_depth_sensor,
                depth_xu,
                DS5_ENABLE_AUTO_EXPOSURE,
                "Enable Auto Exposure");
            depth_sensor.register_option(RS2_OPTION_ENABLE_AUTO_EXPOSURE, enable_auto_exposure);

            // register HDR options
            if (_fw_version >= hdr_firmware_version)
            {
                auto d400_depth = As<d400_depth_sensor, synthetic_sensor>(&get_depth_sensor());
                d400_depth->init_hdr_config(exposure_range, gain_range);
                auto&& hdr_cfg = d400_depth->get_hdr_config();

                // values from 4 to 14 - for internal use
                // value 15 - saved for emiter on off subpreset
                option_range hdr_id_range = { 0.f /*min*/, 3.f /*max*/, 1.f /*step*/, 1.f /*default*/ };
                auto hdr_id_option = std::make_shared<hdr_option>(hdr_cfg, RS2_OPTION_SEQUENCE_NAME, hdr_id_range,
                    std::map<float, std::string>{ {0.f, "0"}, { 1.f, "1" }, { 2.f, "2" }, { 3.f, "3" } });
                depth_sensor.register_option(RS2_OPTION_SEQUENCE_NAME, hdr_id_option);

                option_range hdr_sequence_size_range = { 2.f /*min*/, 2.f /*max*/, 1.f /*step*/, 2.f /*default*/ };
                auto hdr_sequence_size_option = std::make_shared<hdr_option>(hdr_cfg, RS2_OPTION_SEQUENCE_SIZE, hdr_sequence_size_range,
                    std::map<float, std::string>{ { 2.f, "2" } });
                depth_sensor.register_option(RS2_OPTION_SEQUENCE_SIZE, hdr_sequence_size_option);

                option_range hdr_sequ_id_range = { 0.f /*min*/, 2.f /*max*/, 1.f /*step*/, 0.f /*default*/ };
                auto hdr_sequ_id_option = std::make_shared<hdr_option>(hdr_cfg, RS2_OPTION_SEQUENCE_ID, hdr_sequ_id_range,
                    std::map<float, std::string>{ {0.f, "UVC"}, { 1.f, "1" }, { 2.f, "2" } });
                depth_sensor.register_option(RS2_OPTION_SEQUENCE_ID, hdr_sequ_id_option);

                option_range hdr_enable_range = { 0.f /*min*/, 1.f /*max*/, 1.f /*step*/, 0.f /*default*/ };
                hdr_enabled_option = std::make_shared<hdr_option>(hdr_cfg, RS2_OPTION_HDR_ENABLED, hdr_enable_range);
                depth_sensor.register_option(RS2_OPTION_HDR_ENABLED, hdr_enabled_option);

                //EXPOSURE AND GAIN - preparing hdr options
                auto hdr_exposure_option = std::make_shared<hdr_option>(hdr_cfg, RS2_OPTION_EXPOSURE, exposure_range);
                auto hdr_gain_option = std::make_shared<hdr_option>(hdr_cfg, RS2_OPTION_GAIN, gain_range);

                //EXPOSURE AND GAIN - preparing hybrid options
                auto hdr_conditional_exposure_option = std::make_shared<hdr_conditional_option>(hdr_cfg, uvc_xu_exposure_option, hdr_exposure_option);
                auto hdr_conditional_gain_option = std::make_shared<hdr_conditional_option>(hdr_cfg, uvc_pu_gain_option, hdr_gain_option);

                exposure_option = hdr_conditional_exposure_option;
                gain_option = hdr_conditional_gain_option;

                std::vector<std::pair<std::shared_ptr<option>, std::string>> options_and_reasons = { std::make_pair(hdr_enabled_option,
                        "Auto Exposure cannot be set while HDR is enabled") };
                depth_sensor.register_option(RS2_OPTION_ENABLE_AUTO_EXPOSURE,
                    std::make_shared<gated_option>(
                        enable_auto_exposure,
                        options_and_reasons));
            }
            else
            {
                exposure_option = uvc_xu_exposure_option;
                gain_option = uvc_pu_gain_option;
            }

            // DEPTH AUTO EXPOSURE MODE
            if ((val_in_range(_pid, { RS455_PID })) && (_fw_version >= firmware_version("5.15.0.0")))
            {
                auto depth_auto_exposure_mode = std::make_shared<uvc_xu_option<uint8_t>>( raw_depth_sensor,
                    depth_xu,
                    DS5_DEPTH_AUTO_EXPOSURE_MODE,
                    "Depth Auto Exposure Mode",
                    std::map<float, std::string>{
                    { (float)RS2_DEPTH_AUTO_EXPOSURE_REGULAR, "Regular" },
                    { (float)RS2_DEPTH_AUTO_EXPOSURE_ACCELERATED, "Accelerated" } } , false);

                depth_sensor.register_option(
                    RS2_OPTION_DEPTH_AUTO_EXPOSURE_MODE, depth_auto_exposure_mode );
            }

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

            // Alternating laser pattern is applicable for global shutter/active SKUs
            auto mask = ds_caps::CAP_GLOBAL_SHUTTER | ds_caps::CAP_ACTIVE_PROJECTOR;
            // Alternating laser pattern should be set and query in a different way according to the firmware version
            if ((_fw_version >= firmware_version("5.11.3.0")) && ((_device_capabilities & mask) == mask))
            {
                bool is_fw_version_using_id = (_fw_version >= firmware_version("5.12.8.100"));
                auto alternating_emitter_opt = std::make_shared<alternating_emitter_option>(*_hw_monitor, is_fw_version_using_id, ds::d400_hwmon_response::opcodes::NO_DATA_TO_RETURN);
                auto emitter_always_on_opt = std::make_shared<emitter_always_on_option>( _hw_monitor, ds::LASERONCONST, ds::LASERONCONST );

                if ((_fw_version >= firmware_version("5.12.1.0")) && ((_device_capabilities & ds_caps::CAP_GLOBAL_SHUTTER) == ds_caps::CAP_GLOBAL_SHUTTER))
                {
                    std::vector<std::pair<std::shared_ptr<option>, std::string>> options_and_reasons = { std::make_pair(alternating_emitter_opt,
                        "Emitter always ON cannot be set while Emitter ON/OFF is enabled") };
                    depth_sensor.register_option(RS2_OPTION_EMITTER_ALWAYS_ON,
                        std::make_shared<gated_option>(
                            emitter_always_on_opt,
                            options_and_reasons));
                }

                if (_fw_version >= hdr_firmware_version)
                {
                    std::vector<std::pair<std::shared_ptr<option>, std::string>> options_and_reasons = { std::make_pair(hdr_enabled_option, "Emitter ON/OFF cannot be set while HDR is enabled"),
                            std::make_pair(emitter_always_on_opt, "Emitter ON/OFF cannot be set while Emitter always ON is enabled") };
                    depth_sensor.register_option(RS2_OPTION_EMITTER_ON_OFF,
                        std::make_shared<gated_option>(
                            alternating_emitter_opt,
                            options_and_reasons
                            ));
                }
                else if ((_fw_version >= firmware_version("5.12.1.0")) && ((_device_capabilities & ds_caps::CAP_GLOBAL_SHUTTER) == ds_caps::CAP_GLOBAL_SHUTTER))
                {
                    std::vector<std::pair<std::shared_ptr<option>, std::string>> options_and_reasons = { std::make_pair(emitter_always_on_opt,
                        "Emitter ON/OFF cannot be set while Emitter always ON is enabled") };
                    depth_sensor.register_option(RS2_OPTION_EMITTER_ON_OFF,
                        std::make_shared<gated_option>(
                            alternating_emitter_opt,
                            options_and_reasons));
                }
                else
                {
                    depth_sensor.register_option(RS2_OPTION_EMITTER_ON_OFF, alternating_emitter_opt);
                }

            }

            if ((_device_capabilities & ds_caps::CAP_INTERCAM_HW_SYNC) == ds_caps::CAP_INTERCAM_HW_SYNC)
            {
                if (_fw_version >= firmware_version("5.12.12.100") && (_device_capabilities & ds_caps::CAP_GLOBAL_SHUTTER) == ds_caps::CAP_GLOBAL_SHUTTER)
                {
                    depth_sensor.register_option(RS2_OPTION_INTER_CAM_SYNC_MODE,
                        std::make_shared<external_sync_mode>(*_hw_monitor, raw_depth_sensor, 3));
                }
                else if (_fw_version >= firmware_version("5.12.4.0") && (_device_capabilities & ds_caps::CAP_GLOBAL_SHUTTER) == ds_caps::CAP_GLOBAL_SHUTTER)
                {
                    depth_sensor.register_option(RS2_OPTION_INTER_CAM_SYNC_MODE,
                        std::make_shared<external_sync_mode>(*_hw_monitor, raw_depth_sensor, 2));
                }
                else if (_fw_version >= firmware_version("5.9.15.1"))
                {
                    depth_sensor.register_option(RS2_OPTION_INTER_CAM_SYNC_MODE,
                        std::make_shared<external_sync_mode>(*_hw_monitor, raw_depth_sensor, 1));
                }
            }

            if (!_is_mipi_device)
            {
                depth_sensor.register_option( RS2_OPTION_STEREO_BASELINE,
                                              std::make_shared< const_value_option >(
                                                  "Distance in mm between the stereo imagers",
                                                  rsutils::lazy< float >( [this]() { return get_stereo_baseline_mm(); } ) ) );
            }

            if (advanced_mode && _fw_version >= firmware_version("5.6.3.0"))
            {
                auto depth_scale = std::make_shared<depth_scale_option>(*_hw_monitor);
                auto depth_sensor = As<d400_depth_sensor, synthetic_sensor>(&get_depth_sensor());
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
                // default depth units is different for D405
                if (_pid == RS405_PID)
                    default_depth_units = 0.0001f;  //meters
                depth_sensor.register_option(RS2_OPTION_DEPTH_UNITS, std::make_shared<const_value_option>("Number of meters represented by a single depth unit",
                        rsutils::lazy< float >( [default_depth_units]() { return default_depth_units; } ) ) );
            }
        }); //group_multiple_fw_calls

        
        // REGISTER METADATA
        if (!_is_mipi_device)
        {
            register_metadata(depth_sensor, hdr_firmware_version);
        }
        else
        {
            // used for mipi device
            register_metadata_mipi(depth_sensor, hdr_firmware_version);
        }
        //mipi

        register_info(RS2_CAMERA_INFO_NAME, device_name);
        register_info(RS2_CAMERA_INFO_SERIAL_NUMBER, optic_serial);
        register_info(RS2_CAMERA_INFO_ASIC_SERIAL_NUMBER, asic_serial);
        register_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID, asic_serial);
        register_info(RS2_CAMERA_INFO_FIRMWARE_VERSION, _fw_version);
        register_info(RS2_CAMERA_INFO_PHYSICAL_PORT, group.uvc_devices.front().device_path);
        register_info(RS2_CAMERA_INFO_DEBUG_OP_CODE, std::to_string(static_cast<int>(fw_cmd::GLD)));
        register_info(RS2_CAMERA_INFO_ADVANCED_MODE, ((advanced_mode) ? "YES" : "NO"));
        register_info(RS2_CAMERA_INFO_PRODUCT_ID, pid_hex_str);
        register_info(RS2_CAMERA_INFO_PRODUCT_LINE, "D400");
        register_info(RS2_CAMERA_INFO_RECOMMENDED_FIRMWARE_VERSION, _recommended_fw_version);
        register_info(RS2_CAMERA_INFO_CAMERA_LOCKED, _is_locked ? "YES" : "NO");
        register_info(RS2_CAMERA_INFO_DFU_DEVICE_PATH, group.uvc_devices.front().dfu_device_path);
        register_info(RS2_CAMERA_INFO_IMU_TYPE, _imu_type);

        if (usb_modality)
        {
            register_info(RS2_CAMERA_INFO_CONNECTION_TYPE, "USB");
            register_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR, usb_type_str);
        }
        else
            register_info(RS2_CAMERA_INFO_CONNECTION_TYPE, "GMSL");

        std::string curr_version= _fw_version;

        register_features();

        auto_calibrated::add_depth_write_observer( [this]()
        { 
            _coefficients_table_raw.reset();
            _new_calib_table_raw.reset();
        } );
    }

    void d400_device::register_features()
    {
        firmware_version fw_ver = firmware_version( get_info( RS2_CAMERA_INFO_FIRMWARE_VERSION ) );
        auto pid = get_pid();

        if( (_is_mipi_device || pid == ds::RS455_PID) && fw_ver >= firmware_version( 5, 14, 0, 0 ) )
            register_feature( std::make_shared< emitter_frequency_feature >( get_depth_sensor() ) );

        if( fw_ver >= firmware_version( 5, 11, 9, 0 ) )
            register_feature( std::make_shared< amplitude_factor_feature >() );

        if( fw_ver >= firmware_version( 5, 9, 10, 0 ) ) // TODO - add PID here? Now checked at advanced_mode
            register_feature( std::make_shared< remove_ir_pattern_feature >() );

        register_feature( std::make_shared< auto_exposure_roi_feature >( get_depth_sensor(), _hw_monitor ) );

        if( !_is_mipi_device && fw_ver >= firmware_version( 5, 12, 10, 11 ) )
        {
            register_feature(
                std::make_shared< auto_exposure_limit_feature >( get_depth_sensor(), d400_device::_hw_monitor ) );
            register_feature( std::make_shared< gain_limit_feature >( get_depth_sensor(), d400_device::_hw_monitor ) );
        }
    }

    void d400_device::register_metadata(const synthetic_sensor &depth_sensor, const firmware_version& hdr_firmware_version) const
    {
        depth_sensor.register_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP, make_uvc_header_parser(&platform::uvc_header::timestamp));

        // attributes of md_capture_timing
        auto md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_depth_mode, depth_y_mode) +
            offsetof(md_depth_y_normal_mode, intel_capture_timing);

        depth_sensor.register_metadata(RS2_FRAME_METADATA_FRAME_COUNTER, make_attribute_parser(&md_capture_timing::frame_counter, md_capture_timing_attributes::frame_counter_attribute, md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP, make_rs400_sensor_ts_parser(make_uvc_header_parser(&platform::uvc_header::timestamp),
            make_attribute_parser(&md_capture_timing::sensor_timestamp, md_capture_timing_attributes::sensor_timestamp_attribute, md_prop_offset)));

        // attributes of md_capture_stats
        md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_depth_mode, depth_y_mode) +
            offsetof(md_depth_y_normal_mode, intel_capture_stats);

        depth_sensor.register_metadata(RS2_FRAME_METADATA_WHITE_BALANCE, make_attribute_parser(&md_capture_stats::white_balance, md_capture_stat_attributes::white_balance_attribute, md_prop_offset));

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
        depth_sensor.register_metadata((rs2_frame_metadata_value)RS2_FRAME_METADATA_ACTUAL_FPS, std::make_shared<ds_md_attribute_actual_fps>());

        if (_fw_version >= firmware_version("5.12.7.0"))
        {
            depth_sensor.register_metadata(RS2_FRAME_METADATA_GPIO_INPUT_DATA, make_attribute_parser(&md_configuration::gpioInputData, md_configuration_attributes::gpio_input_data_attribute, md_prop_offset));
        }

        if (_fw_version >= hdr_firmware_version)
        {
            // attributes of md_capture_timing
            auto md_prop_offset = metadata_raw_mode_offset + offsetof(md_depth_mode, depth_y_mode) + offsetof(md_depth_y_normal_mode, intel_configuration);

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
        }
    }

    void d400_device::register_metadata_mipi(const synthetic_sensor &depth_sensor, const firmware_version& hdr_firmware_version) const
    {
        depth_sensor.register_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP, make_uvc_header_parser(&platform::uvc_header::timestamp));

        // frame counter
        depth_sensor.register_metadata(RS2_FRAME_METADATA_FRAME_COUNTER, make_uvc_header_parser(&platform::uvc_header_mipi::frame_counter));

        // attributes of md_mipi_depth_control structure
        auto md_prop_offset = offsetof(metadata_mipi_depth_raw, depth_mode);

        // timestamp metadata field alignment with FW was implemented on FW version 5.16.3.3
        if ( _fw_version >= firmware_version( "5.16.3.3" ) ) 
        {
            // optical_timestamp contains value of exposure/2
            depth_sensor.register_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP,
                                       make_rs400_sensor_ts_parser(make_uvc_header_parser(&platform::uvc_header::timestamp),
                                                                   make_attribute_parser(&md_mipi_depth_mode::optical_timestamp,
                                                                                         md_mipi_depth_control_attributes::optical_timestamp_attribute,
                                                                                         md_prop_offset)));

        }
        else  // keep backward compatible with previous behavior
        {  
            depth_sensor.register_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP,
                                           make_attribute_parser(&md_mipi_depth_mode::optical_timestamp,
                                                                 md_mipi_depth_control_attributes::optical_timestamp_attribute,
                                                                 md_prop_offset));
        }

        depth_sensor.register_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE,
                                       make_attribute_parser(&md_mipi_depth_mode::exposure_time,
                                                             md_mipi_depth_control_attributes::exposure_time_attribute,
                                                             md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_EXPOSURE_PRIORITY,  // instead of MANUAL_EXPOSURE - not in enum yet
                                       make_attribute_parser(&md_mipi_depth_mode::manual_exposure,
                                                             md_mipi_depth_control_attributes::manual_exposure_attribute,
                                                             md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_FRAME_LASER_POWER,
                                       make_attribute_parser(&md_mipi_depth_mode::laser_power,
                                                             md_mipi_depth_control_attributes::laser_power_attribute,
                                                             md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_TRIGGER,
                                       make_attribute_parser(&md_mipi_depth_mode::trigger,
                                                             md_mipi_depth_control_attributes::trigger_attribute,
                                                             md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_FRAME_EMITTER_MODE,
                                       make_attribute_parser(&md_mipi_depth_mode::projector_mode,
                                                             md_mipi_depth_control_attributes::projector_mode_attribute,
                                                             md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_PRESET,
                                       make_attribute_parser(&md_mipi_depth_mode::preset,
                                                             md_mipi_depth_control_attributes::preset_attribute,
                                                             md_prop_offset));

        depth_sensor.register_metadata(RS2_FRAME_METADATA_GAIN_LEVEL,
                                       make_attribute_parser(&md_mipi_depth_mode::manual_gain,
                                                             md_mipi_depth_control_attributes::manual_gain_attribute,
                                                             md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_AUTO_EXPOSURE,
                                       make_attribute_parser(&md_mipi_depth_mode::auto_exposure_mode,
                                                             md_mipi_depth_control_attributes::auto_exposure_mode_attribute,
                                                             md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_INPUT_WIDTH,
                                       make_attribute_parser(&md_mipi_depth_mode::input_width,
                                                             md_mipi_depth_control_attributes::input_width_attribute,
                                                             md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_INPUT_HEIGHT,
                                       make_attribute_parser(&md_mipi_depth_mode::input_height,
                                                             md_mipi_depth_control_attributes::input_height_attribute,
                                                             md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_SUB_PRESET_INFO,
                                       make_attribute_parser(&md_mipi_depth_mode::sub_preset_info,
                                                             md_mipi_depth_control_attributes::sub_preset_info_attribute,
                                                             md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_CALIB_INFO,
                                       make_attribute_parser(&md_mipi_depth_mode::calib_info,
                                                             md_mipi_depth_control_attributes::calibration_info_attribute,
                                                             md_prop_offset));
        depth_sensor.register_metadata(RS2_FRAME_METADATA_CRC,
                                       make_attribute_parser(&md_mipi_depth_mode::crc,
                                                             md_mipi_depth_control_attributes::crc_attribute,
                                                             md_prop_offset));

        if (_fw_version >= hdr_firmware_version)
        {
            depth_sensor.register_metadata(RS2_FRAME_METADATA_SEQUENCE_SIZE,
                make_attribute_parser(&md_mipi_depth_mode::sub_preset_info,
                    md_configuration_attributes::sub_preset_info_attribute, md_prop_offset,
                    [](const rs2_metadata_type& param) {
                        // bit mask and offset used to get data from bitfield
                        return (param & md_configuration::SUB_PRESET_BIT_MASK_SEQUENCE_SIZE)
                            >> md_configuration::SUB_PRESET_BIT_OFFSET_SEQUENCE_SIZE;
                    }));

            depth_sensor.register_metadata(RS2_FRAME_METADATA_SEQUENCE_ID,
                make_attribute_parser(&md_mipi_depth_mode::sub_preset_info,
                    md_configuration_attributes::sub_preset_info_attribute, md_prop_offset,
                    [](const rs2_metadata_type& param) {
                        // bit mask and offset used to get data from bitfield
                        return (param & md_configuration::SUB_PRESET_BIT_MASK_SEQUENCE_ID)
                            >> md_configuration::SUB_PRESET_BIT_OFFSET_SEQUENCE_ID;
                    }));

            depth_sensor.register_metadata(RS2_FRAME_METADATA_SEQUENCE_NAME,
                make_attribute_parser(&md_mipi_depth_mode::sub_preset_info,
                    md_configuration_attributes::sub_preset_info_attribute, md_prop_offset,
                    [](const rs2_metadata_type& param) {
                        // bit mask and offset used to get data from bitfield
                        return (param & md_configuration::SUB_PRESET_BIT_MASK_ID)
                            >> md_configuration::SUB_PRESET_BIT_OFFSET_ID;
                    }));
        }
    }

    // Check if need change camera name due to number modifications on one device PID.
    void update_device_name(std::string& device_name, const ds::ds_caps cap)
    {
        switch (cap)
        {
        case ds::ds_caps::CAP_IR_FILTER:
            device_name += "F"; // Adding "F" to end of device name if it has IR filter.
            break;

        case ds::ds_caps::CAP_IP65:
            device_name = std::regex_replace(device_name, std::regex("D455"), "D456"); // Change device name from D455 to D456.
            break;

        default:
            throw invalid_value_exception("capability '" + ds::ds_capabilities_names.at(cap) + "' is not supported for device name update");
            break;
        }
    }

    platform::usb_spec d400_device::get_usb_spec() const
    {
        if( supports_info( RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR ) )
        {
            auto it = platform::usb_name_to_spec.find( get_info( RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR ) );
            if( it != platform::usb_name_to_spec.end() )
                return it->second;
        }
        return platform::usb_undefined;
    }


    rs2_format d400_device::get_ir_format() const
    {
        auto const format_conversion = get_format_conversion();
        return ( format_conversion == format_conversion::raw ) ? RS2_FORMAT_Y8I : RS2_FORMAT_Y8;
    }


    double d400_device::get_device_time_ms()
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

    command d400_device::get_firmware_logs_command() const
    {
        return command{ ds::GLD, 0x1f4 };
    }

    command d400_device::get_flash_logs_command() const
    {
        return command{ ds::FRB, 0x17a000, 0x3f8 };
    }

    std::shared_ptr<synthetic_sensor> ds5u_device::create_ds5u_depth_device(std::shared_ptr<context> ctx,
        const std::vector<platform::uvc_device_info>& all_device_infos)
    {
        using namespace ds;

        std::vector<std::shared_ptr<platform::uvc_device>> depth_devices;
        for( auto & info : filter_by_mi( all_device_infos, 0 ) )  // Filter just mi=0, DEPTH
            depth_devices.push_back( get_backend()->create_uvc_device( info ) );

        std::unique_ptr< frame_timestamp_reader > d400_timestamp_reader_backup( new ds_timestamp_reader() );
        std::unique_ptr<frame_timestamp_reader> d400_timestamp_reader_metadata(new ds_timestamp_reader_from_metadata(std::move(d400_timestamp_reader_backup)));

        auto enable_global_time_option = std::shared_ptr<global_time_option>(new global_time_option());
        auto raw_depth_ep = std::make_shared<uvc_sensor>(ds::DEPTH_STEREO, std::make_shared<platform::multi_pins_uvc_device>(depth_devices), std::unique_ptr<frame_timestamp_reader>(new global_timestamp_reader(std::move(d400_timestamp_reader_metadata), _tf_keeper, enable_global_time_option)), this);
        auto depth_ep = std::make_shared<ds5u_depth_sensor>(this, raw_depth_ep);

        depth_ep->register_option(RS2_OPTION_GLOBAL_TIME_ENABLED, enable_global_time_option);

        raw_depth_ep->register_xu(depth_xu); // make sure the XU is initialized every time we power the camera

        depth_ep->register_processing_block({ {RS2_FORMAT_W10} }, { {RS2_FORMAT_RAW10, RS2_STREAM_INFRARED, 1} }, []() { return std::make_shared<w10_converter>(RS2_FORMAT_RAW10); });
        depth_ep->register_processing_block({ {RS2_FORMAT_W10} }, { {RS2_FORMAT_Y10BPACK, RS2_STREAM_INFRARED, 1} }, []() { return std::make_shared<w10_converter>(RS2_FORMAT_Y10BPACK); });

        depth_ep->register_processing_block(processing_block_factory::create_pbf_vector<uyvy_converter>(RS2_FORMAT_UYVY, map_supported_color_formats(RS2_FORMAT_UYVY), RS2_STREAM_INFRARED));


        return depth_ep;
    }

    ds5u_device::ds5u_device( std::shared_ptr< const d400_info > const & dev_info )
        : d400_device(dev_info), device(dev_info)
    {
        using namespace ds;

        // Override the basic d400 sensor with the development version
        _depth_device_idx = assign_sensor(create_ds5u_depth_device( dev_info->get_context(), dev_info->get_group().uvc_devices), _depth_device_idx);

        init( dev_info->get_context(), dev_info->get_group() );

        auto& depth_ep = get_depth_sensor();

        // Inhibit specific unresolved options
        depth_ep.unregister_option(RS2_OPTION_OUTPUT_TRIGGER_ENABLED);
        depth_ep.unregister_option(RS2_OPTION_ERROR_POLLING_ENABLED);
        depth_ep.unregister_option(RS2_OPTION_ASIC_TEMPERATURE);
        depth_ep.unregister_option(RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE);

        // Enable laser etc.
        auto pid = dev_info->get_group().uvc_devices.front().pid;
        if (pid != RS_USB2_PID)
        {
            auto depth_ep = get_raw_depth_sensor();
            auto emitter_enabled = std::make_shared<emitter_option>(depth_ep);
            depth_ep->register_option(RS2_OPTION_EMITTER_ENABLED, emitter_enabled);

            auto laser_power = std::make_shared<uvc_xu_option<uint16_t>>(depth_ep,
                depth_xu,
                DS5_LASER_POWER,
                "Manual laser power in mw. applicable only when laser power mode is set to Manual" );
            depth_ep->register_option(RS2_OPTION_LASER_POWER,
                                       std::make_shared< auto_disabling_control >( laser_power,
                                                                                   emitter_enabled,
                                                                                   std::vector< float >{ 0.f, 2.f },
                                                                                   1.f ) );

            depth_ep->register_option(RS2_OPTION_PROJECTOR_TEMPERATURE,
                std::make_shared< asic_and_projector_temperature_options >( depth_ep,
                                                                            RS2_OPTION_PROJECTOR_TEMPERATURE ) );
        }
    }

    

    void d400_device::set_imu_type()
    {
        using namespace ds;

        if( ( _device_capabilities & ds_caps::CAP_BMI_055 ) == ds_caps::CAP_BMI_055 )
            _imu_type = "BMI055";
        else if( ( _device_capabilities & ds_caps::CAP_BMI_085 ) == ds_caps::CAP_BMI_085 )
            _imu_type = "BMI085";
        else if( ( _device_capabilities & ds_caps::CAP_BMI_088 ) == ds_caps::CAP_BMI_088 )
            _imu_type = "BMI088";
        else
            _imu_type = "IMU_Unknown";
    }

    }
