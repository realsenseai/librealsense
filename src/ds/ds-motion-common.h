// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#pragma once

#include "ds-device-common.h"
#include <rsutils/string/from.h>
#include "ds-calib-parsers.h"

#include <rsutils/lazy.h>


namespace librealsense
{
    // Enforce complile-time verification of all the assigned FPS profiles
    enum class IMU_OUTPUT_DATA_RATES : uint16_t
    {
        IMU_FPS_63 = 63,
        IMU_FPS_100 = 100,
        IMU_FPS_200 = 200,
        IMU_FPS_250 = 250,
        IMU_FPS_400 = 400
    };

    using odr = IMU_OUTPUT_DATA_RATES;

#ifdef _WIN32
    static const std::string gyro_sensor_name = "HID Sensor Class Device: Gyroscope";
    static const std::string accel_sensor_name = "HID Sensor Class Device: Accelerometer";
    static const std::map<odr, uint16_t> hid_fps_translation =
    {  //FPS   Value to send to the Driver
        {odr::IMU_FPS_63,   1000},
        {odr::IMU_FPS_100,  1000},
        {odr::IMU_FPS_200,  500},
        {odr::IMU_FPS_250,  400},
        {odr::IMU_FPS_400,  250} };

#else
    static const std::string gyro_sensor_name = "gyro_3d";
    static const std::string accel_sensor_name = "accel_3d";
    static const std::map<IMU_OUTPUT_DATA_RATES, unsigned> hid_fps_translation =
    {  //FPS   Value to send to the Driver
        {odr::IMU_FPS_63,   1},
        {odr::IMU_FPS_100,  1},
        {odr::IMU_FPS_200,  2},
        {odr::IMU_FPS_250,  3},
        {odr::IMU_FPS_400,  4} };
#endif

    class auto_exposure_mechanism;
    class fisheye_auto_exposure_roi_method : public region_of_interest_method
    {
    public:
        fisheye_auto_exposure_roi_method(std::shared_ptr<auto_exposure_mechanism> auto_exposure)
            : _auto_exposure(auto_exposure)
        {}
        void set(const region_of_interest& roi) override;
        region_of_interest get() const override;

    private:
        std::shared_ptr<auto_exposure_mechanism> _auto_exposure;
        region_of_interest _roi{};
    };

    class ds_fisheye_sensor : public synthetic_sensor, public video_sensor_interface, public roi_sensor_base, public fisheye_sensor
    {
    public:
        explicit ds_fisheye_sensor( std::shared_ptr< raw_sensor_base > const & sensor, device * owner );

        rs2_intrinsics get_intrinsics(const stream_profile& profile) const override;
        stream_profiles init_stream_profiles() override;
        std::shared_ptr<uvc_sensor> get_raw_sensor();

    private:
        const std::vector<uint8_t>& get_fisheye_calibration_table() const;
        std::shared_ptr<stream_interface> get_fisheye_stream() const;

        device* _owner;
    };

    class ds_motion_sensor : public synthetic_sensor,
                          public motion_sensor
    {
    public:
        explicit ds_motion_sensor( std::string const & name,
                                   std::shared_ptr< raw_sensor_base > const & sensor,
                                   device * owner );

        explicit ds_motion_sensor( std::string const & name,
                                   std::shared_ptr< raw_sensor_base > const & sensor,
                                   device * owner,
                                   const std::map< uint32_t, rs2_format > & motion_fourcc_to_rs2_format,
                                   const std::map< uint32_t, rs2_stream > & motion_fourcc_to_rs2_stream );

        rs2_motion_device_intrinsic get_motion_intrinsics(rs2_stream stream) const;

        stream_profiles init_stream_profiles() override;

    private:
        std::shared_ptr<stream_interface> get_accel_stream() const;
        std::shared_ptr<stream_interface> get_gyro_stream() const;

        const device* _owner;
    };

    class global_time_option;
    class time_diff_keeper;
    namespace platform {
        struct uvc_device_info;
        struct hid_device_info;
        struct backend_device_group;
    }

    class context;
    class backend_device;

    class ds_motion_common
    {
    public:
        ds_motion_common( backend_device * owner,
                          firmware_version fw_version,
                          const ds::ds_caps& device_capabilities,
                          std::shared_ptr<hw_monitor> hwm);

        rs2_motion_device_intrinsic get_motion_intrinsics(rs2_stream) const;

        std::vector<platform::uvc_device_info> init_fisheye(const platform::backend_device_group& group,
            bool& is_fisheye_available);

        void assign_fisheye_ep(std::shared_ptr<uvc_sensor> raw_fisheye_ep,
            std::shared_ptr<synthetic_sensor> fisheye_ep,
            std::shared_ptr<global_time_option> enable_ae_option);

        void register_fisheye_options();
        void register_fisheye_metadata();

        std::shared_ptr<synthetic_sensor> create_hid_device(std::shared_ptr<context> ctx,
                                                            const std::vector<platform::hid_device_info>& all_hid_infos,
                                                            std::shared_ptr<time_diff_keeper> tf_keeper);

        void init_motion(bool is_infos_empty, const stream_interface& depth_stream);

        const std::vector<uint8_t>& get_fisheye_calibration_table() const;

        // Bandwidth parameters required for HID sensors
        // The Acceleration configuration will be resolved according to the IMU sensor type at run-time
        std::vector<std::pair<std::string, stream_profile>> _sensor_name_and_hid_profiles;

        // Translate frequency to SENSOR_PROPERTY_CURRENT_REPORT_INTERVAL.
        std::map<rs2_stream, std::map<unsigned, unsigned>> _fps_and_sampling_frequency_per_rs2_stream;

        inline std::shared_ptr<stream_interface> get_fisheye_stream() const { return _fisheye_stream; }
        inline std::shared_ptr<stream_interface> get_accel_stream() const { return _accel_stream; }
        inline std::shared_ptr<stream_interface> get_gyro_stream() const { return _gyro_stream; }

        inline std::shared_ptr<mm_calib_handler> get_calib_handler() const {return _mm_calib;}

    private:
        std::shared_ptr<auto_exposure_mechanism> register_auto_exposure_options(synthetic_sensor* ep,
            const platform::extension_unit* fisheye_xu);
        void set_roi_method();
        void register_streams_to_extrinsic_groups();
        std::vector<platform::uvc_device_info> filter_device_by_capability(const std::vector<platform::uvc_device_info>& devices,
            ds::ds_caps caps);

        friend class ds_motion_sensor;
        friend class ds_fisheye_sensor;

        backend_device * _owner;
        firmware_version _fw_version;
        ds::ds_caps _device_capabilities;
        std::shared_ptr<hw_monitor> _hw_monitor;

        std::shared_ptr<mm_calib_handler> _mm_calib;
        rsutils::lazy< std::vector< uint8_t > > _fisheye_calibration_table_raw;

        std::shared_ptr<stream_interface> _fisheye_stream;
        std::shared_ptr<stream_interface> _accel_stream;
        std::shared_ptr<stream_interface> _gyro_stream;

        std::shared_ptr< rsutils::lazy< ds::imu_intrinsic > > _accel_intrinsic;
        std::shared_ptr< rsutils::lazy< ds::imu_intrinsic > > _gyro_intrinsic;
        std::shared_ptr< rsutils::lazy< rs2_extrinsics > > _depth_to_imu;  // Mechanical installation pose

        std::shared_ptr<uvc_sensor> _raw_fisheye_ep;
        std::shared_ptr<synthetic_sensor> _fisheye_ep;
        std::shared_ptr<global_time_option> _enable_global_time_option;
    };
}