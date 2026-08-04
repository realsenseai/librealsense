// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <librealsense2/rs.hpp>

#include <realsense_imgui.h>
#include "model-views.h"
#include "subdevice-model.h"
#include "stream-model.h"
#include "updates-model.h"
#include "notifications.h"
#include "fw-update-helper.h"
#include "on-chip-calib.h"
#include "viewer.h"
#include "post-processing-filters-list.h"
#include "post-processing-block-model.h"
#include <imgui_internal.h>
#include <time.h>
#include "ux-window.h"

#include "imgui-fonts-karla.hpp"
#include "imgui-fonts-fontawesome.hpp"
#include "imgui-fonts-monofont.hpp"

#include "os.h"

#include "metadata-helper.h"
#include "calibration-model.h"
#include "http/http-downloader.h"

#include <thread>
#include <algorithm>
#include <regex>
#include <cmath>

#include "opengl3.h"

ImVec4 flip(const ImVec4& c)
{
    return{ c.y, c.x, c.z, c.w };
}

ImVec4 from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool consistent_color)
{
    auto res = ImVec4(r / (float)255, g / (float)255, b / (float)255, a / (float)255);
#ifdef FLIP_COLOR_SCHEME
    if (!consistent_color) return flip(res);
#endif
    return res;
}

ImVec4 operator+(const ImVec4& c, float v)
{
    return ImVec4(
        std::max(0.f, std::min(1.f, c.x + v)),
        std::max(0.f, std::min(1.f, c.y + v)),
        std::max(0.f, std::min(1.f, c.z + v)),
        std::max(0.f, std::min(1.f, c.w))
    );
}

namespace rs2
{
    std::vector<uint8_t> bytes_from_bin_file(const std::string& filename)
    {
        std::ifstream file(filename.c_str(), std::ios::binary);
        if (!file.good())
            throw std::runtime_error( rsutils::string::from() << "Invalid binary file specified " << filename
                                                              << " verify the source path and location permissions" );

        // Determine the file length
        file.seekg(0, std::ios_base::end);
        std::size_t size = file.tellg();
        if (!size)
            throw std::runtime_error( rsutils::string::from()
                                      << "Invalid binary file " << filename << " provided  - zero-size " );
        file.seekg(0, std::ios_base::beg);

        // Create a vector to store the data
        std::vector<uint8_t> v(size);

        // Load the data
        file.read((char*)&v[0], size);

        return v;
    }

    // Flush binary stream to file, override previous if exists
    void bin_file_from_bytes(const std::string& filename, const std::vector<uint8_t> bytes)
    {
        std::ofstream file(filename, std::ios::binary | std::ios::trunc);
        if (!file.good())
            throw std::runtime_error( rsutils::string::from() << "Invalid binary file specified " << filename
                                                              << " verify the target path and location permissions" );
        file.write((char*)bytes.data(), bytes.size());
    }

    void hyperlink(ux_window& window, const char* title, const char* link)
    {
        if (ImGui::Button(title))
        {
            open_url(link);
        }
        if (ImGui::IsItemHovered())
        {
            window.link_hovered();
        }
    }

    std::tuple<uint8_t, uint8_t, uint8_t> get_texcolor(video_frame texture, texture_coordinate texcoords)
    {
        const int w = texture.get_width(), h = texture.get_height();
        int x = std::min(std::max(int(texcoords.u*w + .5f), 0), w - 1);
        int y = std::min(std::max(int(texcoords.v*h + .5f), 0), h - 1);
        int idx = x * texture.get_bytes_per_pixel() + y * texture.get_stride_in_bytes();
        const auto texture_data = reinterpret_cast<const uint8_t*>(texture.get_data());
        return std::tuple<uint8_t, uint8_t, uint8_t>(
            texture_data[idx], texture_data[idx + 1], texture_data[idx + 2]);
    }

