// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include "../usb/usb-types.h"
#include "stream-profile.h"
#include "frame-object.h"

#include <librealsense2/h/rs_option.h>  // rs2_option

#include <cstdint>  // uintX_t
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <set>
#include <chrono>
#include <thread>
#include <algorithm>  // find


const uint8_t  DEFAULT_V4L2_FRAME_BUFFERS = 4;
const uint16_t MAX_RETRIES = 100;
const uint16_t DELAY_FOR_RETRIES = 10;


namespace librealsense {


struct notification;


namespace platform {


struct guid
{
    uint32_t data1;
    uint16_t data2, data3;
    uint8_t data4[8];
};

std::ostream & operator<<( std::ostream &, guid const & );

// subdevice and node fields are assigned by Host driver; unit and GUID are hard-coded in camera firmware
struct extension_unit
{
    int subdevice;
    uint8_t unit;
    int node;
    guid id;
};

enum power_state
{
    D0,
    D3
};

#pragma pack( push, 1 )
struct uvc_header
{
    uint8_t length;  // UVC Metadata total length is
                     // limited by design to 255 bytes
    uint8_t info;
    uint32_t timestamp;
    uint8_t source_clock[6];
};

struct uvc_header_mipi
{
    uvc_header header;
    uint32_t frame_counter;
};
#pragma pack( pop )

constexpr uint8_t uvc_header_size = sizeof( uvc_header );
constexpr uint8_t uvc_header_mipi_size = sizeof( uvc_header_mipi );

typedef std::function< void( stream_profile, frame_object, std::function< void() > ) > frame_callback;


struct control_range
{
    control_range() {}

    control_range( int32_t in_min, int32_t in_max, int32_t in_step, int32_t in_def )
    {
        populate_raw_data( min, in_min );
        populate_raw_data( max, in_max );
        populate_raw_data( step, in_step );
        populate_raw_data( def, in_def );
    }
    control_range( std::vector< uint8_t > in_min,
                   std::vector< uint8_t > in_max,
                   std::vector< uint8_t > in_step,
                   std::vector< uint8_t > in_def )
    {
        min = in_min;
        max = in_max;
        step = in_step;
        def = in_def;
    }
    std::vector< uint8_t > min;
    std::vector< uint8_t > max;
    std::vector< uint8_t > step;
    std::vector< uint8_t > def;

private:
    void populate_raw_data( std::vector< uint8_t > & vec, int32_t value );
};


class uvc_device
{
public:
    virtual void
    probe_and_commit( stream_profile profile, frame_callback callback, int buffers = DEFAULT_V4L2_FRAME_BUFFERS )
        = 0;
    virtual void stream_on( std::function< void( const notification & n ) > error_handler
                            = []( const notification & n ) {} )
        = 0;
    virtual void start_callbacks() = 0;
    virtual void stop_callbacks() = 0;
    virtual void close( stream_profile profile ) = 0;

    virtual void set_power_state( power_state state ) = 0;
    virtual power_state get_power_state() const = 0;

    virtual void init_xu( const extension_unit & xu ) = 0;
    virtual bool set_xu( const extension_unit & xu, uint8_t ctrl, const uint8_t * data, int len ) = 0;
    virtual bool get_xu( const extension_unit & xu, uint8_t ctrl, uint8_t * data, int len ) const = 0;
    virtual control_range get_xu_range( const extension_unit & xu, uint8_t ctrl, int len ) const = 0;

    virtual bool get_pu( rs2_option opt, int32_t & value ) const = 0;
    virtual bool set_pu( rs2_option opt, int32_t value ) = 0;
    virtual control_range get_pu_range( rs2_option opt ) const = 0;

    virtual std::vector< stream_profile > get_profiles() const = 0;

    virtual void lock() const = 0;
    virtual void unlock() const = 0;

    virtual std::string get_device_location() const = 0;
    virtual usb_spec get_usb_specification() const = 0;

    virtual bool is_platform_jetson() const = 0;

    virtual ~uvc_device() = default;

protected:
    std::function< void( const notification & n ) > _error_handler;
};


class retry_controls_work_around : public uvc_device
{
public:
    explicit retry_controls_work_around( std::shared_ptr< uvc_device > dev )
        : _dev( dev )
    {
    }

    void probe_and_commit( stream_profile profile, frame_callback callback, int buffers ) override
    {
        _dev->probe_and_commit( profile, callback, buffers );
    }

