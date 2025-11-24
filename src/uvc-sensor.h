// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include "sensor.h"
#include "platform/uvc-device.h"


namespace librealsense {


class uvc_sensor : public raw_sensor_base
{
    typedef raw_sensor_base super;

public:
    explicit uvc_sensor( std::string const & name,
                         std::shared_ptr< platform::uvc_device > uvc_device,
                         std::unique_ptr< frame_timestamp_reader > timestamp_reader,
                         device * dev );
    virtual ~uvc_sensor() override;

    void open( const stream_profiles & requests ) override;
    void close() override;
    void start( rs2_frame_callback_sptr callback ) override;
    void stop() override;
    void register_xu( platform::extension_unit xu );
    void register_pu( rs2_option id );

    virtual void prepare_for_bulk_operation() override;
    virtual void finished_bulk_operation() override;

    std::vector< platform::stream_profile > get_configuration() const { return _internal_config; }
    std::shared_ptr< platform::uvc_device > get_uvc_device() { return _device; }
    platform::usb_spec get_usb_specification() const { return _device->get_usb_specification(); }
    std::string get_device_path() const { return _device->get_device_location(); }

    template< class T >
    auto invoke_powered( T action ) -> decltype( action( *static_cast< platform::uvc_device * >( nullptr ) ) )
    {
        power on( std::dynamic_pointer_cast< uvc_sensor >( shared_from_this() ) );
        return action( *_device );
    }

    
    void power_for_duration( std::chrono::steady_clock::duration timeout = std::chrono::milliseconds( 500 ) )
    {
        acquire_power();
        std::weak_ptr< uvc_sensor > weak = std::dynamic_pointer_cast< uvc_sensor >( shared_from_this() );
        std::thread release_power_thread( [weak, timeout]()
        {
            std::this_thread::sleep_for( timeout );
            if( auto strong = weak.lock() )
                strong->release_power();
        } );
        release_power_thread.detach();
    }

protected:
    stream_profiles init_stream_profiles() override;
    void verify_supported_requests( const stream_profiles & requests ) const;

private:
    void acquire_power();
    void release_power();
    void reset_streaming();
    std::atomic<int64_t> _gyro_counter;
    std::atomic<int64_t> _accel_counter;


    struct power
    {
        explicit power( std::weak_ptr< uvc_sensor > owner )
            : _owner( owner )
        {
            auto strong = _owner.lock();
            if( strong )
            {
                strong->acquire_power();
            }
        }

        ~power()
        {
            if( auto strong = _owner.lock() )
            {
                try
                {
                    strong->release_power();
                }
                catch( ... )
                {
                }
            }
        }

    private:
        std::weak_ptr< uvc_sensor > _owner;
    };

    std::shared_ptr< platform::uvc_device > _device;
    std::vector< platform::stream_profile > _internal_config;
    std::mutex _configure_lock;
    std::unique_ptr< power > _power;
    std::unique_ptr< frame_timestamp_reader > _timestamp_reader;
};


// Helper function that should be used when multiple FW calls needs to be made.
// This function change the USB power to D0 (Operational) using the invoke_power function
// activate the received function and power down the state to D3 (Idle)
//
template< class T >
auto group_multiple_fw_calls( synthetic_sensor & s, T action ) -> decltype( action() )
{
    auto & us = dynamic_cast< uvc_sensor & >( *s.get_raw_sensor() );

    return us.invoke_powered( [&]( platform::uvc_device & dev ) { return action(); } );
}


}  // namespace librealsense
