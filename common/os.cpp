// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.
#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "os.h"
#include <rsutils/easylogging/easyloggingpp.h>

#include <thread>
#include <algorithm>
#include <regex>
#include <cmath>
#include <iomanip>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <tinyfiledialogs.h>

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#include <KnownFolders.h>
#include <shlobj.h>
#include <rsutils/os/hresult.h>
#endif

#if (defined(_WIN32) || defined(_WIN64))
#include "ShellAPI.h"
#endif

#if defined __linux__ || defined __APPLE__
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

#include <GLFW/glfw3.h>

namespace rs2
{
    // Use shortcuts for long names to avoid trimming of essential data
    std::string truncate_string(const std::string& str, size_t width)
    {
        if (str.length() > width)
        {
            std::ostringstream ss;
            ss << str.substr(0, width / 3) << "..." << str.substr(str.length() - width / 3);
            return ss.str().c_str();
        }
        return str;
    }

    bool is_valid_url( const std::string & url )
    {
        static const std::regex url_regex( R"(^(https?):\/\/([a-zA-Z0-9-]+\.)+[a-zA-Z]{2,}(:\d+)?(\/[^\s]*)?$)",
                                           std::regex::icase );

        return std::regex_match( url, url_regex );
    }

    void open_url(const char* url)
    {
        if( url && !is_valid_url( url ) )
        {
            throw std::invalid_argument( "The URL provided is not valid: " + std::string( url ) );
        }
#if (defined(_WIN32) || defined(_WIN64))
        if (reinterpret_cast<INT_PTR>(ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOW)) < 32)
            throw std::runtime_error("Failed opening URL");
#elif defined __linux__ || defined(__linux__)
        std::string command_name = "xdg-open ";
        std::string command = command_name + url;
        if (system(command.c_str()))
            throw std::runtime_error("Failed opening URL");
#elif __APPLE__
        std::string command_name = "open ";
        std::string command = command_name + url;
        if (system(command.c_str()))
            throw std::runtime_error("Failed opening URL");
#else
#pragma message ( "\nLibrealsense couldn't establish OS/Build environment. \
Some auxillary functionalities might be affected. Please report this message if encountered")
#endif
    }

    std::vector<std::string> split_string(std::string& input, char delim)
    {
        std::vector<std::string> result;
        auto e = input.end();
        auto i = input.begin();
        while (i != e) {
            i = find_if_not(i, e, [delim](char c) { return c == delim; });
            if (i == e) break;
            auto j = find(i, e, delim);
            result.emplace_back(i, j);
            i = j;
        }
        return result;
    }

    // Helper function to get window rect from GLFW
    rect get_window_rect(GLFWwindow* window)
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        int xpos, ypos;
        glfwGetWindowPos(window, &xpos, &ypos);

        return{ (float)xpos, (float)ypos,
            (float)width, (float)height };
    }

    // Helper function to get monitor rect from GLFW
    rect get_monitor_rect(GLFWmonitor* monitor)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        int xpos, ypos;
        glfwGetMonitorPos(monitor, &xpos, &ypos);

        return{ (float)xpos, (float)ypos,
            (float)mode->width, (float)mode->height };
    }

    // Select appropriate scale factor based on the display
    // that most of the application is presented on
    int pick_scale_factor(GLFWwindow* window)
    {
        auto window_rect = get_window_rect(window);
        int count;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        if (count == 0) return 1; // Not sure if possible, but better be safe

                                    // Find the monitor that covers most of the application pixels:
        GLFWmonitor* best = monitors[0];
        float best_area = 0.f;
        for (int i = 0; i < count; i++)
        {
            auto int_area = window_rect.intersection(
                get_monitor_rect(monitors[i])).area();
            if (int_area >= best_area)
            {
                best_area = int_area;
                best = monitors[i];
            }
        }

        int widthMM = 0;
        int heightMM = 0;
        glfwGetMonitorPhysicalSize(best, &widthMM, &heightMM);

        // This indicates that the monitor dimentions are unknown
        if (widthMM * heightMM == 0) return 1;

        // The actual calculation is somewhat arbitrary, but we are going for
        // about 1cm buttons, regardless of resultion
        // We discourage fractional scale factors
        float how_many_pixels_in_mm =
            get_monitor_rect(best).area() / (widthMM * heightMM);
        float scale = sqrt(how_many_pixels_in_mm) / 5.f;
        if (scale < 1.f) return 1;
        return (int)(floor(scale));
    }

    bool directory_exists(const char* dir)
    {
        struct stat info;

        if (stat(dir, &info ) != 0)
            return false;
        else if (info.st_mode & S_IFDIR) 
            return true;
        else
            return false;
    }

    const char* file_dialog_open(file_dialog_mode flags, const char* filters,
        const char* default_path, const char* default_name)
    {
        std::string def = "";
        if (default_path) def += default_path;
        if (default_name && default_path) def += "/";
        if (default_name) def += default_name;
        const char* def_ptr = nullptr;
        if (default_name || default_path)
            def_ptr = def.c_str();

        char const* const* aFilterPatterns = nullptr;
        char const* aSingleFilterDescription = nullptr;

        std::vector<std::string> filters_split;

        if (filters)
        {
            auto curr = filters;
            while (*curr != '\0')
            {
                auto end = curr + strlen(curr);
                filters_split.push_back({ curr, end });
                curr = end + 1;
            }
        }

        int filters_count = 0;
        std::vector<const char*> filter;

        if (filters_split.size() >= 1)
        {
            filters_count = static_cast<int>( filters_split.size() - 1 );

            // set description
            aSingleFilterDescription = filters_split[0].c_str();

            // fill filter pattern with extensions
            for(int i = 1; i < filters_split.size(); ++i)
            {
                filter.push_back(filters_split[i].c_str());
            }
            aFilterPatterns = filter.data();
        }

        if (flags == save_file)
        {
            return tinyfd_saveFileDialog("Save File", def_ptr, filters_count, aFilterPatterns, aSingleFilterDescription);
        }
        if (flags == open_file)
        {
            return tinyfd_openFileDialog("Open File", def_ptr, filters_count, aFilterPatterns, aSingleFilterDescription, 0);
        }
        return nullptr;
    }

    int save_to_png(const char* filename,
        size_t pixel_width, size_t pixels_height, size_t bytes_per_pixel,
        const void* raster_data, size_t stride_bytes)
    {
        return stbi_write_png(filename, (int)pixel_width, (int)pixels_height, (int)bytes_per_pixel, raster_data, (int)stride_bytes);
    }

    std::string get_file_name(const std::string& path)
    {
        std::string file_name;
        for (auto rit = path.rbegin(); rit != path.rend(); ++rit)
        {
            if (*rit == '\\' || *rit == '/')
                break;
            file_name += *rit;
        }
        std::reverse(file_name.begin(), file_name.end());
        return file_name;
    }

    std::string get_timestamped_file_name()
    {
        std::time_t now = std::time(NULL);
        std::tm * ptm = std::localtime(&now);
        char buffer[16];
        // Format: 20170529_205500
        std::strftime(buffer, 16, "%Y%m%d_%H%M%S", ptm);
        return buffer;
    }

    bool ends_with(const std::string& s, const std::string& suffix)
    {
        auto i = s.rbegin(), j = suffix.rbegin();
        for (; i != s.rend() && j != suffix.rend() && *i == *j;
            i++, j++);
        return j == suffix.rend();
    }

    bool starts_with(const std::string& s, const std::string& prefix)
    {
        auto i = s.begin(), j = prefix.begin();
        for (; i != s.end() && j != prefix.end() && *i == *j;
            i++, j++);
        return j == prefix.end();
    }

    bool is_debug()
    {
#ifndef NDEBUG
        return false;
#else
#ifndef _DEBUG
        return false;
#else
        return true;
#endif
#endif
    }

    std::string url_encode(const std::string &value) 
    {
        // based on https://stackoverflow.com/a/17708801
        using namespace std;

        ostringstream escaped;
        escaped.fill('0');
        escaped << hex;

        for (string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
            string::value_type c = (*i);

            // Keep alphanumeric and other accepted characters intact
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
                continue;
            }

            // Any other characters are percent-encoded
            escaped << uppercase;
            escaped << '%' << setw(2) << int((unsigned char)c);
            escaped << nouppercase;
            }

        return escaped.str();
    }
}
