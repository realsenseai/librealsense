// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/rs.hpp>

#include "rect.h"
#include "animated.h"

#include <functional>
#include <string>
#include <map>

#include "rendering.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <rsutils/concurrency/concurrency.h>
#include <rsutils/concurrency/shared-data-access.h>

namespace rs2
{
    class device_model;
    class ux_window;

    class stream_dashboard
    {
    public:
        stream_dashboard(std::string name, int size) : q(size), name(name), shared_data(m), t([this](){ thread_function(); }) {}
        virtual ~stream_dashboard()
        {
            stop = true;
            t.join();
        }

        std::string get_name() const { return name; }

        void add_frame(rs2::frame f) { q.enqueue(f); }

        virtual void draw(ux_window& win, rect r) = 0;

        virtual int get_height() const { return 150; }

        virtual void clear(bool full = false) {}

        void close() { to_close = true; }
        bool closing() const { return to_close; }

    protected:
        virtual void process_frame(rs2::frame f) = 0;

        void add_point(float x, float y) { xy.push_back(std::make_pair(x, y)); }

        void draw_dashboard(ux_window& win, rect& r);

        rsutils::concurrency::shared_data_access shared_data;

    private:
        void thread_function()
        {
            while(!stop)
            {
                rs2::frame f;
                if (q.try_wait_for_frame(&f, 100))
                    process_frame(f);
            }
        }
        std::string name;
        rs2::frame_queue q;
        std::mutex m;
        std::atomic<int> stop { false };
        std::thread t;
        std::vector<std::pair<float, float>> xy;
        bool to_close = false;
    };

    class frame_drops_dashboard : public stream_dashboard
    {
    public:
        frame_drops_dashboard(std::string name, int* frame_drop_count, int* total)
            : stream_dashboard(name, 30),
              last_time(glfwGetTime()), frame_drop_count(frame_drop_count), total(total)
        {
            clear(true);
        }

        void process_frame(rs2::frame f) override;
        void draw(ux_window& win, rect r) override;
        int get_height() const override;

        void clear(bool full) override;

    private:
        std::map<int, double> stream_to_time;
        int drops = 0;
        double last_time;
        std::deque<int> drops_history;
        int *frame_drop_count, *total;
        int counter = 0;
        int method = 0;
    };

    class output_model
    {
    public:
        struct log_entry
        {
            std::string line = "";
            std::string filename = "";
            rs2_log_severity severity = RS2_LOG_SEVERITY_FATAL;
            int line_number = 0;
            double time_added = 0.0;
            std::string timestamp = "";
            bool selected = false;
        };

        void update_dashboards(rs2::frame f);

        output_model();
        ~output_model();

        void add_log(rs2_log_severity severity, std::string filename, int line_number, std::string line);

        void draw(ux_window& win, rect view_rect, std::vector<std::unique_ptr<device_model>> &  device_models);

        int get_output_height() const { return default_log_h; }
        int get_dashboard_width() const { return default_dashboard_w; }

        void run_command(std::string command, std::vector<std::unique_ptr<device_model>> & device_models);
        bool user_defined_command(std::string command, std::vector<std::unique_ptr<device_model>> & device_models);


    private:
        void open(ux_window& win);

        void foreach_log(std::function<void(log_entry& line)> action);
        bool round_indicator(ux_window& win, std::string icon, int count,
            ImVec4 color, std::string tooltip, bool& highlighted, std::string suffix = "");

        bool new_log = false;
        std::recursive_mutex m;

        single_consumer_queue<log_entry> incoming_log_queue;
        std::deque<log_entry> notification_logs;

        animated<int> default_log_h { 36 };
        bool is_output_open = true;
        animated< int > default_dashboard_w { 0 };
        bool is_dashboard_open = true;

        bool enable_firmware_logs = false;

        bool errors_selected = false;
        bool warnings_selected = false;
        bool info_selected = false;

        bool errors_highlighted = false;
        bool warnings_highlighted = false;
        bool info_highlighted = false;
        bool drops_highlighted = false;

        int number_of_errors = 0;
        int number_of_warnings = 0;
        int number_of_info = 0;
        int number_of_drops = 0;
        int total_frames = 0;

        animated<int> search_width { 0, std::chrono::milliseconds(400) };
        bool search_open = false;
        bool set_number_open = false;
        size_t max_notifications_kept = 1000;

        std::deque<std::string> autocomplete;
        std::string autocomplete_prefix;

        std::mutex devices_mutex;
        std::vector<rs2::device> devices;

        static constexpr int MAX_HISTORY_SIZE = 100;

        std::string search_line { "" };
        std::string command_line { "" };
        std::string history_draft { "" };
        std::deque<std::string> commands_histroy;
        int history_offset = -1;
        bool command_focus = true;

        std::vector<std::shared_ptr<stream_dashboard>> dashboards;
        std::map<std::string, std::function<std::shared_ptr<stream_dashboard>(std::string)>> available_dashboards;

        std::atomic<int> to_stop { 0 };
        std::thread fw_logger;

        void thread_loop();
        bool cycle_autocomplete(bool forward);
    };
}
