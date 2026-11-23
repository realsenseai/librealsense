// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "device_hub.h"

#include <rsutils/easylogging/easyloggingpp.h>
#include <librealsense2/rs.hpp>


namespace librealsense
{
    typedef rs2::devices_changed_callback<std::function<void(rs2::event_information& info)>> hub_devices_changed_callback;

    device_hub::device_hub( std::shared_ptr< librealsense::context > ctx, int mask )
        : _ctx( ctx )
        , _mask( mask )
    {
    }

    void device_hub::init( std::shared_ptr< device_hub > const & sptr )
    {
        _device_list = _ctx->query_devices( _mask );

        _device_change_subscription = _ctx->on_device_changes(
            [&, liveliness = std::weak_ptr< device_hub >( sptr )](
                std::vector< std::shared_ptr< device_info > > const & /*removed*/,
                std::vector< std::shared_ptr< device_info > > const & /*added*/ )
            {
                if( auto keepalive = liveliness.lock() )
                {
                    std::unique_lock< std::mutex > lock( _mutex );

                    _device_list = _ctx->query_devices( _mask );

                    // Current device will point to the first available device
                    _camera_index = 0;
                    if( _device_list.size() > 0 )
                        _cv.notify_all();
                }
            } );
    }

    /*static*/ std::shared_ptr< device_hub > device_hub::make( std::shared_ptr< librealsense::context > const & ctx,
                                                               int mask )
    {
        std::shared_ptr< device_hub > sptr( new device_hub( ctx, mask ) );
        sptr->init( sptr );
        return sptr;
    }

    device_hub::~device_hub()
    {
    }

    std::shared_ptr<device_interface> device_hub::create_device(const std::string& serial, bool cycle_devices)
    {
        std::shared_ptr<device_interface> res = nullptr;
        for(size_t i = 0; ((i< _device_list.size()) && (nullptr == res)); i++)
        {
            // _camera_index is the curr device that the hub will expose
            auto d = _device_list[ (_camera_index + i) % _device_list.size()];
            try
            {
                auto dev = d->create_device();

                if(serial.size() > 0 )
                {
                    auto new_serial = dev->get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);

                    if(serial == new_serial)
                    {
                        res = dev;
                        cycle_devices = false;  // Requesting a device by its serial shall not invoke internal cycling
                    }
                }
                else // Use the first selected if "any device" pattern was used
                {
                    res = dev;
                }
            }
            catch (const std::exception& ex)
            {
                LOG_WARNING("Could not open device " << ex.what());
            }
        }

        // Advance the internal selection when appropriate
        if (res && cycle_devices)
            _camera_index = (1+_camera_index) % _device_list.size();

        return res;
    }


    /**
     * If any device is connected return it, otherwise wait until next RealSense device connects.
     * Calling this method multiple times will cycle through connected devices
     */
    std::shared_ptr<device_interface> device_hub::wait_for_device(const std::chrono::milliseconds& timeout, bool loop_through_devices, const std::string& serial)
    {
        std::unique_lock<std::mutex> lock(_mutex);

        // The list is kept current by the device-changes callback; query again only when it has no match
        auto res = create_device(serial, loop_through_devices);
        if (!res)
        {
            _device_list = _ctx->query_devices(_mask);
            res = create_device(serial, loop_through_devices);
        }

        if (res) return res;

        // block for the requested device to be connected, or till the timeout occurs
        if (!_cv.wait_for(lock, timeout, [&]()
        {
            if (_device_list.size() > 0)
            {
                res = create_device(serial, loop_through_devices);
            }
            return res != nullptr;
        }))
        {
            throw std::runtime_error("No device connected");
        }
        return res;
    }

    /**
    * Checks if device is still connected
    */
    bool device_hub::is_connected(const device_interface& dev)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return dev.is_valid();
    }

    std::shared_ptr<librealsense::context> device_hub::get_context()
    {
        return _ctx;
    }
}

