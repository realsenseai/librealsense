// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021-2024 RealSense, Inc. All Rights Reserved.

#include <iostream>
#include <chrono>
#include "ds-private.h"
#include "ds-thermal-monitor.h"

namespace librealsense
{
    ds_thermal_monitor::ds_thermal_monitor(std::shared_ptr<option> temp_option, std::shared_ptr<option> tl_toggle) :
        _monitor([this](dispatcher::cancellable_timer cancellable_timer)
            {
                polling(cancellable_timer);
            }, rsutils::concurrency::thread_category_device_monitoring, "thermal-mon"),
        _poll_intervals_ms(2000), // Temperature check routine to be invoked every 2 sec
        _thermal_threshold_deg(2.f),
        _temp_base(0.f),
        _hw_loop_on(false),
        _temperature_sensor(temp_option),
        _tl_activation(tl_toggle)
    {
    }

    ds_thermal_monitor::~ds_thermal_monitor()
    {
        _monitor.stop();
        _temp_base = 0.f;
        _hw_loop_on = false;
    }

    void ds_thermal_monitor::update(bool on)
    {
        if (on != _monitor.is_active())
        {
            if (!on)
            {
                _monitor.stop();
                _hw_loop_on = false;
                notify(0);
            }
            else
            {
                _monitor.start();
            }
        }
    }

    void ds_thermal_monitor::polling(dispatcher::cancellable_timer cancellable_timer)
    {
        if (cancellable_timer.try_sleep( std::chrono::milliseconds( _poll_intervals_ms )))
        {
            try
            {
                // Verify TL is active on FW level
                if (auto tl_active = _tl_activation.lock())
                {
                    bool tl_state = (std::fabs(tl_active->query()) > std::numeric_limits< float >::epsilon());
                    if (tl_state != _hw_loop_on)
                    {
                        _hw_loop_on = tl_state;
                        if (!_hw_loop_on)
                            notify(0);

                    }

                    if (!tl_state)
                        return;
                }

                // Track temperature and update on temperature changes
                auto ts = (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
                if( auto temp = _temperature_sensor.lock() )
                {
                    if( temp->is_enabled() )
                    {
                        auto cur_temp = temp->query();
                        if( fabs( _temp_base - cur_temp ) >= _thermal_threshold_deg )
                        {
                            LOG_DEBUG_THERMAL_LOOP( "Thermal calibration adjustment is triggered on change from "
                                                    << std::dec << std::setprecision( 1 ) << _temp_base << " to "
                                                    << cur_temp << " deg (C)" );

                            notify( cur_temp );
                        }
                    }
                }
                else
                {
                    LOG_ERROR("Thermal Compensation: temperature sensor option is not present");
                }
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR("Error during thermal compensation handling: " << ex.what());
            }
            catch (...)
            {
                LOG_ERROR("Unresolved error during Thermal Compensation handling");
            }
        }
        else
        {
            LOG_DEBUG_THERMAL_LOOP("Thermal Compensation is being shut-down");
        }
    }

    void ds_thermal_monitor::notify(float temperature)
    {
        _temp_base = temperature;
        for (auto&& cb : _thermal_changes_callbacks)
            cb(temperature);
    }
}
