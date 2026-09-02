// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.
#include "global_timestamp_reader.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

using namespace std::chrono;

namespace librealsense
{
    // Clock-sync sample admission, delay gate (see time_diff_keeper::update_diff_time).
    // Each poll pairs a device-clock reading with a system time. If the control-transfer round-trip is
    // much slower than the fastest seen, the device clock could have been read anywhere within that
    // round-trip: the pairing is unreliable and a single such sample can shift the fit by seconds.
    // Reject these, but not forever - after too many in a row accept one, to keep the fit from going stale.
    static const double max_delay_over_min_ms = 15.;
    static const unsigned int max_rejections_in_a_row = 10;

    // Clock-sync sample admission, value gate (innovation gate).
    // The delay gate can't catch a sample that arrived quickly but carries a wrong device-clock reading.
    // So each sample is also compared against the fit's own prediction: if the measured system time
    // differs from the predicted one by more than max_innovation_ms it cannot be genuine clock drift
    // (which is at most tens of ms between polls) and is rejected before it can move the fit.
    //
    // A rejected sample is not necessarily the wrong one, though - the fit may be. Two mechanisms keep a
    // bad fit from rejecting good samples forever:
    //  1. The fit is computed robustly (theil_sen_fit), so a minority of outlier samples can't skew it.
    //  2. If the fit rejects every sample for max_consecutive_rejections polls in a row it is assumed
    //     wrong (stale, or the device clock re-based) and rebuilt from the rejected samples themselves:
    //     the last re_fit_window rejections are kept and passed to refit_from_samples(). This self-heals
    //     within max_consecutive_rejections polls and never drops readiness.
    static const double max_innovation_ms = 100.;
    static const unsigned int max_consecutive_rejections = 60;
    static const unsigned int re_fit_window = 15;

    // _min_command_delay sentinel before any sample was measured.
    static const double initial_min_command_delay_ms = 1000.;

    // The device clock is a 32-bit microsecond counter; it wraps every 2^32 us (~71.6 minutes).
    static const double max_device_time_ms = 4294967296. * MICROSEC_TO_MILLISEC;

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

    // Robust linear fit over a window of (x, y) samples: Theil-Sen - the median of all pairwise slopes,
    // then the median of the intercepts that slope implies. Unlike least squares, a minority of outlier
    // samples (up to ~29% of the window) can't move the result, which is why the fit tolerates the
    // occasional bad clock reading. The window is small (<=16 samples) so the O(n^2) slope enumeration
    // is cheap. Samples are centered on base_sample only for numerical conditioning; the result does not
    // depend on that choice. Returns false without touching a, b if there aren't two samples with
    // distinct x to form a slope.
    bool CLinearCoefficients::theil_sen_fit(const std::deque<CSample>& values, const CSample& base_sample, double& a, double& b)
    {
        static const double min_dx_ms = 1e-6; // Guard degenerate/near-duplicate x pairs (would blow up the slope estimate).
        std::vector<CSample> centered;
        centered.reserve(values.size());
        for (auto& s : values)
        {
            CSample c(s);
            c -= base_sample;
            centered.push_back(c);
        }
        size_t n = centered.size();
        if (n < 2)
            return false;
        std::vector<double> slopes;
        slopes.reserve(n * (n - 1) / 2);
        for (size_t i = 0; i < n; ++i)
        {
            for (size_t j = i + 1; j < n; ++j)
            {
                double dx = centered[j]._x - centered[i]._x;
                if (std::fabs(dx) < min_dx_ms)
                    continue;
                slopes.push_back((centered[j]._y - centered[i]._y) / dx);
            }
        }
        if (slopes.empty())
            return false;
        std::sort(slopes.begin(), slopes.end());
        size_t mid = slopes.size() / 2;
        a = (slopes.size() % 2) ? slopes[mid] : (slopes[mid - 1] + slopes[mid]) / 2.0;

        std::vector<double> intercepts;
        intercepts.reserve(n);
        for (auto& c : centered)
            intercepts.push_back(c._y - a * c._x);
        std::sort(intercepts.begin(), intercepts.end());
        size_t imid = intercepts.size() / 2;
        b = (intercepts.size() % 2) ? intercepts[imid] : (intercepts[imid - 1] + intercepts[imid]) / 2.0;
        return true;
    }

