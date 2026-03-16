// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.
#include "global_timestamp_reader.h"
#include <chrono>

using namespace std::chrono;

namespace librealsense
{
    CSample& CSample::operator-=(const CSample& other)
    {
        _x -= other._x;
        _y -= other._y;
        return *this;
    }

    CSample& CSample::operator+=(const CSample& other)
    {
        _x += other._x;
        _y += other._y;
        return *this;
    }

    CLinearCoefficients::CLinearCoefficients(unsigned int buffer_size) :
        _base_sample(0, 0),
        _buffer_size(buffer_size),
        _time_span_ms(1000) // Spread the linear equation modifications over a whole second.
    {
    }

    void CLinearCoefficients::reset()
    {
        _last_values.clear();
    }

    bool CLinearCoefficients::is_full() const
    {
        return _last_values.size() >= _buffer_size;
    }

    void CLinearCoefficients::add_value(CSample val)
    {
        while (_last_values.size() > _buffer_size)
        {
            _last_values.pop_back();
        }
        _last_values.push_front(val);
        calc_linear_coefs();
    }

    void CLinearCoefficients::add_const_y_coefs(double dy)
    {
        for (auto &&sample : _last_values)
        {
            sample._y += dy;
        }
    }

    void CLinearCoefficients::calc_linear_coefs()
    {
        // Calculate linear coefficients, based on calculus described in: https://www.statisticshowto.datasciencecentral.com/probability-and-statistics/regression-analysis/find-a-linear-regression-equation/
        // Calculate Std
        double n(static_cast<double>(_last_values.size()));
        double a(1);
        double b(0);
        double dt(1);
        if (n == 1)
        {
            _base_sample = _last_values.back();
            _dest_a = 1;
            _dest_b = 0;
            _prev_a = 0;
            _prev_b = 0;
            _last_request_time = _last_values.front()._x;
        }
        else
        {
            double sum_x(0);
            double sum_y(0);
            double sum_xy(0);
            double sum_x2(0);
            for (auto sample = _last_values.begin(); sample != _last_values.end(); sample++)
            {
                CSample crnt_sample(*sample);
                crnt_sample -= _base_sample;
                sum_x += crnt_sample._x;
                sum_y += crnt_sample._y;
                sum_xy += (crnt_sample._x * crnt_sample._y);
                sum_x2 += (crnt_sample._x * crnt_sample._x);
            }
            double denom = n * sum_x2 - sum_x * sum_x;
            if( denom > std::numeric_limits< double >::epsilon() )
            {
                b = (sum_y * sum_x2 - sum_x * sum_xy) / denom;
                a = (n * sum_xy - sum_x * sum_y) / denom;
            }
            else
            {
                a = _dest_a;
                b = _dest_b;
            }
            if( _last_request_time - _prev_time < _time_span_ms )
            {
                dt = (_last_request_time - _prev_time) / _time_span_ms;
            }
        }
        _prev_a = _dest_a * dt + _prev_a * (1 - dt);
        _prev_b = _dest_b * dt + _prev_b * (1 - dt);
        _dest_a = a;
        _dest_b = b;
        _prev_time = _last_request_time;
    }

    void CLinearCoefficients::get_a_b(double x, double& a, double& b) const
    {
        a = _dest_a;
        b = _dest_b;
        if (x - _prev_time < _time_span_ms)
        {
            double dt((x - _prev_time) / _time_span_ms);
            a = _dest_a * dt + _prev_a * (1 - dt);
            b = _dest_b * dt + _prev_b * (1 - dt);
        }
    }

    double CLinearCoefficients::calc_value(double x) const
    {
        double a, b;
        get_a_b(x, a, b);
        double y(a * (x - _base_sample._x) + b + _base_sample._y);
        //LOG_DEBUG(__FUNCTION__ << ": " << x << " -> " << y << " with coefs:" << a << ", " << b << ", " << _base_sample._x << ", " << _base_sample._y);
        return y;
    }


    // Method update_samples_base
    // Aim: This method is used in our code to update global timestamp.
    // It updates the relative origin of the HW timestamp if it had a rewind,
    // so that the global timestamp can be correctly computed
    bool CLinearCoefficients::update_samples_base(double x)
    {
        static const double max_device_time(pow(2, 32) * MICROSEC_TO_MILLISEC);
        double base_x;
        if (_last_values.empty())
            return false;
        if ((_last_values.front()._x - x) > max_device_time / 2)
            base_x = max_device_time;
        else if ((x - _last_values.front()._x) > max_device_time / 2)
            base_x = -max_device_time;
        else
            return false;
        LOG_DEBUG(__FUNCTION__ << "(" << base_x << ")");

        double a, b;
        get_a_b(x + base_x, a, b);
        for (auto &&sample : _last_values)
        {
            sample._x -= base_x;
        }
        _prev_time -= base_x;
        _base_sample._x -= base_x;
        return true;
    }

    void CLinearCoefficients::update_last_sample_time(double x)
    {
        _last_request_time = x;
    }

    time_diff_keeper::time_diff_keeper(global_time_interface* dev, const unsigned int sampling_interval_ms) :
        _device(dev),
        _poll_intervals_ms(sampling_interval_ms),
        _coefs(15),
        _users_count(0),
        _is_ready(false),
        _min_command_delay(1000),
        _active_object([this](dispatcher::cancellable_timer cancellable_timer)
            {
                polling(cancellable_timer);
            })
    {
        //LOG_DEBUG("start new time_diff_keeper ");
    }