    void export_frame(const std::string& fname, std::unique_ptr<rs2::filter> exporter,
        notifications_model& ns, frame data, bool notify)
    {
        auto manager = std::make_shared<export_manager>(fname, std::move(exporter), data);

        auto n = std::make_shared<export_notification_model>(manager);
        ns.add_notification(n);
        n->forced = true;

        auto invoke = [n](std::function<void()> action) {
            n->invoke(action);
        };
        manager->start(invoke);
    }

    bool save_frame_raw_data(const std::string& filename, rs2::frame frame)
    {
        bool ret = false;
        auto image = frame.as<video_frame>();
        if (image)
        {
            std::ofstream outfile(filename.data(), std::ofstream::binary);
            outfile.write(static_cast<const char*>(image.get_data()), image.get_height()*image.get_stride_in_bytes());

            outfile.close();
            ret = true;
        }

        return ret;
    }

    bool frame_metadata_to_csv(const std::string& filename, rs2::frame frame)
    {
        bool ret = false;
        auto image = frame.as<video_frame>();
        if (image)
        {
            std::ofstream csv(filename);

            auto profile = image.get_profile();
            csv << "Frame Info: " << std::endl << "Type," << profile.stream_name() << std::endl;
            csv << "Format," << rs2_format_to_string(profile.format()) << std::endl;
            csv << "Frame Number," << image.get_frame_number() << std::endl;
            csv << "Timestamp (ms)," << std::fixed << std::setprecision(2) << image.get_timestamp() << std::endl;
            csv << "Resolution x," << (int)image.get_width() << std::endl;
            csv << "Resolution y," << (int)image.get_height() << std::endl;
            csv << "Bytes per pixel," << (int)image.get_bytes_per_pixel() << std::endl;

            if (auto vsp = profile.as<video_stream_profile>())
            {
                auto intrinsics = vsp.get_intrinsics();
                csv << std::endl << "Intrinsic:," << std::fixed << std::setprecision(6) << std::endl;
                csv << "Fx," << intrinsics.fx << std::endl;
                csv << "Fy," << intrinsics.fy << std::endl;
                csv << "PPx," << intrinsics.ppx << std::endl;
                csv << "PPy," << intrinsics.ppy << std::endl;
                csv << "Distorsion," << rs2_distortion_to_string(intrinsics.model) << std::endl;
            }

            ret = true;
        }

        return ret;
    }

    bool motion_data_to_csv( const std::string & filename, rs2::frame frame )
    {
        bool ret = false;
        if( auto motion = frame.as< motion_frame >() )
        {
            std::string units;
            if( motion.get_profile().stream_type() == RS2_STREAM_GYRO )
                units = "( deg/sec )";
            else
                units = "( m/sec^2 )";
            auto axes = motion.get_motion_data();
            std::ofstream csv( filename );

            auto profile = frame.get_profile();
            csv << "Frame Info: " << std::endl << "Type," << profile.stream_name() << std::endl;
            csv << "Format," << rs2_format_to_string( profile.format() ) << std::endl;
            csv << "Frame Number," << frame.get_frame_number() << std::endl;
            csv << "Timestamp (ms)," << std::fixed << std::setprecision( 2 )
                << frame.get_timestamp() << std::endl;
            csv << std::setprecision( 7 ) << "Axes" << units << ", " << axes << std::endl;

            ret = true;
        }

        return ret;
    }

