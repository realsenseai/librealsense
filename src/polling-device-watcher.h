// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#pragma once

#include "backend.h"
#include "platform/device-watcher.h"
#include <rsutils/concurrency/concurrency.h>
#include "callback-invocation.h"


namespace librealsense {


// This device_watcher enumerates all devices every set amount of time (POLLING_DEVICES_INTERVAL_MS)
//
class polling_device_watcher : public librealsense::platform::device_watcher
{
public:
    polling_device_watcher( const platform::backend * backend_ref )
        : _backend( backend_ref )
        , _active_object( [this]( dispatcher::cancellable_timer cancellable_timer ) { polling( cancellable_timer ); },
                          rsutils::concurrency::thread_category_device_monitoring, "poll-dev-w" )
        , _devices_data()
    {
        _devices_data = { _backend->query_uvc_devices(), _backend->query_usb_devices(), _backend->query_hid_devices() };
    }

    ~polling_device_watcher() { stop(); }

    void polling( dispatcher::cancellable_timer cancellable_timer )
    {
        if( cancellable_timer.try_sleep( std::chrono::milliseconds( POLLING_DEVICES_INTERVAL_MS ) ) )
        {
            platform::backend_device_group curr( _backend->query_uvc_devices(),
                                                 _backend->query_usb_devices(),
                                                 _backend->query_hid_devices() );
            if( list_changed( _devices_data.uvc_devices, curr.uvc_devices )
                || list_changed( _devices_data.usb_devices, curr.usb_devices )
                || list_changed( _devices_data.hid_devices, curr.hid_devices ) )
            {
                callback_invocation_holder callback = { _callback_inflight.allocate(), &_callback_inflight };
                if( callback )
                {
                    _callback( _devices_data, curr );
                    _devices_data = curr;
                }
            }
        }
    }

    void start( platform::device_changed_callback callback ) override
    {
        stop();
        _callback = std::move( callback );
        _active_object.start();
    }

    void stop() override
    {
        _active_object.stop();

        _callback_inflight.wait_until_empty();
    }

    bool is_stopped() const override { return ! _active_object.is_active(); }

private:
    active_object<> _active_object;

    callbacks_heap _callback_inflight;
    const platform::backend * _backend;

    platform::backend_device_group _devices_data;
    platform::device_changed_callback _callback;
};


}  // namespace librealsense
