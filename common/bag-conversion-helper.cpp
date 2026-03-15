// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "bag-conversion-helper.h"
#include "viewer.h"
#include "ux-window.h"
#include "os.h"

namespace rs2
{
    // Returns true if the file is a legacy .bag recording and the conversion dialog was activated.
    // When _skip_next is set (after "Play as-is" or failed conversion), bypasses the dialog once
    // so add_playback_device can load the .bag directly.
    bool bag_conversion_helper::show_dialog_if_needed(const std::string& file)
    {
        if (!ends_with(file, ".bag") && !ends_with(file, ".BAG"))
            return false;
        if (_skip_next)
        {
            _skip_next = false;
            return false;
        }
        _pending_file = file;
        _show_dialog = true;
        return true;
    }

    // Checks whether the background conversion thread has finished.
    // On success, returns the new .db3 path. On failure, shows an error and falls back
    // to the original .bag (with _skip_next so the dialog doesn't re-trigger).
    std::string bag_conversion_helper::poll_result(std::string& error_message,
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
            // Converter already appended .db3 to the output file
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

    // Draws the initial dialog content: deprecation message and Convert / Play as-is / Cancel buttons.
    // "Convert" launches the background thread; "Play as-is" returns the .bag path to load directly.
    std::string bag_conversion_helper::draw_prompt(context& ctx)
    {
        ImGui::Text("\nROS1 .bag recordings are deprecated and will be\n"
                    "removed in a future release.\n\n"
                    "It is recommended to convert to the new ROS2-compatible\n"
                    ".db3 format.\n");

        auto width = ImGui::GetWindowWidth();
        ImGui::Dummy(ImVec2(width / 5.f, 0));
        ImGui::SameLine();
        if (ImGui::Button("Convert", ImVec2(80, 30)))
        {
            _thread = std::thread([&ctx, this]()
            {
                try
                {
                    // Output is the stem — converter appends .db3
                    auto output = _pending_file.substr(0, _pending_file.size() - 4);
                    ctx.convert_bag_to_db3(_pending_file, output);
                }
                catch (const std::exception& ex)
                {
                    _error = ex.what();
                }
                _done = true;
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Play as-is", ImVec2(80, 30)))
        {
            ImGui::CloseCurrentPopup();
            _show_dialog = false;
            auto bag_file = _pending_file;
            _pending_file.clear();
            _skip_next = true;
            return bag_file;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 30)))
        {
            ImGui::CloseCurrentPopup();
            _show_dialog = false;
            _pending_file.clear();
        }
        return {};
    }

    // Draws the "converting" view: a spinning arc and centered status text.
    void bag_conversion_helper::draw_progress()
    {
        ImGui::Dummy(ImVec2(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2, 0));

        float radius = 20.0f;
        float thickness = 2.0f;
        float avail_w = ImGui::GetContentRegionAvail().x;
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 center(cursor.x + avail_w * 0.5f, cursor.y + radius + 5.0f);
        auto* draw_list = ImGui::GetWindowDrawList();

        draw_list->PathArcTo(center, radius, 0.0f, IM_PI * 2.0f, 30);
        draw_list->PathStroke(ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f)), false, thickness);

        float t = static_cast<float>(ImGui::GetTime()) * 3.0f;
        draw_list->PathArcTo(center, radius, t, t + 1.8f, 15);
        draw_list->PathStroke(ImGui::GetColorU32(ImVec4(0.36f, 0.51f, 0.71f, 1.0f)), false, thickness);

        ImGui::Dummy(ImVec2(0, radius * 2 + 15));

        ImGui::SetWindowFontScale(1.3f);
        const char* msg = "Converting .bag to .db3...";
        float text_w = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - text_w) * 0.5f);
        ImGui::Text("%s", msg);
        ImGui::NewLine();
    }

    // Called every frame from the render loop.
    // Checks if a background conversion finished, then draws the popup when active.
    // Returns a file path to load, or empty if nothing to load yet.
    std::string bag_conversion_helper::draw_and_poll(context& ctx,
                                                     std::string& error_message,
                                                     viewer_model& viewer_model,
                                                     ux_window& window)
    {
        auto file = poll_result(error_message, viewer_model);
        if (!file.empty())
            return file;

        if (!_show_dialog)
            return {};

        const char* popup_title = "Legacy Recording Format";
        ImGui::OpenPopup(popup_title);
        ImGui::SetNextWindowPos({ window.width() * 0.35f, window.height() * 0.35f });

        RsImGui_ScopePushFont(window.get_font());
        RsImGui_ScopePushStyleColor(ImGuiCol_Button, button_color);
        RsImGui_ScopePushStyleColor(ImGuiCol_ButtonHovered, sensor_header_light_blue);
        RsImGui_ScopePushStyleColor(ImGuiCol_ButtonActive, regular_blue);
        RsImGui_ScopePushStyleColor(ImGuiCol_TextSelectedBg, light_grey);
        RsImGui_ScopePushStyleColor(ImGuiCol_TitleBg, header_color);
        RsImGui_ScopePushStyleColor(ImGuiCol_PopupBg, sensor_bg);
        RsImGui_ScopePushStyleColor(ImGuiCol_BorderShadow, dark_grey);
        RsImGui_ScopePushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 10));

        if (ImGui::BeginPopup(popup_title))
        {
            {
                RsImGui_ScopePushStyleColor(ImGuiCol_Text, almost_white_bg);
                ImGui::SetWindowFontScale(1.3f);
                ImGui::Text("%s", popup_title);
            }
            {
                RsImGui_ScopePushStyleColor(ImGuiCol_Text, light_grey);
                ImGui::Separator();
                ImGui::SetWindowFontScale(1.1f);

                if (_thread.joinable())
                    draw_progress();
                else
                    file = draw_prompt(ctx);
            }
            ImGui::EndPopup();
        }
        return file;
    }
}