    bool pose_data_to_csv( const std::string & filename, rs2::frame frame )
    {
        bool ret = false;
        if( auto pose = frame.as< pose_frame >() )
        {
            auto pose_data = pose.get_pose_data();
            std::ofstream csv( filename );

            auto profile = frame.get_profile();
            csv << "Frame Info: " << std::endl << "Type," << profile.stream_name() << std::endl;
            csv << "Format," << rs2_format_to_string( profile.format() ) << std::endl;
            csv << "Frame Number," << frame.get_frame_number() << std::endl;
            csv << "Timestamp (ms)," << std::fixed << std::setprecision( 2 )
                << frame.get_timestamp() << std::endl;
            csv << std::setprecision( 7 ) << "Acceleration( meters/sec^2 ), "
                << pose_data.acceleration << std::endl;
            csv << std::setprecision( 7 ) << "Angular_acceleration( radians/sec^2 ), "
                << pose_data.angular_acceleration << std::endl;
            csv << std::setprecision( 7 ) << "Angular_velocity( radians/sec ), "
                << pose_data.angular_velocity << std::endl;
            csv << std::setprecision( 7 )
                << "Mapper_confidence( 0x0 - Failed 0x1 - Low 0x2 - Medium 0x3 - High ), "
                << pose_data.mapper_confidence << std::endl;
            csv << std::setprecision( 7 )
                << "Rotation( quaternion rotation (relative to initial position) ), "
                << pose_data.rotation << std::endl;
            csv << std::setprecision( 7 )
                << "Tracker_confidence( 0x0 - Failed 0x1 - Low 0x2 - Medium 0x3 - High ), "
                << pose_data.tracker_confidence << std::endl;
            csv << std::setprecision( 7 ) << "Translation( meters ), " << pose_data.translation
                << std::endl;
            csv << std::setprecision( 7 ) << "Velocity( meters/sec ), " << pose_data.velocity
                << std::endl;

            ret = true;
        }

        return ret;
    }

    std::vector<std::string> get_device_info(const device& dev, bool include_location)
    {
        std::vector<std::string> res;
        for (auto i = 0; i < RS2_CAMERA_INFO_COUNT; i++)
        {
            auto info = static_cast<rs2_camera_info>(i);

            // When camera is being reset, either because of "hardware reset"
            // or because of switch into advanced mode,
            // we don't want to capture the info that is about to change
            if ((info == RS2_CAMERA_INFO_PHYSICAL_PORT ||
                info == RS2_CAMERA_INFO_ADVANCED_MODE)
                && !include_location) continue;

            if (dev.supports(info))
            {
                auto value = dev.get_info(info);
                res.push_back(value);
            }
        }
        return res;
    }

