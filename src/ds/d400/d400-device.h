// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d400-private.h"

#include "algo.h"
#include "error-handling.h"
#include "core/debug.h"
#include "core/advanced_mode.h"
#include "device.h"
#include "global_timestamp_reader.h"
#include "fw-update/fw-update-device-interface.h"
#include "d400-auto-calibration.h"
#include "d400-options.h"
#include <src/core/video.h>
#include <src/depth-sensor.h>

#include "ds/ds-device-common.h"
#include "backend-device.h"

namespace librealsense
{
    class d400_device;

    class d400_depth_sensor
        : public synthetic_sensor
        , public video_sensor_interface
        , public depth_stereo_sensor
        , public roi_sensor_base
    {
    public:
        explicit d400_depth_sensor( d400_device * owner, std::shared_ptr< uvc_sensor > uvc_sensor );
        processing_blocks get_recommended_processing_blocks() const override;

        rs2_intrinsics get_intrinsics( const stream_profile & profile ) const override;
        void set_frame_metadata_modifier( on_frame_md callback ) override;
        void open( const stream_profiles & requests ) override;
        void close() override;
        rs2_intrinsics get_color_intrinsics( const stream_profile & profile ) const;
        stream_profiles init_stream_profiles() override;
        float get_depth_scale() const override;
        void set_depth_scale( float val );
        void init_hdr_config( const option_range & exposure_range, const option_range & gain_range );

        std::shared_ptr< hdr_config > get_hdr_config() { return _hdr_cfg; }
        float get_stereo_baseline_mm() const override;

        float get_preset_max_value() const override;

    protected:
        const d400_device * _owner;
        mutable std::atomic< float > _depth_units;
        float _stereo_baseline_mm;
        std::shared_ptr< hdr_config > _hdr_cfg;
    };

    class hdr_config;
    class d400_thermal_monitor;

    class d400_device
        : public virtual backend_device
        , public debug_interface
        , public global_time_interface
        , public updatable
        , public auto_calibrated
    {
    public:
        std::shared_ptr<synthetic_sensor> create_depth_device(std::shared_ptr<context> ctx,
                                                        const std::vector<platform::uvc_device_info>& all_device_infos);

        synthetic_sensor& get_depth_sensor()
        {
            return dynamic_cast<synthetic_sensor&>(get_sensor(_depth_device_idx));
        }

        std::shared_ptr< uvc_sensor > get_raw_depth_sensor()
        {
            synthetic_sensor& depth_sensor = get_depth_sensor();
            return std::dynamic_pointer_cast< uvc_sensor >( depth_sensor.get_raw_sensor() );
        }

        d400_device( std::shared_ptr< const d400_info > const & );

        std::vector<uint8_t> send_receive_raw_data(const std::vector<uint8_t>& input) override;

        std::vector<uint8_t> build_command(uint32_t opcode,
            uint32_t param1 = 0,
            uint32_t param2 = 0,
            uint32_t param3 = 0,
            uint32_t param4 = 0,
            uint8_t const * data = nullptr,
            size_t dataLength = 0) const override;

        void hardware_reset() override;

        platform::usb_spec get_usb_spec() const;
        virtual double get_device_time_ms() override;

        void enter_update_state() const override;
        std::vector<uint8_t> backup_flash( rs2_update_progress_callback_sptr callback) override;
        void update_flash(const std::vector<uint8_t>& image, rs2_update_progress_callback_sptr callback, int update_mode) override;
        bool check_fw_compatibility(const std::vector<uint8_t>& image) const override;
        std::string get_opcode_string(int opcode) const override;

    protected:
        std::shared_ptr<ds_device_common> _ds_device_common;

        std::vector<uint8_t> get_d400_raw_calibration_table(ds::d400_calibration_table_id table_id) const;
        std::vector<uint8_t> get_new_calibration_table() const;

        bool is_camera_in_advanced_mode() const;

        float get_stereo_baseline_mm() const;

        ds::ds_caps parse_device_capabilities( const std::vector<uint8_t> &gvd_buf ) const;

        rs2_format get_ir_format() const;

        //TODO - add these to device class as pure virtual methods
        command get_firmware_logs_command() const;
        command get_flash_logs_command() const;

        void register_metadata(const synthetic_sensor& depth_sensor, const firmware_version& hdr_firmware_version) const;
        void register_metadata_mipi(const synthetic_sensor& depth_sensor, const firmware_version& hdr_firmware_version) const;

        void init(std::shared_ptr<context> ctx, const platform::backend_device_group& group);
        void register_features();
        void set_imu_type();
        void get_fw_details( const std::vector< uint8_t > & gvd_buff, std::string & optic_serial,
                             std::string & asic_serial, std::string & fwv ) const;
        bool is_d401_usb_device( uint8_t gvd_hw_type ) const;

        friend class d400_depth_sensor;

        std::shared_ptr<hw_monitor> _hw_monitor;
        firmware_version            _fw_version;
        firmware_version            _recommended_fw_version;
        ds::ds_caps               _device_capabilities;

        std::shared_ptr<stream_interface> _depth_stream;
        std::shared_ptr<stream_interface> _left_ir_stream;
        std::shared_ptr<stream_interface> _right_ir_stream;
        std::shared_ptr<stream_interface> _color_stream;

        uint8_t _depth_device_idx;

        rsutils::lazy< std::vector< uint8_t > > _coefficients_table_raw;
        rsutils::lazy< std::vector< uint8_t > > _new_calib_table_raw;

        std::shared_ptr<polling_error_handler> _polling_error_handler;
        std::shared_ptr<ds_thermal_monitor> _thermal_monitor;
        std::shared_ptr< rsutils::lazy< rs2_extrinsics > > _left_right_extrinsics;
        rsutils::lazy< std::vector< uint8_t > > _color_calib_table_raw;
        std::shared_ptr< rsutils::lazy< rs2_extrinsics > > _color_extrinsic;
        bool _is_locked = true;

        std::shared_ptr<auto_gain_limit_option> _gain_limit_value_control;
        std::shared_ptr<auto_exposure_limit_option> _ae_limit_value_control;
        bool _is_mipi_device;
        std::string _imu_type;
    };

    class ds5u_device : public d400_device
    {
    public:
        ds5u_device( std::shared_ptr< const d400_info > const & );

        std::shared_ptr<synthetic_sensor> create_ds5u_depth_device(std::shared_ptr<context> ctx,
            const std::vector<platform::uvc_device_info>& all_device_infos);

    protected:
        friend class ds5u_depth_sensor;
    };

    // Update device name according capability in it.
    void update_device_name(std::string& device_name, const ds::ds_caps cap);
    void update_d405_device_name( std::string & device_name );

 }
