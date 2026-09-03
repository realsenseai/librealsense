// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include "sensor.h"
#include "platform/hid-device.h"


namespace librealsense {


class iio_hid_timestamp_reader : public frame_timestamp_reader
{
    static const int sensors = 2;
    bool started;
    mutable std::vector< int64_t > counter;
    mutable std::recursive_mutex _mtx;

public:
    iio_hid_timestamp_reader();

    void reset() override;

    rs2_time_t get_frame_timestamp( const std::shared_ptr< frame_interface > & frame ) override;

    bool has_metadata( const std::shared_ptr< frame_interface > & frame ) const;

    unsigned long long get_frame_counter( const std::shared_ptr< frame_interface > & frame ) const override;

    rs2_timestamp_domain get_frame_timestamp_domain( const std::shared_ptr< frame_interface > & frame ) const override;
};


class hid_sensor : public raw_sensor_base
{
    typedef raw_sensor_base super; 

public:
    explicit hid_sensor(
        std::shared_ptr< platform::hid_device > hid_device,
        std::unique_ptr< frame_timestamp_reader > hid_iio_timestamp_reader,
        std::unique_ptr< frame_timestamp_reader > custom_hid_timestamp_reader,
        const std::map< rs2_stream, std::map< unsigned, unsigned > > & fps_and_sampling_frequency_per_rs2_stream,
        const std::vector< std::pair< std::string, stream_profile > > & sensor_name_and_hid_profiles,
        device * dev );

    ~hid_sensor() override;

    void open( const stream_profiles & requests ) override;
    void close() override;
    void start( rs2_frame_callback_sptr callback ) override;
    void stop() override;
    void set_imu_sensitivity( rs2_stream stream, float value );
    double get_imu_sensitivity_values( rs2_stream stream );
    // HKR FW accepts the option value directly as a range index:
    // 0/1/2/3/4 select 2000/1000/500/250/125 dps respectively.
    // Other devices retain the D400 conversion to FW tokens 0/0.1/0.2/0.3/0.4.
    void enable_gyro_sensitivity_range_index() { _gyro_sensitivity_uses_range_index = true; }
    void set_gyro_scale_factor(double scale_factor);
    void set_accel_scale_factor(double scale_factor);
    std::vector< uint8_t > get_custom_report_data( const std::string & custom_sensor_name,
                                                   const std::string & report_name,
                                                   platform::custom_sensor_report_field report_field ) const;

protected:
    stream_profiles init_stream_profiles() override;

private:
    const std::vector< std::pair< std::string, stream_profile > > _sensor_name_and_hid_profiles;
    std::map< rs2_stream, std::map< uint32_t, uint32_t > > _fps_and_sampling_frequency_per_rs2_stream;
    std::shared_ptr< platform::hid_device > _hid_device;
    std::mutex _configure_lock;
    std::map< std::string, std::shared_ptr< stream_profile_interface > > _configured_profiles;
    std::vector< bool > _is_configured_stream;
    std::vector< platform::hid_sensor > _hid_sensors;
    std::unique_ptr< frame_timestamp_reader > _hid_iio_timestamp_reader;
    std::unique_ptr< frame_timestamp_reader > _custom_hid_timestamp_reader;
    //Keeps set sensitivity values for gyro and accel
    std::map< rs2_stream, float > _imu_sensitivity_per_rs2_stream;
    bool _gyro_sensitivity_uses_range_index = false;

    stream_profiles get_sensor_profiles( std::string sensor_name ) const;

    const std::string & rs2_stream_to_sensor_name( rs2_stream stream ) const;

    uint32_t stream_to_fourcc( rs2_stream stream ) const;

    uint32_t fps_to_sampling_frequency( rs2_stream stream, uint32_t fps ) const;
};


}  // namespace librealsense
