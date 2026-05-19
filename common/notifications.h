// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#pragma once

#include <string>
#include <functional>
#include <vector>
#include <chrono>

#include "output-model.h"

namespace rs2
{
    class ux_window;

    class notification_data
    {
    public:
        notification_data(std::string description,
                          rs2_log_severity severity,
                          rs2_notification_category category);

        rs2_notification_category get_category() const;
        std::string get_description() const;
        double get_timestamp() const;
        rs2_log_severity get_severity() const;
    private:
        std::string _description;
        double _timestamp;
        rs2_log_severity _severity;
        rs2_notification_category _category;
    };

    struct notification_model : public std::enable_shared_from_this<notification_model>
    {
        notification_model();
        notification_model(const notification_data& n);

        template <class dst_type>
        bool is()
        {
            return dynamic_cast<const dst_type*>(this) != nullptr;
        }

        double get_age_in_ms(bool total = false) const;
        bool interacted() const;
        std::function<void()> draw(ux_window& win, int w, int y, 
            std::shared_ptr<notification_model>& selected, std::string& error_message);
        void draw_text(const char* msg, int x, int y, int h);
        virtual void set_color_scheme(float t) const;
        void unset_color_scheme() const;
        virtual int get_max_lifetime_ms() const;

        virtual int calc_height();
        virtual void draw_pre_effect(int x, int y) {}
        virtual void draw_content(ux_window& win, int x, int y, float t, std::string& error_message);
        virtual void draw_dismiss(ux_window& win, int x, int y);
        virtual void draw_expanded(ux_window& win, std::string& error_message) {}

        virtual void dismiss(bool snooze) { dismissed = true; snoozed = snooze; }

        std::string get_title();

        std::function<void()> custom_action;

        int count = 1;
        int height = 40;
        int index = 0;
        std::string message;
        double timestamp = 0.0;
        rs2_log_severity severity = RS2_LOG_SEVERITY_NONE;
        std::chrono::system_clock::time_point created_time;
        rs2_notification_category category = RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR;
        bool to_close = false; // true when user clicks on close notification

        int width = 320;
        int stack_offset = 4;
        int max_stack = 3;

        // Behaviour variables
        bool dismissed = false;
        bool expanded = false;
        bool visible = true;
        bool pinned = false;
        bool forced = false;
        bool snoozed = false;
        bool enable_dismiss = true;
        bool enable_expand = true;
        bool enable_click = false;
        bool enable_complex_dismiss = false;

        std::string delay_id = "";

        bool is_delayed() const;
        void delay(int days);
        void reset_delay();

        float last_x, last_y;
        bool animating = false;
        std::chrono::system_clock::time_point last_moved;
        std::chrono::system_clock::time_point last_interacted;

        single_consumer_queue<std::function<void()>> dispatch_queue;
        void invoke(std::function<void()> action);
    };

    class device_model;

    using invoker = std::function<void(std::function<void()>)>;

    class process_manager : public std::enable_shared_from_this<process_manager>
    {
    public:
        process_manager(std::string name)
            : _process_name(name) {}

        virtual ~process_manager() = default;

        void start(invoker invoke);
        int get_progress() const { return int(_progress); }
        bool done() const { return _done; }
        bool started() const { return _started; }
        bool failed() const { return _failed; }
        const std::string get_log() const;
        void reset();

        void check_error(std::string& error) { if (_failed) error = _last_error; }

        void log(std::string line);
        void fail(std::string error);

    protected:
        virtual void process_flow(
            std::function<void()> cleanup,
            invoker invoke) = 0;

        std::string _log;
        bool _started = false;
        bool _done = false;
        bool _failed = false;
        float _progress = 0;

        mutable std::mutex _log_lock;
        std::string _last_error;
        std::string _process_name;
    };

    struct progress_bar
    {
        void draw(ux_window& win, int w, int progress);

        float progress_speed = 5.f;
        std::chrono::system_clock::time_point last_progress_time;
        int last_progress = 0;
        float curr_progress_value = 0.f;
        float threshold_progress = 5.f;
    };

    struct process_notification_model : public notification_model
    {
        process_notification_model(std::shared_ptr<process_manager> manager)
            : update_manager(manager) {}

        void draw_progress_bar(ux_window& win, int w);

        void draw_pre_effect(int x, int y) override;

        std::shared_ptr<process_manager> update_manager = nullptr;
        int update_state = 0;
        int update_state_prev = 0;
        progress_bar _progress_bar;
    };

    struct version_upgrade_model : public process_notification_model
    {
        version_upgrade_model(int version);