    void time_diff_keeper::start()
    {
        std::lock_guard<std::recursive_mutex> lock(_enable_mtx);
        _users_count++;
        LOG_DEBUG("time_diff_keeper::start: _users_count = " << _users_count);
        _active_object.start();
    }

    void time_diff_keeper::stop()
    {
        std::lock_guard<std::recursive_mutex> lock(_enable_mtx);
        if (_users_count <= 0)
            LOG_ERROR("time_diff_keeper users_count <= 0.");

        _users_count--;
        LOG_DEBUG("time_diff_keeper::stop: _users_count = " << _users_count);
        if (_users_count == 0)
        {
            LOG_DEBUG("time_diff_keeper::stop: stop object.");
            _active_object.stop();
            _is_ready = false;
            std::lock_guard< std::recursive_mutex > lock( _read_mtx );
            _coefs.reset();
        }
    }

    time_diff_keeper::~time_diff_keeper()
    {
        _active_object.stop();
    }

    bool time_diff_keeper::update_diff_time()
    {
        try
        {
            if (!_users_count)
                throw wrong_api_call_sequence_exception("time_diff_keeper::update_diff_time called before object started.");
            double system_time_start = duration<double, std::milli>(system_clock::now().time_since_epoch()).count();
            double sample_hw_time = _device->get_device_time_ms();
            double system_time_finish = duration<double, std::milli>(system_clock::now().time_since_epoch()).count();
            double command_delay = (system_time_finish-system_time_start)/2;

            std::lock_guard<std::recursive_mutex> lock(_read_mtx);
            if (command_delay < _min_command_delay)
            {
                _coefs.add_const_y_coefs(command_delay - _min_command_delay);
                _min_command_delay = command_delay;
            }
            double system_time(system_time_finish - _min_command_delay);
            if (_is_ready)
            {
                _coefs.update_samples_base(sample_hw_time);
            }
            CSample crnt_sample(sample_hw_time, system_time);
            _coefs.add_value(crnt_sample);
            _is_ready = true;
            return true;
        }
        catch (const io_exception& ex)
        {
            LOG_DEBUG("Temporary skip during time_diff_keeper polling: " << ex.what());
        }
        catch (const wrong_api_call_sequence_exception& ex)
        {
            LOG_DEBUG("Temporary skip during time_diff_keeper polling: " << ex.what());
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR("Error during time_diff_keeper polling: " << ex.what());
        }
        catch (...)
        {
            LOG_ERROR("Unknown error during time_diff_keeper polling!");
        }
        return false;
    }

    void time_diff_keeper::polling(dispatcher::cancellable_timer cancellable_timer)
    {
        update_diff_time();
        unsigned int time_to_sleep = _poll_intervals_ms + _coefs.is_full() * (9 * _poll_intervals_ms);
        if (!cancellable_timer.try_sleep( std::chrono::milliseconds( time_to_sleep )))
        {
            LOG_DEBUG("Notification: time_diff_keeper polling loop is being shut-down");
        }
    }

    double time_diff_keeper::get_system_hw_time(double crnt_hw_time, bool& is_ready)
    {
        std::lock_guard<std::recursive_mutex> lock(_read_mtx);
        is_ready = _is_ready;
        if (_is_ready)
        {
            _coefs.update_samples_base(crnt_hw_time);
            _coefs.update_last_sample_time(crnt_hw_time);
            return _coefs.calc_value(crnt_hw_time);
        }
        else
            return crnt_hw_time;
    }

    global_timestamp_reader::global_timestamp_reader(std::unique_ptr<frame_timestamp_reader> device_timestamp_reader,
                                                     std::shared_ptr<time_diff_keeper> timediff,
                                                     std::shared_ptr<global_time_option> enable_option) :
        _device_timestamp_reader(std::move(device_timestamp_reader)),
        _time_diff_keeper(timediff),
        _option_is_enabled(enable_option),
        _ts_is_ready(false)
    {
    }

    double global_timestamp_reader::get_frame_timestamp(const std::shared_ptr<frame_interface>& frame)
    {
        double frame_time = _device_timestamp_reader->get_frame_timestamp(frame);
        rs2_timestamp_domain ts_domain = _device_timestamp_reader->get_frame_timestamp_domain(frame);
        if (_option_is_enabled->is_true() && ts_domain == RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK)
        {
            auto sp = _time_diff_keeper.lock();
            if (sp)
                frame_time = sp->get_system_hw_time(frame_time, _ts_is_ready);
            else
                LOG_DEBUG("Notification: global_timestamp_reader - time_diff_keeper is being shut-down");
        }
        return frame_time;
    }


    unsigned long long global_timestamp_reader::get_frame_counter(const std::shared_ptr<frame_interface>& frame) const
    {
        return _device_timestamp_reader->get_frame_counter(frame);
    }

    rs2_timestamp_domain global_timestamp_reader::get_frame_timestamp_domain(const std::shared_ptr<frame_interface>& frame) const
    {
        rs2_timestamp_domain ts_domain = _device_timestamp_reader->get_frame_timestamp_domain(frame);
        return (_option_is_enabled->is_true() && _ts_is_ready && ts_domain == RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK) ? RS2_TIMESTAMP_DOMAIN_GLOBAL_TIME : ts_domain;
    }

    void global_timestamp_reader::reset()
    {
        _device_timestamp_reader->reset();
    }

    global_time_interface::global_time_interface() :
        _tf_keeper(std::make_shared<time_diff_keeper>(this, 100))
    {}

    void global_time_interface::enable_time_diff_keeper(bool is_enable)
    {
        if (is_enable)
            _tf_keeper->start();
        else
            _tf_keeper->stop();
    }
}