    void stream_on( std::function< void( const notification & n ) > error_handler
                    = []( const notification & n ) {} ) override
    {
        _dev->stream_on( error_handler );
    }

    void start_callbacks() override { _dev->start_callbacks(); }

    void stop_callbacks() override { _dev->stop_callbacks(); }

    void close( stream_profile profile ) override { _dev->close( profile ); }

    void set_power_state( power_state state ) override { _dev->set_power_state( state ); }

    power_state get_power_state() const override { return _dev->get_power_state(); }

    void init_xu( const extension_unit & xu ) override { _dev->init_xu( xu ); }

    bool set_xu( const extension_unit & xu, uint8_t ctrl, const uint8_t * data, int len ) override
    {
        for( auto i = 0; i < MAX_RETRIES; ++i )
        {
            if( _dev->set_xu( xu, ctrl, data, len ) )
                return true;

            std::this_thread::sleep_for( std::chrono::milliseconds( DELAY_FOR_RETRIES ) );
        }
        return false;
    }

    bool get_xu( const extension_unit & xu, uint8_t ctrl, uint8_t * data, int len ) const override
    {
        for( auto i = 0; i < MAX_RETRIES; ++i )
        {
            if( _dev->get_xu( xu, ctrl, data, len ) )
                return true;

            std::this_thread::sleep_for( std::chrono::milliseconds( DELAY_FOR_RETRIES ) );
        }
        return false;
    }

    control_range get_xu_range( const extension_unit & xu, uint8_t ctrl, int len ) const override
    {
        return _dev->get_xu_range( xu, ctrl, len );
    }

    bool get_pu( rs2_option opt, int32_t & value ) const override
    {
        for( auto i = 0; i < MAX_RETRIES; ++i )
        {
            if( _dev->get_pu( opt, value ) )
                return true;

            std::this_thread::sleep_for( std::chrono::milliseconds( DELAY_FOR_RETRIES ) );
        }
        return false;
    }

    bool set_pu( rs2_option opt, int32_t value ) override
    {
        for( auto i = 0; i < MAX_RETRIES; ++i )
        {
            if( _dev->set_pu( opt, value ) )
                return true;

            std::this_thread::sleep_for( std::chrono::milliseconds( DELAY_FOR_RETRIES ) );
        }
        return false;
    }

    control_range get_pu_range( rs2_option opt ) const override { return _dev->get_pu_range( opt ); }

    std::vector< stream_profile > get_profiles() const override { return _dev->get_profiles(); }

    std::string get_device_location() const override { return _dev->get_device_location(); }

    usb_spec get_usb_specification() const override { return _dev->get_usb_specification(); }

    void lock() const override { _dev->lock(); }
    void unlock() const override { _dev->unlock(); }

    bool is_platform_jetson() const override { return _dev->is_platform_jetson(); }

private:
    std::shared_ptr< uvc_device > _dev;
};


class multi_pins_uvc_device : public uvc_device
{
public:
    explicit multi_pins_uvc_device( const std::vector< std::shared_ptr< uvc_device > > & dev )
        : _dev( dev )
    {
    }

    void probe_and_commit( stream_profile profile, frame_callback callback, int buffers ) override
    {
        // Decode the SDK-facing pin_index to the matched sub-device native backend pin index
        auto dev_index = decode_pin( profile.pin_index );
        _dev[dev_index]->probe_and_commit( profile, callback, buffers );  // may throw
        _configured_indexes.insert( dev_index );  // record only after a successful commit
    }


    void stream_on( std::function< void( const notification & n ) > error_handler
                    = []( const notification & n ) {} ) override
    {
        for( auto & elem : _configured_indexes )
        {
            _dev[elem]->stream_on( error_handler );
        }
    }
    void start_callbacks() override
    {
        for( auto & elem : _configured_indexes )
        {
            _dev[elem]->start_callbacks();
        }
    }

    void stop_callbacks() override
    {
        for( auto & elem : _configured_indexes )
        {
            _dev[elem]->stop_callbacks();
        }
    }

    void close( stream_profile profile ) override
    {
        // Decode the SDK-facing pin_index to the matched sub-device native backend pin index
        auto dev_index = decode_pin( profile.pin_index );
        _configured_indexes.erase( dev_index );
        _dev[dev_index]->close( profile ); // Might throw, keep after erase()
    }

    void set_power_state( power_state state ) override
    {
        for( auto & elem : _dev )
        {
            elem->set_power_state( state );
        }
    }

