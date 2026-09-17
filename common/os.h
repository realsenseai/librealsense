// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once

#include "rect.h"

#include <vector>
#include <string>

struct GLFWmonitor;
struct GLFWwindow;

// Either OS specific or common helper functions
namespace rs2
{
    bool starts_with(const std::string& s, const std::string& prefix);
    bool ends_with(const std::string& s, const std::string& prefix);

    std::string truncate_string(const std::string& str, size_t width);

    bool open_url(const char* url);

    bool directory_exists(const char* dir);

    std::vector<std::string> split_string(std::string& input, char delim);

    // Helper function to get window rect from GLFW
    rect get_window_rect(GLFWwindow* window);

    // Helper function to get monitor rect from GLFW
    rect get_monitor_rect(GLFWmonitor* monitor);

    // Select appropriate scale factor based on the display
    // that most of the application is presented on
    int pick_scale_factor(GLFWwindow* window);

    std::string get_file_name(const std::string& path);

    // Wrapper for cross-platform dialog control
    enum file_dialog_mode {
        open_file       = (1 << 0),
        save_file       = (1 << 1),
    };

    const char* file_dialog_open(file_dialog_mode flags, const char* filters, const char* default_path, const char* default_name);

    // Encapsulate helper function to resolve linking
    int save_to_png(const char* filename,
        size_t pixel_width, size_t pixels_height, size_t bytes_per_pixel,
        const void* raster_data, size_t stride_bytes);

    std::string get_timestamped_file_name();

    std::string url_encode(const std::string &value);

    bool is_debug();
}