    void CLinearCoefficients::calc_linear_coefs()
    {
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
            if (!theil_sen_fit(_last_values, _base_sample, a, b))
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

    // Replace the fit outright from an explicit sample window, instead of blending a sample in gradually
    // the way add_value() does. Used to rebuild the fit after it has been rejecting samples for too long
    // (see max_consecutive_rejections), so a re-based device clock - or a fit that itself went bad - is
    // corrected at once rather than over the next second. base_sample is re-anchored to the window so
    // the fit is self-contained.
    void CLinearCoefficients::refit_from_samples(const std::deque<CSample>& samples)
    {
        if (samples.empty())
            return;
        _last_values = samples;
        _base_sample = samples.front();
        double a(1), b(0);
        theil_sen_fit(_last_values, _base_sample, a, b);
        _dest_a = _prev_a = a;
        _dest_b = _prev_b = b;
        _last_request_time = samples.front()._x;
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
        double base_x;
        if (_last_values.empty())
            return false;
        if ((_last_values.front()._x - x) > max_device_time_ms / 2)
            base_x = max_device_time_ms;
        else if ((x - _last_values.front()._x) > max_device_time_ms / 2)
            base_x = -max_device_time_ms;
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

    // Shift x by whole wrap periods onto the epoch of anchor. Two device-clock readings can only
    // be compared on a continuous axis after mapping them onto the same wrap epoch.
    double CLinearCoefficients::align_to_epoch(double x, double anchor)
    {
        double k = std::round((anchor - x) / max_device_time_ms);
        return x + k * max_device_time_ms;
    }

    // Map a HW time onto the fit's current wrap epoch WITHOUT mutating shared state.
    // Near a wrap, frames from different streams of the same device straddle the boundary (one
    // reads ~2^32 us, the next ~0) and are converted interleaved. The fit is shared across those
    // streams, so re-basing it per frame here would corrupt the others' conversions; instead just
    // shift the queried value onto the samples' epoch and leave the fit untouched.
    double CLinearCoefficients::to_fit_domain(double x) const
    {
        if (_last_values.empty())
            return x;
        return align_to_epoch(x, _last_values.front()._x);
    }

    time_diff_keeper::time_diff_keeper(global_time_interface* dev, const unsigned int sampling_interval_ms) :
        _device(dev),
        _poll_intervals_ms(sampling_interval_ms),
        _coefs(15),
        _users_count(0),
        _is_ready(false),
        _min_command_delay(initial_min_command_delay_ms),
        _rejections_in_row(0),
        _innovation_rejections_in_row(0),
        _first_sample_dropped(false),
        _active_object([this](dispatcher::cancellable_timer cancellable_timer)
            {
                polling(cancellable_timer);
            }, "time-diff-keeper")
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
            std::lock_guard< std::recursive_mutex > lock( _read_mtx );
            _is_ready = false;
            _first_sample_dropped = false;
            _coefs.reset();
            _rejections_in_row = 0;
            _innovation_rejections_in_row = 0;
            _rejected_samples.clear();
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
            // Drop the first clock read if it's slow - it can skew the few-point fit for ~2s.
            if( ! _first_sample_dropped )
            {
                _first_sample_dropped = true;
                if( command_delay > 10. )
                    return false;
            }

            if (command_delay < _min_command_delay)
            {
                _coefs.add_const_y_coefs(command_delay - _min_command_delay);
                _min_command_delay = command_delay;
            }
            else if (command_delay - _min_command_delay > max_delay_over_min_ms)
            {
                // Delayed control transfer (e.g. USB congestion): the pairing is unreliable
                // (see the delay-gate comment above max_delay_over_min_ms).
                if (!_is_ready)
                {
                    // During warmup accept it anyway - rejecting could starve the fit before it is ever
                    // ready, and enforce_monotonicity bounds the damage a noisy early fit can do.
                    LOG_DEBUG("time_diff_keeper: accepting delayed clock sample during warmup: command delay "
                              << command_delay << " ms exceeds minimal delay " << _min_command_delay
                              << " ms by more than " << max_delay_over_min_ms << " ms");
                }
                else if (++_rejections_in_row <= max_rejections_in_a_row)
                {
                    LOG_DEBUG("time_diff_keeper: rejecting delayed clock sample: command delay " << command_delay
                              << " ms exceeds minimal delay " << _min_command_delay << " ms by more than "
                              << max_delay_over_min_ms << " ms");
                    return false;
                }
                else
                {
                    LOG_WARNING("time_diff_keeper: accepting delayed clock sample (command delay " << command_delay
                                << " ms, minimal delay " << _min_command_delay << " ms) after " << _rejections_in_row - 1
                                << " consecutive rejections, to avoid a stale fit");
                }
            }
            _rejections_in_row = 0;
            double system_time(system_time_finish - _min_command_delay);
            if (_is_ready)
            {
                _coefs.update_samples_base(sample_hw_time);
                // Value gate (see max_innovation_ms): compare the measured system time against the fit's
                // prediction for this device time. If this sample also improved _min_command_delay above,
                // calc_value() still returns the pre-shift prediction (the shift is applied on the next
                // recompute); that error is a few ms at most, negligible against the gate.
                // update_samples_base above already re-based the fit into sample_hw_time's epoch, so
                // calling calc_value with the raw value is in-domain (no to_fit_domain mapping needed).
                double predicted_system_time = _coefs.calc_value(sample_hw_time);
                double innovation = system_time - predicted_system_time;
                if (std::fabs(innovation) > max_innovation_ms)
                {
                    ++_innovation_rejections_in_row;
                    // A rejection streak can span the HW clock wrap; keep the window on one wrap
                    // epoch so refit_from_samples() gets a continuous x-axis.
                    double rejected_x = sample_hw_time;
                    if (!_rejected_samples.empty())
                        rejected_x = CLinearCoefficients::align_to_epoch(rejected_x, _rejected_samples.front()._x);
                    _rejected_samples.push_front(CSample(rejected_x, system_time));
                    if (_rejected_samples.size() > re_fit_window)
                        _rejected_samples.pop_back();

                    if (_innovation_rejections_in_row < max_consecutive_rejections)
                    {
                        LOG_DEBUG("time_diff_keeper: rejecting clock sample on value: measured system time "
                                  << system_time << " ms vs. predicted " << predicted_system_time
                                  << " ms; innovation " << innovation << " ms exceeds "
                                  << max_innovation_ms << " ms (" << _innovation_rejections_in_row
                                  << " consecutive rejections)");
                        return false;
                    }

                    // Rejected every sample for max_consecutive_rejections polls in a row: the fit is no
                    // longer trustworthy (the device clock re-based, or the fit went bad). Rebuild it from
                    // the rejected samples - they are consistent readings the fit fails to match - and
                    // resume. Never drops _is_ready; only stop() does that.
                    _coefs.refit_from_samples(_rejected_samples);
                    LOG_WARNING("time_diff_keeper: clock fit re-based from rejected-sample window after "
                                << _innovation_rejections_in_row << " consecutive innovation-gate rejections");
                    _innovation_rejections_in_row = 0;
                    _rejected_samples.clear();
                    return true;
                }
            }
            _innovation_rejections_in_row = 0;
            _rejected_samples.clear();
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
            // Read-only wrap alignment: only the polling thread (update_diff_time) may
            // re-base the fit; see to_fit_domain().
            double x = _coefs.to_fit_domain(crnt_hw_time);
            _coefs.update_last_sample_time(x);
            return _coefs.calc_value(x);
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
        _ts_is_ready(false),
        _last_hw_time_ms(-1),
        _last_global_time_ms(0)
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
            {
                double hw_time = frame_time;
                frame_time = sp->get_system_hw_time(hw_time, _ts_is_ready);
                if (_ts_is_ready)
                    frame_time = enforce_monotonicity(hw_time, frame_time);
            }
            else
                LOG_DEBUG("Notification: global_timestamp_reader - time_diff_keeper is being shut-down");
        }
        return frame_time;
    }

