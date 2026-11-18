// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021-2024 RealSense, Inc. All Rights Reserved.
#pragma once

#include "option.h"
#include "device-calibration.h"



namespace librealsense
{
    class ds_thermal_monitor
    {
    public:
        ds_thermal_monitor(std::shared_ptr<option> temp_option, std::shared_ptr<option> tl_toggle);
        ~ds_thermal_monitor();

        void update(bool on);
        void add_observer(std::function<void(float)> callback)
        {
            _thermal_changes_callbacks.push_back(callback);
        }

    private:
        ds_thermal_monitor(const ds_thermal_monitor&) = delete;       // disable copy and assignment ctors
        ds_thermal_monitor& operator=(const ds_thermal_monitor&) = delete;

        // Active Object's main routine
        void polling(dispatcher::cancellable_timer cancellable_timer);
        void notify(float  temperature);

        active_object<> _monitor;
        unsigned int _poll_intervals_ms;
        float _thermal_threshold_deg;
        float _temp_base;
        bool _hw_loop_on;
        std::weak_ptr<option> _temperature_sensor;
        std::weak_ptr<option> _tl_activation;
        std::vector<std::function<void(float)>>  _thermal_changes_callbacks;   // Distribute notifications on device thermal changes
    };

    //// The class allows to track and calibration updates on the fly
    //// Specifically for D400 the calibration adjustments are generated in FW
    //// and retrieved on demand when a certain (thermal) trigger is invoked
    class ds_thermal_tracking : public calibration_change_device
    {
    public:
        ds_thermal_tracking(std::shared_ptr<ds_thermal_monitor> monitor):
            _monitor(monitor)
        {
            if (auto mon = _monitor.lock())
                mon->add_observer([&](float) { notify_calibration_change(); });
        }

    private:
        std::weak_ptr<ds_thermal_monitor>  _monitor;
    };
}