    bool yes_no_dialog(const std::string& title, const std::string& message_text, bool& approved, ux_window& window, const std::string& error_message, bool disabled, const std::string& disabled_reason)
    {
        RsImGui_ScopePushFont(window.get_font());
        RsImGui_ScopePushStyleColor(ImGuiCol_Button, button_color);
        RsImGui_ScopePushStyleColor(ImGuiCol_ButtonHovered, sensor_header_light_blue); //TODO: Change color?
        RsImGui_ScopePushStyleColor(ImGuiCol_ButtonActive, regular_blue); //TODO: Change color?
        RsImGui_ScopePushStyleColor(ImGuiCol_TextSelectedBg, light_grey);
        RsImGui_ScopePushStyleColor(ImGuiCol_TitleBg, header_color);
        RsImGui_ScopePushStyleColor(ImGuiCol_PopupBg, sensor_bg);
        RsImGui_ScopePushStyleColor(ImGuiCol_BorderShadow, dark_grey);
        RsImGui_ScopePushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 10));
        auto clicked = false;

        ImGui::OpenPopup(title.c_str());
        ImGui::SetNextWindowPos( {window.width() * 0.35f, window.height() * 0.35f });
        if (ImGui::BeginPopup(title.c_str()))
        {
            {
                RsImGui_ScopePushStyleColor(ImGuiCol_Text, almost_white_bg);

                ImGui::SetWindowFontScale(1.3f);
                ImGui::Text("%s", title.c_str());
            }
            {
                RsImGui_ScopePushStyleColor(ImGuiCol_Text, light_grey);
                ImGui::Separator();
                ImGui::SetWindowFontScale(1.1f);
                ImGui::Text("\n%s\n", message_text.c_str());

                if (!disabled)
                {
                    ImGui::SameLine();
                    auto width = ImGui::GetWindowWidth();
                    ImGui::Dummy(ImVec2(0, 0));
                    ImGui::Dummy(ImVec2(width / 3.f, 0));
                    ImGui::SameLine();
                    if (ImGui::Button("Yes", ImVec2(60, 30)))
                    {
                        ImGui::CloseCurrentPopup();
                        approved = true;
                        clicked = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("No", ImVec2(60, 30)))
                    {
                        ImGui::CloseCurrentPopup();
                        approved = false;
                        clicked = true;
                    }
                }
                else
                {
                    ImGui::NewLine();
                    {
                        RsImGui_ScopePushStyleColor(ImGuiCol_Text, red);
                        ImGui::Text("%s\n\n", disabled_reason.c_str());
                    }
                    auto window_width = ImGui::GetWindowWidth();
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + window_width / 2.f - 30.f - ImGui::GetStyle().WindowPadding.x);
                    if (ImGui::Button("Close", ImVec2(60, 30)))
                    {
                        ImGui::CloseCurrentPopup();
                        approved = false;
                        clicked = true;
                    }
                }
            }
            ImGui::EndPopup();
        }
        return clicked;
    }

    // Create a process windows with process details from the caller,
    // and close button activated by the caller
    bool status_dialog(const std::string& title, const std::string& process_topic_text, const std::string& process_status_text , bool enable_close, ux_window& window)
    {
        RsImGui_ScopePushFont(window.get_font());
        RsImGui_ScopePushStyleColor(ImGuiCol_Button, button_color);
        RsImGui_ScopePushStyleColor(ImGuiCol_ButtonHovered, sensor_header_light_blue); //TODO: Change color?
        RsImGui_ScopePushStyleColor(ImGuiCol_ButtonActive, regular_blue); //TODO: Change color?
        RsImGui_ScopePushStyleColor(ImGuiCol_Text, light_grey);
        RsImGui_ScopePushStyleColor(ImGuiCol_TextSelectedBg, light_grey);
        RsImGui_ScopePushStyleColor(ImGuiCol_TitleBg, header_color);
        RsImGui_ScopePushStyleColor(ImGuiCol_PopupBg, sensor_bg);
        RsImGui_ScopePushStyleColor(ImGuiCol_BorderShadow, dark_grey);
        RsImGui_ScopePushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 10));
        auto close_clicked = false;

        ImGui::OpenPopup(title.c_str());
        ImGui::SetNextWindowPos({ window.width() * 0.35f, window.height() * 0.35f });
        if (ImGui::BeginPopup(title.c_str()))
        {
            {
                RsImGui_ScopePushStyleColor(ImGuiCol_Text, almost_white_bg);

                ImGui::SetWindowFontScale(1.3f);
                ImGui::Text("%s", title.c_str());
            }
            {

                ImGui::Separator();
                ImGui::SetWindowFontScale(1.1f);

                ImGui::NewLine();
                ImGui::Text("%s", process_topic_text.c_str());
                ImGui::NewLine();

                auto window_width = ImGui::GetWindowWidth();
                auto process_status_text_size = ImGui::CalcTextSize(process_status_text.c_str()).x + ImGui::CalcTextSize("Status: ").x + ImGui::GetStyle().WindowPadding.x;
                auto future_window_width = std::max(process_status_text_size, window_width);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + future_window_width / 2.f - process_status_text_size / 2.f);

                ImGui::Text("Status: %s", process_status_text.c_str());
                ImGui::NewLine();
                window_width = ImGui::GetWindowWidth();

                if (enable_close)
                {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + window_width / 2.f - 50.f); // 50 = 30 (button size) + 20 (padding)
                    if (ImGui::Button("Close", ImVec2(60, 30)))
                    {
                        ImGui::CloseCurrentPopup();
                        close_clicked = true;
                    }
                }
            }
            ImGui::EndPopup();
        }
        return close_clicked;
    }

}