    power_state get_power_state() const override { return _dev.front()->get_power_state(); }

    void init_xu( const extension_unit & xu ) override { _dev.front()->init_xu( xu ); }

    bool set_xu( const extension_unit & xu, uint8_t ctrl, const uint8_t * data, int len ) override
    {
        return _dev.front()->set_xu( xu, ctrl, data, len );
    }

    bool get_xu( const extension_unit & xu, uint8_t ctrl, uint8_t * data, int len ) const override
    {
        return _dev.front()->get_xu( xu, ctrl, data, len );
    }

    control_range get_xu_range( const extension_unit & xu, uint8_t ctrl, int len ) const override
    {
        return _dev.front()->get_xu_range( xu, ctrl, len );
    }

    bool get_pu( rs2_option opt, int32_t & value ) const override { return _dev.front()->get_pu( opt, value ); }

    bool set_pu( rs2_option opt, int32_t value ) override { return _dev.front()->set_pu( opt, value ); }

    control_range get_pu_range( rs2_option opt ) const override { return _dev.front()->get_pu_range( opt ); }

    std::vector< stream_profile > get_profiles() const override
    {
        const uint32_t stride = pin_stride();
        std::vector< stream_profile > all_stream_profiles;
        uint32_t dev_index = 0;
        for( auto & elem : _dev )
        {
            auto pin_stream_profiles = elem->get_profiles();
            // Several sub-devices (pins) are combined into one multi_pins_uvc_device, same {w,h,fps,format} can
            // appear on more than one pin (e.g. the two M420 RGB endpoints). We override the native backend pin index
            // (e.g. the Windows MF stream index) to supply SDK with a unique pin ID for each profile.
            // Encode each profile's native index together with its sub-device index into a single reversible value:
            // native + dev_index * stride, where stride = max native pin index + 1.
            for( auto & p : pin_stream_profiles )
                p.pin_index = p.pin_index + dev_index * stride;
            all_stream_profiles.insert( all_stream_profiles.end(),
                                        pin_stream_profiles.begin(),
                                        pin_stream_profiles.end() );
            ++dev_index;
        }
        return all_stream_profiles;
    }

    std::string get_device_location() const override { return _dev.front()->get_device_location(); }

    usb_spec get_usb_specification() const override { return _dev.front()->get_usb_specification(); }

    void lock() const override
    {
        std::vector< uvc_device * > locked_dev;
        try
        {
            for( auto & elem : _dev )
            {
                elem->lock();
                locked_dev.push_back( elem.get() );
            }
        }
        catch( ... )
        {
            for( auto & elem : locked_dev )
            {
                elem->unlock();
            }
            throw;
        }
    }

    void unlock() const override
    {
        for( auto & elem : _dev )
        {
            elem->unlock();
        }
    }

    bool is_platform_jetson() const override
    {
        if(_dev.size() > 0 && _dev[0])
            return _dev[0]->is_platform_jetson();
        return false;
    }

private:
    // Width of the pin_index range reserved per sub-device = (max native pin index across all sub-devices) + 1.
    // Computed lazily (not at construction): get_profiles() requires the device to be powered to D0, which is not
    // the case during enumeration. Cached because the sub-devices' profile sets are static; >= 1, so 0 is a safe
    // "not computed" sentinel.
    uint32_t pin_stride() const
    {
        if( _pin_stride == 0 )
        {
            uint32_t max_native = 0;
            for( auto & elem : _dev )
                for( auto & p : elem->get_profiles() )
                    if( p.pin_index > max_native )
                        max_native = p.pin_index;
            _pin_stride = max_native + 1;
        }
        return _pin_stride;
    }

    // Inverse of the encoding applied in get_profiles(): returns the sub-device index and rewrites 'pin'
    // in place from the encoded value to the sub-device's native backend pin index.
    uint32_t decode_pin( uint32_t & pin ) const
    {
        const uint32_t stride = pin_stride();
        uint32_t dev_index = pin / stride;
        if( dev_index >= _dev.size() )  // Guard against manual pin_index override in the SDK (e.g. for testing)
            throw std::runtime_error( "multi_pins_uvc_device: pin_index out of range" );
        pin %= stride;
        return dev_index;
    }

    std::vector< std::shared_ptr< uvc_device > > _dev;
    std::set< uint32_t > _configured_indexes;
    mutable uint32_t _pin_stride = 0;
};


}  // namespace platform
}  // namespace librealsense
