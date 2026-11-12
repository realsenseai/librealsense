// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 RealSense, Inc. All Rights Reserved.
#pragma once

#include "types.h"
#include "core/extension.h"

#include <atomic>
#include <mutex>
#include <vector>


namespace librealsense
{
    // A device whose calibration can change after it is created. Users learn of a change through callbacks.
    // Frame consumers poll the change count instead, so they hold no registration.
    class calibration_change_device
    {
    public:
        virtual ~calibration_change_device() = default;

        void register_calibration_change_callback( rs2_calibration_change_callback_sptr callback )
        {
            std::lock_guard< std::mutex > lock( _callbacks_mutex );
            _callbacks.push_back( std::move( callback ) );
        }

        unsigned get_calibration_change_count() const { return _change_count; }

    protected:
        // Call only once the new calibration reads back, as callbacks may re-read it right away
        void notify_calibration_change()
        {
            ++_change_count;
            std::vector< rs2_calibration_change_callback_sptr > callbacks;
            {
                std::lock_guard< std::mutex > lock( _callbacks_mutex );
                callbacks = _callbacks;
            }
            for( auto & callback : callbacks )
                callback->on_calibration_change( RS2_CALIBRATION_SUCCESSFUL );
        }

    private:
        std::atomic< unsigned > _change_count{ 0 };
        std::mutex _callbacks_mutex;
        std::vector< rs2_calibration_change_callback_sptr > _callbacks;
    };

    MAP_EXTENSION(RS2_EXTENSION_CALIBRATION_CHANGE_DEVICE, calibration_change_device);

    // This extension should allow generic calibrations using the same interface
    // by adding to rs2_calibration_type instead of adding new function calls
    class device_calibration : public calibration_change_device
    {
    public:
        virtual void trigger_device_calibration( rs2_calibration_type ) = 0;
    };

    MAP_EXTENSION(RS2_EXTENSION_DEVICE_CALIBRATION, device_calibration );
}