        void set_color_scheme(float t) const override;
        void draw_content(ux_window& win, int x, int y, float t, std::string& error_message) override;
        int calc_height() override;
        int get_max_lifetime_ms() const override { return 40000; }

        int _version;
        bool _first = true;
    };

    struct metadata_warning_model : public notification_model
    {
        metadata_warning_model();

        void set_color_scheme(float t) const override;
        void draw_content(ux_window& win, int x, int y, float t, std::string& error_message) override;
        int calc_height() override { return 130; }
        int get_max_lifetime_ms() const override { return 40000; }
    };


    struct notifications_model
    {
        std::shared_ptr<notification_model> add_notification(const notification_data& n);
        std::shared_ptr<notification_model> add_notification(const notification_data& n,
            std::function<void()> custom_action,
            bool use_custom_action = true);
        void add_notification(std::shared_ptr<notification_model> model);
        
        // Check of a notification of type T is currently on the display queue.
        template <typename T>
        bool notification_type_is_displayed()
        {
           std::lock_guard<std::recursive_mutex> lock(m);
           return std::any_of(pending_notifications.cbegin(), pending_notifications.cend(), [](const std::shared_ptr<notification_model>& nm) {return nm->is<T>(); });
        }
        bool draw(ux_window& win, int w, int h, std::string& error_message);

        notifications_model() {}

        void add_log(std::string message, rs2_log_severity severity = RS2_LOG_SEVERITY_INFO )
        {
            output.add_log(severity, "", 0, message);
        }

        output_model output;

    private:
        std::vector<std::shared_ptr<notification_model>> pending_notifications;
        int index = 1;
        const int MAX_SIZE = 6;
        std::recursive_mutex m;

        std::shared_ptr<notification_model> selected;
    };

    struct sw_recommended_update_alert_model : public notification_model
    {
        sw_recommended_update_alert_model(const std::string & current_version, const std::string & recommended_version, const std::string &recommended_version_link);

        void set_color_scheme( float t ) const override;
        void draw_content(
            ux_window & win, int x, int y, float t, std::string & error_message ) override;
        int calc_height() override { return 150; }
        const std::string _current_version;
        const std::string _recommended_version;
        const std::string _recommended_version_link;
    };

    inline ImVec4 saturate(const ImVec4& a, float f)
    {
        return{ f * a.x, f * a.y, f * a.z, a.w };
    }

    inline ImVec4 alpha(const ImVec4& v, float a)
    {
        return{ v.x, v.y, v.z, a };
    }

    struct sw_update_up_to_date_model : public notification_model
    {
        sw_update_up_to_date_model();

        void set_color_scheme(float t) const override;
        void draw_content(
            ux_window& win, int x, int y, float t, std::string& error_message) override;
        int calc_height() override { return 65; }
    };    

    struct ucal_disclaimer_model : public notification_model
    {
        ucal_disclaimer_model();

        void draw_content(ux_window& win, int x, int y, float t, std::string& error_message) override;
        int calc_height() override { return 110; }
        int get_max_lifetime_ms() const override { return 15000; }
    };

    struct fl_cal_limitation_model : public notification_model
    {
        fl_cal_limitation_model();

        void draw_content(ux_window& win, int x, int y, float t, std::string& error_message) override;
        int calc_height() override { return 100; }
        int get_max_lifetime_ms() const override { return 10000; }
    };

    struct udev_warning_model : public notification_model
    {
        udev_warning_model();

        void draw_content(ux_window& win, int x, int y, float t, std::string& error_message) override;
        int calc_height() override { return 130; }
        int get_max_lifetime_ms() const override { return 20000; }
    };

    class export_manager : public process_manager
    {
    public:
        export_manager(const std::string& fname, std::unique_ptr<rs2::filter> exporter, frame data)
            : process_manager("Export"), _fname(fname), _exporter(std::move(exporter)), _data(data) {}

        std::string get_filename() const { return _fname; }
        frame get_data() const { return _data; }


    private:
        void process_flow(std::function<void()> cleanup,
            invoker invoke) override;

        std::string _fname;
        std::unique_ptr<rs2::filter> _exporter;
        frame _data;
    };

    struct export_notification_model : public process_notification_model
    {
        enum states
        {
            STATE_INITIAL_PROMPT = 0,
            STATE_IN_PROGRESS = 1,
            STATE_COMPLETE = 2,
            STATE_FAILED = 3,
        };

        export_manager& get_manager() {
            return *std::dynamic_pointer_cast<export_manager>(update_manager);
        }

        export_notification_model(std::shared_ptr<export_manager> manager);

        void set_color_scheme(float t) const override;
        void draw_content(ux_window& win, int x, int y, float t, std::string& error_message) override;
        int calc_height() override;
    };
}
