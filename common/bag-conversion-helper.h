// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/rs.hpp>

#include "notifications.h"

#include <string>
#include <thread>
#include <atomic>

namespace rs2
{
    class ux_window;
    class viewer_model;

    class bag_conversion_helper : public std::enable_shared_from_this<bag_conversion_helper>
    {
    public:
        ~bag_conversion_helper()
        {
            _stop_requested = true;
            if (_thread.joinable())
                _thread.join();
        }

        // If the file is a .bag, activates the conversion dialog and returns true
        bool show_dialog_if_needed(const std::string& file);

        // True while a background conversion is in progress
        bool is_converting() const { return _thread.joinable(); }

        // True when the dialog should be displayed
        bool should_show_dialog() const { return _show_dialog; }

        // Check if background conversion finished; returns file to load (empty if not done)
        std::string poll_completion(std::string& error_message, viewer_model& viewer_model);

        // Draw the conversion prompt popup. Returns file to load if user chose "Play as-is".
        std::string draw_prompt(context& ctx, ux_window& window);

        // Draw the progress bar popup.
        void draw_progress(ux_window& window);

    private:

        bool _show_dialog = false;
        std::string _pending_file;
        std::thread _thread;
        std::atomic<bool> _stop_requested{false};
        std::atomic<bool> _done{false};
        std::atomic<float> _progress{0.0f};
        progress_bar _progress_bar;
        std::string _error;
        bool _skip_next = false;
    };
}
