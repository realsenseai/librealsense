// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include "frame-object.h"

#include <cstdint>  // uintX_t
#include <string>
#include <vector>
#include <functional>
#include <memory>


namespace librealsense {
namespace platform {


struct hid_profile
{
    std::string sensor_name;
    uint32_t frequency;
    double sensitivity;
};


struct hid_sensor
{
    std::string name;
};

struct hid_sensor_input
{
    uint32_t index;
    std::string name;
};

struct sensor_data
{
    hid_sensor sensor;
    frame_object fo;
};

#pragma pack( push, 1 )
struct hid_sensor_data
{
    int32_t x;
    int32_t y;
    int32_t z;
    uint32_t ts_low;
    uint32_t ts_high;
};
#pragma pack( pop )


typedef std::function< void( const sensor_data & ) > hid_callback;


enum custom_sensor_report_field
{
    minimum,
    maximum,
    name,
    size,
    unit_expo,
    units,
    value
};


class hid_device
{
public:
    virtual ~hid_device() = default;
    virtual void register_profiles( const std::vector< hid_profile > & hid_profiles )
        = 0;  // TODO: this should be queried from the device
    virtual void open( const std::vector< hid_profile > & hid_profiles ) = 0;
    virtual void close() = 0;
    virtual void stop_capture() = 0;
    virtual void start_capture( hid_callback callback ) = 0;
    virtual std::vector< hid_sensor > get_sensors() = 0;
    virtual std::vector< uint8_t > get_custom_report_data( const std::string & custom_sensor_name,
                                                           const std::string & report_name,
                                                           custom_sensor_report_field report_field )
        = 0;
    virtual void set_gyro_scale_factor( double scale_factor ) = 0;
    virtual void set_accel_scale_factor( double scale_factor ) = 0;
};


class multi_pins_hid_device : public hid_device
{
public:
    void register_profiles( const std::vector< hid_profile > & hid_profiles ) override { _hid_profiles = hid_profiles; }
    void open( const std::vector< hid_profile > & sensor_iio ) override
    {
        for( auto && dev : _dev )
            dev->open( sensor_iio );
    }

    void close() override
    {
        for( auto && dev : _dev )
            dev->close();
    }

    void stop_capture() override { _dev.front()->stop_capture(); }

    void start_capture( hid_callback callback ) override { _dev.front()->start_capture( callback ); }

    std::vector< hid_sensor > get_sensors() override { return _dev.front()->get_sensors(); }

    explicit multi_pins_hid_device( const std::vector< std::shared_ptr< hid_device > > & dev )
        : _dev( dev )
    {
    }

    std::vector< uint8_t > get_custom_report_data( const std::string & custom_sensor_name,
                                                   const std::string & report_name,
                                                   custom_sensor_report_field report_field ) override
    {
        return _dev.front()->get_custom_report_data( custom_sensor_name, report_name, report_field );
    }

    void set_gyro_scale_factor( double scale_factor ) override {};
    void set_accel_scale_factor( double scale_factor ) override {};

private:
    std::vector< std::shared_ptr< hid_device > > _dev;
    std::vector< hid_profile > _hid_profiles;
};


}  // namespace platform
}  // namespace librealsense
