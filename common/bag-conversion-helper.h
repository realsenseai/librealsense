// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/rs.hpp>

#include <string>
#include <thread>
#include <atomic>

namespace rs2
{
    class ux_window;
    class viewer_model;

    class bag_conversion_helper
    {
    public:
        // If the file is a .bag, shows the conversion dialog and returns true
        bool show_dialog_if_needed(const std::string& file);

        // Draws the conversion dialog and polls the conversion thread.
        // Returns a file path to load (empty if nothing to load yet).
        std::string draw_and_poll(context& ctx,
                                  std::string& error_message,
                                  viewer_model& viewer_model,
                                  ux_window& window);

    private:
        std::string poll_result(std::string& error_message, viewer_model& viewer_model);
        std::string draw_prompt(context& ctx);
        void draw_progress();

        bool _show_dialog = false;
        std::string _pending_file;
        std::thread _thread;
        std::atomic<bool> _done{false};
        std::string _error;
        bool _skip_next = false;
    };
}
