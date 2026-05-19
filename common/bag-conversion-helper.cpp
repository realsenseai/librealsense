// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "bag-conversion-helper.h"
#include "viewer.h"
#include "ux-window.h"
#include "os.h"

namespace rs2
{
    bool bag_conversion_helper::show_dialog_if_needed(const std::string& file)
    {
        if (ends_with(rsutils::string::to_lower(file), ".db3"))
            return false;
#ifndef BUILD_ROSBAG2
        return false;  // No conversion available — load .bag directly
#endif
        if (is_converting())
            return true;  // block loading while conversion is in progress
        if (_skip_next) // Will happen in case of conversion failure, or 'Play as-is' selected
        {
            _skip_next = false;
            return false;
        }
        _pending_file = file;
        _show_dialog = true;
        return true;
    }

    std::string bag_conversion_helper::poll_completion(std::string& error_message,
                                                       viewer_model& viewer_model)
    {
        if (!_done)
            return {};

        _thread.join();
        _done = false;
        _show_dialog = false;

        std::string load_file;
        if (_error.empty())
        {
            viewer_model.not_model->add_log("Converted " + _pending_file + " to .db3 format");
            load_file = _pending_file;
            load_file.replace(load_file.size() - 4, 4, ".db3");
        }
        else
        {
            error_message = rsutils::string::from()
                << "Failed to convert " << _pending_file << ": " << _error
                << ". Loading original .bag file.";
            load_file = _pending_file;
            _error.clear();
            _skip_next = true;
        }

        _pending_file.clear();
        return load_file;
    }

    static bool begin_dialog_popup(ux_window& window)
    {
        auto popup_title = "Legacy Recording Format";
        ImGui::OpenPopup(popup_title);
        ImGui::SetNextWindowPos({ window.width() * 0.35f, window.height() * 0.35f });
        ImGui::SetNextWindowSizeConstraints({ 450, 0 }, { 450, 1000 });

        ImGui::PushFont(window.get_font());
        ImGui::PushStyleColor(ImGuiCol_Button, button_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sensor_header_light_blue);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, regular_blue);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, light_grey);
        ImGui::PushStyleColor(ImGuiCol_TitleBg, header_color);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_BorderShadow, dark_grey);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 10));

        if (!ImGui::BeginPopup(popup_title))
        {
            ImGui::PopStyleVar(1);
            ImGui::PopStyleColor(7);
            ImGui::PopFont();
            return false;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, almost_white_bg);
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("%s", popup_title);
        ImGui::PopStyleColor(1);

        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::Separator();
        ImGui::SetWindowFontScale(1.1f);
        return true;
    }

    static void end_dialog_popup()
    {
        ImGui::PopStyleColor(1); // light_grey text
        ImGui::EndPopup();
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(7);
        ImGui::PopFont();
    }

    std::string bag_conversion_helper::draw_prompt(context& ctx, ux_window& window)
    {
        std::string file;
        if (!begin_dialog_popup(window))
            return file;

        ImGui::Text("\nROS1 .bag recordings are deprecated and will be\n"
                    "removed in a future release.\n\n"
                    "It is recommended to convert to the new ROS2-compatible\n"
                    ".db3 format.\n");

        auto width = ImGui::GetWindowWidth();
        ImGui::Dummy(ImVec2(width / 5.f, 0));
        ImGui::SameLine();
        if (ImGui::Button("Convert", ImVec2(80, 30)))
        {
            auto input = _pending_file;
            _progress = 0.0f;
            _progress_bar = progress_bar();
            _progress_bar.threshold_progress = 0.f;
            _progress_bar.last_progress_time = std::chrono::system_clock::now();
            std::weak_ptr<bag_conversion_helper> weak = shared_from_this();
            _stop_requested = false;
            _thread = std::thread([ctx, input, weak]() mutable
            {
                try
                {
                    auto output = input.substr(0, input.size() - 4) + ".db3";
                    ctx.convert_bag_to_db3(input, output, [weak](float p) {
                        if (auto self = weak.lock())
                        {
                            if (self->_stop_requested)
                                throw std::runtime_error("conversion cancelled");
                            self->_progress = p;
                        }
                    });
                }
                catch (const std::exception& ex)
                {
                    if (auto self = weak.lock())
                        if (!self->_stop_requested)
                            self->_error = ex.what();
                }
                if (auto self = weak.lock())
                    if (!self->_stop_requested)
                        self->_done = true;
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Play as-is", ImVec2(80, 30)))
        {
            ImGui::CloseCurrentPopup();
            _show_dialog = false;
            file = _pending_file;
            _pending_file.clear();
            _skip_next = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 30)))
        {
            ImGui::CloseCurrentPopup();
            _show_dialog = false;
            _pending_file.clear();
        }

        end_dialog_popup();
        return file;
    }

    void bag_conversion_helper::draw_progress(ux_window& window)
    {
        if (!begin_dialog_popup(window))
            return;

        int progress = int(_progress.load() * 100);
        const char* msg = progress >= 100
            ? "Conversion complete, loading recording..."
            : "Converting .bag to .db3...";
        float text_w = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - text_w) * 0.5f);
        ImGui::Text("%s", msg);

        ImGui::NewLine();
        int bar_width = int(ImGui::GetContentRegionAvail().x);
        _progress_bar.draw(window, bar_width, progress);
        ImGui::NewLine();

        end_dialog_popup();
    }
}
