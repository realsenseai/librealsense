// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d500-private.h"
#include "hw_monitor_extended_buffers.h"

#include "core/debug.h"
#include "global_timestamp_reader.h"
#include "fw-update/fw-update-device-interface.h"

#include "ds/ds-device-common.h"
#include "backend-device.h"
#include "d500-auto-calibration.h"
#include <src/core/video.h>
#include <src/depth-sensor.h>

#include <rsutils/lazy.h>


namespace librealsense
{
    class d500_device;

    class d500_depth_sensor
        : public synthetic_sensor
        , public video_sensor_interface
        , public depth_stereo_sensor
        , public roi_sensor_base
    {
    public:
        explicit d500_depth_sensor( d500_device * owner, std::shared_ptr< uvc_sensor > uvc_sensor );

        processing_blocks get_recommended_processing_blocks() const override;
        rs2_intrinsics get_intrinsics( const stream_profile & profile ) const override;
        void set_frame_metadata_modifier( on_frame_md callback ) override;
        void open( const stream_profiles & requests ) override;
        void close() override;
        rs2_intrinsics get_color_intrinsics( const stream_profile & profile ) const;
        stream_profiles init_stream_profiles() override;
        float get_depth_scale() const override;
        void set_depth_scale( float val );
        float get_stereo_baseline_mm() const override;
        float get_preset_max_value() const override;

    protected:
        const d500_device * _owner;
        mutable std::atomic< float > _depth_units;
        float _stereo_baseline_mm;
    };

    class ds_thermal_monitor;
    class ds_devices_common;
    class d500_info;

    namespace platform {
        struct backend_device_group;
    }

    class d500_device
        : public virtual backend_device
        , public debug_interface
        , public global_time_interface
        , public d500_auto_calibrated
        , public updatable
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
            synthetic_sensor & depth_sensor = get_depth_sensor();
            return std::dynamic_pointer_cast< uvc_sensor >( depth_sensor.get_raw_sensor() );
        }

        d500_device( std::shared_ptr< const d500_info > const & );

        std::shared_ptr<ds::d500_hwmon_response> _hw_monitor_response;

        std::vector<uint8_t> send_receive_raw_data(const std::vector<uint8_t>& input) override;

        std::vector<uint8_t> build_command(uint32_t opcode,
            uint32_t param1 = 0,
            uint32_t param2 = 0,
            uint32_t param3 = 0,
            uint32_t param4 = 0,
            uint8_t const* data = nullptr,
            size_t dataLength = 0) const override;

        void hardware_reset() override;

        platform::usb_spec get_usb_spec() const;
        virtual double get_device_time_ms() override;

        void enter_update_state() const override;
        std::vector<uint8_t> backup_flash( rs2_update_progress_callback_sptr callback ) override;
        void update_flash(const std::vector<uint8_t>& image, rs2_update_progress_callback_sptr callback, int update_mode) override;
        bool check_fw_compatibility( const std::vector<uint8_t>& image ) const override { return true; };
        std::string get_opcode_string(int opcode) const override;
    protected:
        std::shared_ptr<ds_device_common> _ds_device_common;

        std::vector<uint8_t> get_d500_raw_calibration_table(ds::d500_calibration_table_id table_id) const;
        std::vector<uint8_t> get_new_calibration_table() const;

        bool is_camera_in_advanced_mode() const;

        float get_stereo_baseline_mm() const;

        ds::ds_caps parse_device_capabilities( const std::vector<uint8_t>& gvd_buf ) const;
        void get_gvd_details(const std::vector<uint8_t>& gvd_buff, ds::d500_gvd_parsed_fields* parsed_fields) const;

        bool check_symmetrization_enabled() const;

        command get_firmware_logs_command() const;

        void init(std::shared_ptr<context> ctx, const platform::backend_device_group& group);
        void register_features();
        void set_imu_type( const std::vector< uint8_t > & gvd_buf, ds::d500_gvd_parsed_fields * parsed_fields );
        friend class d500_depth_sensor;

        std::shared_ptr<hw_monitor_extended_buffers> _hw_monitor;
        firmware_version _fw_version;
        firmware_version _recommended_fw_version;
        ds::ds_caps _device_capabilities;

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
        bool _is_symmetrization_enabled = true;
    };
}
