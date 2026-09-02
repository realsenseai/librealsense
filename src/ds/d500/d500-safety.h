// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022-2024 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d500-device.h"
#include "safety-sensor.h"
#include "core/video.h"

namespace librealsense
{
    class d500_safety_sensor;
    class ds_advanced_mode_base;

    std::string read_safety_interface_config( std::shared_ptr< hw_monitor_extended_buffers > const & hw_monitor,
                                               rs2_calib_location loc = RS2_CALIB_LOCATION_RAM );

    class d500_safety : public virtual d500_device
    {
    public:
        std::shared_ptr<synthetic_sensor> create_safety_device(std::shared_ptr<context> ctx,
            const std::vector<platform::uvc_device_info>& safety_devices_info);

        d500_safety( std::shared_ptr< const d500_info > const & dev_info );

        void set_advanced_mode_device( ds_advanced_mode_base * advanced_mode );
        void gate_depth_options();

    private:

        friend class d500_safety_sensor;

        void register_options(std::shared_ptr<d500_safety_sensor> safety_ep, std::shared_ptr<uvc_sensor> raw_safety_sensor);
        void register_metadata(std::shared_ptr<uvc_sensor> safety_ep);
        void register_processing_blocks(std::shared_ptr<d500_safety_sensor> safety_ep);

        void block_advanced_mode_if_needed( float val );
        void gate_depth_option( rs2_option opt,
                                synthetic_sensor & depth_sensor,
                                const std::vector< std::tuple< std::shared_ptr< option >, float, std::string > > & options_and_reasons );

    protected:
        std::shared_ptr<stream_interface> _safety_stream;
        uint8_t _safety_device_idx;
        ds_advanced_mode_base * _advanced_mode;
        std::shared_ptr< option > _safety_camera_oper_mode;
    };

    class d500_safety_sensor : public synthetic_sensor,
                              public video_sensor_interface,
                              public safety_sensor
    {
    public:
        explicit d500_safety_sensor(d500_safety* owner,
            std::shared_ptr<uvc_sensor> uvc_sensor,
            std::map<uint32_t, rs2_format> safety_fourcc_to_rs2_format,
            std::map<uint32_t, rs2_stream> safety_fourcc_to_rs2_stream)
            : synthetic_sensor("Safety Camera", uvc_sensor, owner, safety_fourcc_to_rs2_format, safety_fourcc_to_rs2_stream),
            _owner(owner)
        {}

        rs2_intrinsics get_intrinsics(const stream_profile& profile) const override;
        stream_profiles init_stream_profiles() override;
        std::string get_safety_preset(int index) const override;
        void set_safety_preset(int index, const std::string& sp_json_str) const override;
        std::string get_safety_interface_config(rs2_calib_location loc = RS2_CALIB_LOCATION_RAM) const override;
        void set_safety_interface_config(const std::string& sic_json_str) const override;
        std::string get_application_config() const override;
        void set_application_config(const std::string& application_config_json_str) const override;

    protected:
        const d500_safety* _owner;
    };

    // subdevice[h] unit[fw], node[h] guid[fw]
    const platform::extension_unit safety_xu = { 4, 0xC, 2,
    { 0xf6c3c3d1, 0x5cde, 0x4477,{ 0xad, 0xf0, 0x41, 0x33, 0xf5, 0x8d, 0xa6, 0xf4 } } };

    namespace xu_id
    {
        const uint8_t SAFETY_CAMERA_OPER_MODE = 0x1;
        const uint8_t SAFETY_PRESET_ACTIVE_INDEX = 0x2;
    }
}
