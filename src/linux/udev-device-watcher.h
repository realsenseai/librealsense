// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#pragma once

#include "../backend.h"
#include "../platform/device-watcher.h"
#include <rsutils/concurrency/concurrency.h>
#include "../callback-invocation.h"

#include <libudev.h>

#include <chrono>
#include <map>
#include <string>


namespace librealsense {


class udev_device_watcher : public librealsense::platform::device_watcher
{
    active_object<> _active_object;

    callbacks_heap _callback_inflight;
    platform::backend const * _backend;

    platform::backend_device_group _devices_data;
    platform::device_changed_callback _callback;

    struct udev * _udev_ctx;
    struct udev_monitor * _udev_monitor;
    int _udev_monitor_fd;
    bool _changed = false;
    // When each still-enumerating device was first seen that way, so one that never
    // finishes is held back once rather than forever (see incomplete_devices).
    std::map< std::string, std::chrono::steady_clock::time_point > _incomplete_since;

public:
    udev_device_watcher( platform::backend const * );
    ~udev_device_watcher();

    // device_watcher
public:
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
    void foreach_device( std::function< void( struct udev_device* udev_dev ) > );
};


}  // namespace librealsense
