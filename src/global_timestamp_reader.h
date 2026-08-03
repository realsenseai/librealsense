// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once

#include "sensor.h"
#include "error-handling.h"
#include "option.h"
#include <deque>

namespace librealsense
{
    class LRS_EXTENSION_API global_time_option : public bool_option
    {
    public:
        global_time_option() {}
        // TODO: expose this outwards
        const char* get_description() const override { return "Enable/Disable global timestamp"; }
    };

    class CSample
    {
    public:
        CSample(double x, double y) :
            _x(x), _y(y) {};
        CSample& operator-=(const CSample& other);
        CSample& operator+=(const CSample& other);

    public:
        double _x;
        double _y;
    };

    class CLinearCoefficients
    {
    public:
        CLinearCoefficients(unsigned int buffer_size);
        void reset();
        void add_value(CSample val);
        void add_const_y_coefs(double dy);
        void refit_from_samples(const std::deque<CSample>& samples);
        bool update_samples_base(double x);
        double to_fit_domain(double x) const;
        static double align_to_epoch(double x, double anchor);
        void update_last_sample_time(double x);
        double calc_value(double x) const;
        bool is_full() const;

    private:
        void calc_linear_coefs();
        void get_a_b(double x, double& a, double& b) const;
        static bool theil_sen_fit(const std::deque<CSample>& values, const CSample& base_sample, double& a, double& b);

    private:
        unsigned int _buffer_size;
        std::deque<CSample> _last_values;
        CSample _base_sample;
        double _prev_a, _prev_b;    //Linear regression coeffitions - previously used values.
        double _dest_a, _dest_b;    //Linear regression coeffitions - recently calculated.
        double _prev_time, _time_span_ms;
        double _last_request_time;
    };

    class global_time_interface;

    class time_diff_keeper
    {
    public:
        explicit time_diff_keeper(global_time_interface* dev, const unsigned int sampling_interval_ms);
        void start();   // must be called AFTER ALL initializations of _hw_monitor.
        void stop();
        ~time_diff_keeper();

        double get_system_hw_time(double crnt_hw_time, bool& is_ready);
        void set_enabling_opt(std::shared_ptr<global_time_option> en_opt){ _option_is_enabled=en_opt; }
        bool is_enabled() const { return _option_is_enabled? _option_is_enabled->is_true() : false; }

    private:
        bool update_diff_time();
        void polling(dispatcher::cancellable_timer cancellable_timer);

    private:
        global_time_interface* _device;
        unsigned int _poll_intervals_ms;
        int             _users_count;
        std::shared_ptr<global_time_option> _option_is_enabled;
        active_object<> _active_object;
        mutable std::recursive_mutex _read_mtx; // Watch only 1 reader at a time.
        mutable std::recursive_mutex _enable_mtx; // Watch only 1 start/stop operation at a time.
        CLinearCoefficients _coefs;
        double _min_command_delay;
        unsigned int _rejections_in_row; // Consecutive samples rejected for excessive command delay.
        unsigned int _innovation_rejections_in_row; // Consecutive samples rejected for excessive innovation (value vs. fit prediction).
        std::deque<CSample> _rejected_samples;   // Last re_fit_window (x, y) pairs rejected on value; refit source if rejections persist.
        bool _is_ready;
        bool _first_sample_dropped; // the first (often slow) clock read after start is dropped
    };

    class global_timestamp_reader : public frame_timestamp_reader
    {
    public:
        global_timestamp_reader(std::unique_ptr<frame_timestamp_reader> device_timestamp_reader, 
                                std::shared_ptr<time_diff_keeper> timediff,
                                std::shared_ptr<global_time_option>);

        rs2_time_t get_frame_timestamp(const std::shared_ptr<frame_interface>& frame) override;
        unsigned long long get_frame_counter(const std::shared_ptr<frame_interface>& frame) const override;
        rs2_timestamp_domain get_frame_timestamp_domain(const std::shared_ptr<frame_interface>& frame) const override;
        void reset() override;

    private:
        double enforce_monotonicity(double hw_time, double global_time);

    private:
        std::unique_ptr<frame_timestamp_reader> _device_timestamp_reader;
        std::weak_ptr<time_diff_keeper> _time_diff_keeper;
        mutable std::recursive_mutex _mtx;
        std::shared_ptr<global_time_option> _option_is_enabled;
        bool _ts_is_ready;
        double _last_hw_time_ms;        // HW timestamp of the last frame (-1 if none yet).
        double _last_global_time_ms;    // Global timestamp given to the last frame.
    };

    class global_time_interface
    {
    protected:
        std::shared_ptr<time_diff_keeper> _tf_keeper;

    public:
        global_time_interface();
        ~global_time_interface() { _tf_keeper.reset(); }
        void enable_time_diff_keeper(bool is_enable);
        virtual double get_device_time_ms() = 0; // Returns time in miliseconds.
    };

    MAP_EXTENSION(RS2_EXTENSION_GLOBAL_TIMER, librealsense::global_time_interface);

}