    // A frame that is later on the HW clock must never get an earlier global timestamp than the frame
    // before it: a bad clock-sync sample can make the fit walk backwards faster than frames advance,
    // reversing frame order in global time and breaking stream pairing downstream. When that happens,
    // hold the last value. The clamp applies only while the HW clock advances continuously; on a HW
    // rewind or a streaming gap the fit is trusted as-is and tracking restarts.
    //
    // The hold is bounded by max_monotonic_hold_ms so it bridges the small per-frame inversions it
    // exists for, but a larger drop (the fit genuinely re-basing) is accepted as a single backward step
    // rather than held. This trades one bounded ordering violation for never freezing a stream's
    // timestamps at a stuck value - an unbounded hold could freeze a stream indefinitely, which is worse.
    //
    // Ownership: a global_timestamp_reader instance is per-sensor (only the time_diff_keeper is shared
    // across the device), so depth and color frames never interleave through this state. Streams of the
    // same sensor that do interleave with independent HW times (e.g. gyro+accel) hit the hw-rewind
    // bypass below and simply restart tracking - no false hold.
    double global_timestamp_reader::enforce_monotonicity(double hw_time, double global_time)
    {
        static const double max_continuous_hw_gap_ms = 1000.;
        static const double max_monotonic_hold_ms = 100.;
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        double hw_gap = hw_time - _last_hw_time_ms;
        if (_last_hw_time_ms >= 0 && hw_gap >= 0 && hw_gap < max_continuous_hw_gap_ms &&
            global_time < _last_global_time_ms)
        {
            if (_last_global_time_ms - global_time > max_monotonic_hold_ms)
            {
                // Dropped further below the held value than the hold can bridge: the fit has re-based
                // (or the held value was bad). Accept the computed value - one backward step - and
                // resume tracking from it.
                LOG_DEBUG("global_timestamp_reader: releasing monotonicity hold: computed global time "
                          << global_time << " ms is more than " << max_monotonic_hold_ms
                          << " ms below held value " << _last_global_time_ms
                          << " ms; accepting one backward step");
            }
            else
            {
                global_time = _last_global_time_ms;
            }
        }
        _last_hw_time_ms = hw_time;
        _last_global_time_ms = global_time;
        return global_time;
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
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        _last_hw_time_ms = -1;
        _last_global_time_ms = 0;
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
