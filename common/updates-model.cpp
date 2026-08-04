// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

#include <glad/glad.h>
#include "updates-model.h"
#include "device-model.h"
#include "ux-window.h"
#include "os.h"
#include <stb_image.h>
#include <realsense_imgui.h>
#include "http/http-downloader.h"
#include <rsutils/easylogging/easyloggingpp.h>

using namespace rs2;
using namespace sw_update;
using namespace http;

void updates_model::draw(std::shared_ptr<notifications_model> not_model, ux_window& window, std::string& error_message)
{
    // Protect resources
    static std::vector<update_profile_model> updates_copy;
    {
        std::lock_guard<std::mutex> lock(_lock);
        updates_copy = _updates;
    }

    const auto window_name = "Updates Window";

    //Main window pop up only if essential updates exists
    if (updates_copy.size())
    {
        ImGui::OpenPopup(window_name);
    }

    position_params positions;

    positions.w = window.width()  * 0.6f;
    positions.h = 600;
    positions.x0 = window.width()  * 0.2f;
    positions.y0 = std::max(window.height() - positions.h, 0.f) / 2;
    ImGui::SetNextWindowPos({ positions.x0, positions.y0 });
    ImGui::SetNextWindowSize({ positions.w, positions.h });

    auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleColor(ImGuiCol_PopupBg, sensor_bg);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
    ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 1);

    if (ImGui::BeginPopupModal(window_name, nullptr, flags))
    {

        std::string title_message = "SOFTWARE UPDATES";
        auto title_size = ImGui::CalcTextSize(title_message.c_str());
        ImGui::SetCursorPosX(positions.w / 2 - title_size.x / 2);
        ImGui::PushFont(window.get_large_font());
        ImGui::PushStyleColor(ImGuiCol_Text, white);
        ImGui::Text("%s", title_message.c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);

        positions.orig_pos = ImGui::GetCursorScreenPos();
        positions.mid_y = (positions.orig_pos.y + positions.y0 + positions.h - 30) / 2;

        ImGui::GetWindowDrawList()->AddRectFilled({ positions.orig_pos.x, positions.orig_pos.y },
            { positions.x0 + positions.w - 5, positions.y0 + positions.h - 30 }, ImColor(header_color));
        ImGui::GetWindowDrawList()->AddLine({ positions.orig_pos.x, positions.mid_y },
            { positions.x0 + positions.w - 10, positions.mid_y }, ImColor(sensor_bg));

        auto& update = updates_copy[selected_index];

        bool sw_update_needed(false), fw_update_needed(false);

        bool fw_update_in_process = _fw_update_state == fw_update_states::started
                                 || _fw_update_state == fw_update_states::failed_updating
                                 || _fw_update_state == fw_update_states::failed_downloading;

        // Verify Device Exists or a FW update process
        if (update.profile.dev_active || fw_update_in_process)
        {
            // ===========================================================================
            // Draw Software update Pane
            // ===========================================================================
            sw_update_needed = draw_software_section(window_name, update, positions, window, error_message);

            // ===========================================================================
            // Draw Firmware update Pane
            // ===========================================================================
            fw_update_needed = draw_firmware_section(not_model, window_name, update, positions, window, error_message);
        }
        else 
        { // Indicate device disconnected to the user
            ImGui::PushStyleColor(ImGuiCol_Text, white);
            ImGui::SetWindowFontScale(1.5);
            std::string disconnected_text = "THE DEVICE HAS BEEN DISCONNECTED,";
            auto disconnected_text_size = ImGui::CalcTextSize(disconnected_text.c_str());
            auto disconnected_text_x_pixel = positions.w / 2 - disconnected_text_size.x / 2; // Align 2 center
            auto vertical_padding = 100.f;
            ImGui::SetCursorPos({ disconnected_text_x_pixel, vertical_padding });
            ImGui::Text("%s", disconnected_text.c_str());

            std::string reconnect_text = "PLEASE RECONNECT IT OR CLOSE THE UPDATES WINDOW.";
            auto reconnect_text_size = ImGui::CalcTextSize(reconnect_text.c_str());
            auto reconnect_text_x_pixel = positions.w / 2 - reconnect_text_size.x / 2; // Align 2 center
            ImGui::SetCursorPosX(reconnect_text_x_pixel);
            ImGui::Text("%s", reconnect_text.c_str());

            ImGui::SetWindowFontScale(3.);

            auto disconnect_icon_size = ImGui::CalcTextSize(textual_icons::lock);
            auto disconnect_icon_x_pixel = (positions.w / 2.f) -  (disconnect_icon_size.x / 2); // Align 2 center
            ImGui::SetCursorPosX( disconnect_icon_x_pixel );
            ImGui::Text("%s", static_cast<const char *>(textual_icons::lock));
            ImGui::SetWindowFontScale(1.);
            ImGui::PopStyleColor();


        }
        // ===========================================================================
        // Draw Lower Pane
        // ===========================================================================

        auto no_update_needed = (!sw_update_needed && !fw_update_needed);

        if (!no_update_needed)
        {
            ImGui::SetCursorPos({ 5, positions.h - 25 });
            if (emphasize_dismiss_text)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, light_red);
            }

            ImGui::Checkbox("I understand and would like to proceed anyway without updating", &ignore);

            if (emphasize_dismiss_text)
            {
                ImGui::PopStyleColor();
            }
        }

        ImGui::SetCursorPos({ positions.w - 145, positions.h - 25 });

        auto enabled = ignore || no_update_needed;
        ImGui::PushStyleColor(ImGuiCol_Border, black); 
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.5f);
        if (enabled)
        {
            if (_fw_update_state != fw_update_states::started)
            {
                if (ImGui::Button("Close", { 120, 20 }))
                {
                    {
                        std::lock_guard<std::mutex> lock(_lock);
                        _updates.clear();
                    }

                    ImGui::CloseCurrentPopup();
                    emphasize_dismiss_text = false;
                    ignore = false;
                    _fw_update_state = fw_update_states::ready;
                    _fw_download_progress = 0;
                }
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);
                ImGui::Button("Close", { 120, 20 });
                ImGui::PopStyleColor();
            }
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sensor_bg);

            if (ImGui::Button("Close", { 120, 20 }))
            {
                emphasize_dismiss_text = true;
            }
            if (ImGui::IsItemHovered())
            {
                RsImGui::CustomTooltip("To close this window you must install all essential update\n"
                    "or agree to the warning of closing without it");
            }
            ImGui::PopStyleColor(2);
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

}

bool updates_model::draw_software_section(const char * window_name, update_profile_model& selected_profile, position_params& pos, ux_window& window, std::string& error_message)
{
    bool essential_sw_update_needed(false);
    bool recommended_sw_update_needed(false);
    {
        // Prepare sorted array of SW updates profiles
        // Assumption - essential updates version <= other policies versions
        std::vector<dev_updates_profile::version_info> software_updates;
        for (auto&& swu : selected_profile.profile.software_versions)
            software_updates.push_back(swu.second);
        std::sort(software_updates.begin(), software_updates.end(), [](dev_updates_profile::version_info& a, dev_updates_profile::version_info& b) {
            return a.ver < b.ver;
        });
        if (static_cast<int>(software_updates.size()) <= selected_software_update_index) selected_software_update_index = 0;

        dev_updates_profile::version_info selected_software_update;
        if (software_updates.size() != 0)
        {
            bool essential_found(false);
            bool recommended_found(false);
            for (auto sw_update : software_updates)
            {
                if (!essential_found)
                {
                    essential_found = essential_found || sw_update.name_for_display.find("ESSENTIAL") != std::string::npos;
                    essential_sw_update_needed = essential_sw_update_needed || essential_found && (selected_profile.profile.software_version < sw_update.ver);
                }

                if (!recommended_found)
                {
                    recommended_found = recommended_found || sw_update.name_for_display.find("RECOMMENDED") != std::string::npos;
                    recommended_sw_update_needed = recommended_sw_update_needed || recommended_found && (selected_profile.profile.software_version < sw_update.ver);
                }
            }

            // If essential update found on DB but not needed - Remove it
            if (essential_found && !essential_sw_update_needed)
            {
                auto it = std::find_if(software_updates.begin(), software_updates.end(), [&](dev_updates_profile::version_info& u) {
                    return (u.name_for_display.find("ESSENTIAL") != std::string::npos);
                });
                if (it != software_updates.end())
                    software_updates.erase(it);
            }

            // If recommended update found on DB but not needed - Remove it
            if (recommended_found && !recommended_sw_update_needed)
            {
                auto it = std::find_if(software_updates.begin(), software_updates.end(), [&](dev_updates_profile::version_info& u) {
                    return (u.name_for_display.find("RECOMMENDED") != std::string::npos);
                });
                if (it != software_updates.end())
                    software_updates.erase(it);
            }

            if (essential_sw_update_needed || recommended_sw_update_needed)
            {
                selected_software_update = software_updates[selected_software_update_index];
            }
        }

        ImVec2 sw_text_pos(pos.orig_pos.x + 10, pos.orig_pos.y + 10);
        ImGui::SetCursorScreenPos(sw_text_pos);

        ImGui::PushFont(window.get_large_font());
        ImGui::PushStyleColor(ImGuiCol_Text, white);
        ImGui::Text("LibRealSense SDK: ");
        ImGui::PopStyleColor();
        ImGui::SameLine();

        if (!essential_sw_update_needed || software_updates.size() == 0)
        {
            if (recommended_sw_update_needed)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, white);
                ImGui::Text("Recommended update available!");
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
                ImGui::Text("%s", static_cast<const char *>(textual_icons::check_square_o));
                ImGui::SameLine();
                ImGui::Text("%s", "Up to date.");
            }
            ImGui::PopStyleColor();
        }
        else if (essential_sw_update_needed)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, yellowish);
            ImGui::Text("Essential update is available. Please install!");
            ImGui::PopStyleColor();
        }


        ImGui::PopFont();

        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        sw_text_pos.y += 30;
        ImGui::SetCursorScreenPos(sw_text_pos);
        ImGui::PushStyleColor(ImGuiCol_Text, white);
        ImGui::Text("%s", "Content:"); ImGui::SameLine();
        ImGui::PopStyleColor();
        ImGui::Text("%s", "RealSense SDK 2.0, RealSense Viewer and Depth Quality Tool");

        sw_text_pos.y += 20;
        ImGui::SetCursorScreenPos(sw_text_pos);
        ImGui::PushStyleColor(ImGuiCol_Text, white);
        ImGui::Text("%s", "Purpose:"); ImGui::SameLine();
        ImGui::PopStyleColor();
        ImGui::Text("%s", "Enhancements for stream alignment, texture mapping and camera accuracy health algorithms");

        sw_text_pos.y += 30;
        ImGui::SetCursorScreenPos(sw_text_pos);
        ImGui::PushStyleColor(ImGuiCol_Text, white);
        ImGui::Text("%s", "Current SW version:");
        ImGui::SameLine();
        ImGui::PopStyleColor();
        auto current_sw_ver_str = std::string(selected_profile.profile.software_version);
        ImGui::Text("%s", current_sw_ver_str.c_str());

        if (essential_sw_update_needed)
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, yellowish);
            ImGui::Text("%s", " (Your version is older than the minimum version required for the proper functioning of your device)");
            ImGui::PopStyleColor();
        }

        if ( selected_software_update.ver )
        {
            sw_text_pos.y += 25;
            ImGui::SetCursorScreenPos(sw_text_pos);
            ImGui::PushStyleColor(ImGuiCol_Text, white);

            ImGui::Text("%s", (software_updates.size() >= 2) ?
                "Versions available:" :
                "Version to download:");
            ImGui::SameLine();
            ImGui::PopStyleColor();
            // 3 box for multiple versions
            if (software_updates.size() >= 2)
            {
                std::vector<const char*> swu_labels;
                for (auto&& swu : software_updates)
                {
                    swu_labels.push_back(swu.name_for_display.c_str());
                }
                ImGui::PushStyleColor(ImGuiCol_BorderShadow, dark_grey);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, sensor_bg);

                std::string combo_id = "##Software Update Version";
                ImGui::PushItemWidth(200);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3);
                ImGui::Combo(combo_id.c_str(), &selected_software_update_index, swu_labels.data(), static_cast<int>(swu_labels.size()));
                ImGui::PopItemWidth();
                ImGui::PopStyleColor(2);
                ImGui::SetWindowFontScale(1.);
            }
            else
            {   // Single version
                ImGui::Text("%s", std::string(selected_software_update.ver).c_str());
            }
        }

        if (selected_software_update.release_page != "")
        {
            sw_text_pos.y += 25;
            ImGui::SetCursorScreenPos(sw_text_pos);
            ImGui::PushStyleColor(ImGuiCol_Text, white);
            ImGui::Text("%s", "Release Link:"); ImGui::SameLine();
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
            ImGui::Text("%s", selected_software_update.release_page.c_str());

            ImGui::SameLine();
            auto underline_start = ImVec2(ImGui::GetCursorScreenPos().x - (ImGui::CalcTextSize(selected_software_update.release_page.c_str()).x + 8), ImGui::GetCursorScreenPos().y + ImGui::GetFontSize());
            auto underline_end = ImVec2(ImGui::GetCursorScreenPos().x - 8, ImGui::GetCursorScreenPos().y + ImGui::GetFontSize());
            ImGui::GetWindowDrawList()->AddLine(underline_start, underline_end, ImColor(light_grey));

            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                window.link_hovered();
            if (ImGui::IsItemClicked())
            {
                try
                {
                    open_url(selected_software_update.release_page.c_str());
                }
                catch (...)
                {
                    LOG_ERROR("Error opening URL: " + selected_software_update.release_page);
                }
            }
        }


        if (selected_software_update.description != "")
        {
            sw_text_pos.y += 25;
            ImGui::SetCursorScreenPos(sw_text_pos);
            ImGui::PushStyleColor(ImGuiCol_Text, white);

            ImGui::Text("%s", "Description:");
            ImGui::PopStyleColor();

            ImGui::PushTextWrapPos(pos.w - 150);
            ImGui::PushStyleColor(ImGuiCol_Border, transparent);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
            ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, transparent);
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, transparent);
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, transparent);
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, transparent);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, regular_blue);
            auto msg = selected_software_update.description.c_str();
            sw_text_pos.x -= 4;
            sw_text_pos.y += 15;
            ImGui::SetCursorScreenPos(sw_text_pos);
            ImGui::InputTextMultiline("##Software Update Description", const_cast<char*>(msg),
                strlen(msg) + 1, ImVec2(ImGui::GetContentRegionAvail().x - 150, pos.mid_y - (pos.orig_pos.y + 160) - 40),
                ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor(7);
            ImGui::PopTextWrapPos();
        }

        if ( selected_software_update.ver )
        {
            ImGui::SetCursorScreenPos({ pos.orig_pos.x + pos.w - 150, pos.mid_y - 45 });
            ImGui::PushStyleColor(ImGuiCol_Text, white);
            ImGui::PushStyleColor(ImGuiCol_BorderShadow, dark_grey);
            ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);

            if (ImGui::Button("Download", { 120, 40 }))
            {
                try
                {
                    open_url(selected_software_update.download_link.c_str());
                }
                catch (...)
                {
                    LOG_ERROR("Error opening URL: " + selected_software_update.download_link);
                }
            }

            if (ImGui::IsItemHovered())
            {
                std::string tooltip = "This will redirect you to download the selected software from:\n" + selected_software_update.download_link;
                RsImGui::CustomTooltip("%s", tooltip.c_str());
                window.link_hovered();
            }

            ImGui::PopStyleColor(3);
            ImGui::SetCursorScreenPos({ pos.orig_pos.x + 10,  pos.mid_y - 25 });
            ImGui::Text("%s", "Visit the release page before download to identify the most suitable package.");
        }

        ImGui::PopStyleColor();
    }
    return essential_sw_update_needed;
}
bool updates_model::draw_firmware_section(std::shared_ptr<notifications_model> not_model, const char * window_name, update_profile_model& selected_profile, position_params& pos, ux_window& window, std::string& error_message)
{
    bool essential_fw_update_needed(false);
    bool recommended_fw_update_needed(false);

    // Prepare sorted array of FW updates profiles
    // Assumption - essential updates version <= other policies versions
    std::vector<dev_updates_profile::version_info> firmware_updates;
    for (auto&& swu : selected_profile.profile.firmware_versions)
        firmware_updates.push_back(swu.second);
    std::sort(firmware_updates.begin(), firmware_updates.end(), [](dev_updates_profile::version_info& a, dev_updates_profile::version_info& b) {
        return a.ver < b.ver;
    });
    if (static_cast<int>(firmware_updates.size()) <= selected_firmware_update_index) selected_firmware_update_index = 0;

    dev_updates_profile::version_info selected_firmware_update;

    if (firmware_updates.size() != 0)
    {
        bool essential_found(false);
        bool recommended_found(false);

        for (auto fw_update : firmware_updates)
        {
            if (!essential_found)
            {
                essential_found = essential_found || fw_update.name_for_display.find("ESSENTIAL") != std::string::npos;
                essential_fw_update_needed = essential_fw_update_needed || essential_found && (selected_profile.profile.firmware_version < fw_update.ver);
            }

            if (!recommended_found)
            {
                recommended_found = recommended_found || fw_update.name_for_display.find("RECOMMENDED") != std::string::npos;
                recommended_fw_update_needed = recommended_fw_update_needed || recommended_found && (selected_profile.profile.firmware_version < fw_update.ver);
            }
        }

        // If essential update found on DB but not needed - Remove it
        if (essential_found && !essential_fw_update_needed)
        {
            auto it = std::find_if(firmware_updates.begin(), firmware_updates.end(), [&](dev_updates_profile::version_info& u) {
                return (u.name_for_display.find("ESSENTIAL") != std::string::npos);
            });
            if (it != firmware_updates.end())
                firmware_updates.erase(it);
        }

        // If recommended update found on DB but not needed - Remove it
        if (recommended_found && !recommended_fw_update_needed)
        {
            auto it = std::find_if(firmware_updates.begin(), firmware_updates.end(), [&](dev_updates_profile::version_info& u) {
                return (u.name_for_display.find("RECOMMENDED") != std::string::npos);
            });
            if (it != firmware_updates.end())
                firmware_updates.erase(it);
        }
        if (essential_fw_update_needed || recommended_fw_update_needed)
        {
            selected_firmware_update = firmware_updates[selected_firmware_update_index];
        }

    }

    auto left_padding = 10;
    auto top_padding = 15;
    ImVec2 fw_text_pos(pos.orig_pos.x + left_padding, pos.mid_y + top_padding);
    ImGui::SetCursorScreenPos(fw_text_pos);

    ImGui::PushFont(window.get_large_font());
    ImGui::PushStyleColor(ImGuiCol_Text, white);
    ImGui::Text("FIRMWARE: ");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (!essential_fw_update_needed || firmware_updates.size() == 0)
    {
        if (recommended_fw_update_needed)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, white);
            ImGui::Text("Recommended update available!");
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
            ImGui::Text("%s", static_cast<const char *>(textual_icons::check_square_o));
            ImGui::SameLine();
            ImGui::Text("%s", "Up to date.");
        }
        ImGui::PopStyleColor();
    }
    else if (essential_fw_update_needed)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, yellowish);
        ImGui::Text("Essential update is available. Please install!");
        ImGui::PopStyleColor();
    }


    ImGui::PopFont();

    ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
    fw_text_pos.y += 25;
    ImGui::SetCursorScreenPos(fw_text_pos);
    ImGui::PushStyleColor(ImGuiCol_Text, white);
    ImGui::Text("%s", "Content:"); ImGui::SameLine();
    ImGui::PopStyleColor();
    ImGui::Text("%s", "Signed Firmware Image (.bin file)");

    fw_text_pos.y += 25;
    ImGui::SetCursorScreenPos(fw_text_pos);
    ImGui::PushStyleColor(ImGuiCol_Text, white);
    ImGui::Text("%s", "Device detected: ");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::Text("%s", selected_profile.profile.device_name.c_str());
    ImGui::SameLine();
    ImGui::Text(", SN: %s", selected_profile.profile.serial_number.c_str());

    fw_text_pos.y += 25;
    ImGui::SetCursorScreenPos(fw_text_pos);
    ImGui::PushStyleColor(ImGuiCol_Text, white);
    ImGui::Text("%s", "Current FW version:");
    ImGui::SameLine();
    ImGui::PopStyleColor();

    // During FW update do not take the version from the device because a recovery device version is 0.0.0.0.
    // Instead display an update in progress label
    std::string current_fw_ver_str;
    if (_fw_update_state != fw_update_states::started)
        current_fw_ver_str = std::string(selected_profile.profile.firmware_version);
    else
        current_fw_ver_str = "Update in progress..";

    ImGui::Text("%s", current_fw_ver_str.c_str());



    if (essential_fw_update_needed)
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, yellowish);
        ImGui::Text("%s", " (Your version is older than the minimum version required for the proper functioning of your device)");
        ImGui::PopStyleColor();
    }

    if ( selected_firmware_update.ver )
    {
        fw_text_pos.y += 25;
        ImGui::SetCursorScreenPos(fw_text_pos);
        ImGui::PushStyleColor(ImGuiCol_Text, white);

        ImGui::Text("%s", (firmware_updates.size() >= 2) ?
            "Versions available:" :
            "Version to download:");
        ImGui::SameLine();
        ImGui::PopStyleColor();
        // Combo box for multiple versions
        if (firmware_updates.size() >= 2)
        {
            std::vector<const char*> fwu_labels;
            for (auto&& fwu : firmware_updates)
            {
                fwu_labels.push_back(fwu.name_for_display.c_str());
            }

            ImGui::PushStyleColor(ImGuiCol_BorderShadow, dark_grey);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, sensor_bg);

            std::string combo_id = "##Firmware Update Version";
            ImGui::PushItemWidth(200);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3);
            ImGui::Combo(combo_id.c_str(), &selected_firmware_update_index, fwu_labels.data(), static_cast<int>(fwu_labels.size()));
            ImGui::PopItemWidth();
            ImGui::PopStyleColor(2);
            ImGui::SetWindowFontScale(1.);
        }
        else
        {   // Single version
            ImGui::Text("%s", std::string(selected_firmware_update.ver).c_str());
        }
    }

    if (selected_firmware_update.release_page != "")
    {
        fw_text_pos.y += 25;
        ImGui::SetCursorScreenPos(fw_text_pos);
        ImGui::PushStyleColor(ImGuiCol_Text, white);
        ImGui::Text("%s", "Release Link:"); ImGui::SameLine();
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        ImGui::Text("%s", selected_firmware_update.release_page.c_str());

        ImGui::SameLine();
        auto underline_start = ImVec2(ImGui::GetCursorScreenPos().x - (ImGui::CalcTextSize(selected_firmware_update.release_page.c_str()).x + 8), ImGui::GetCursorScreenPos().y + ImGui::GetFontSize());
        auto underline_end = ImVec2(ImGui::GetCursorScreenPos().x - 8, ImGui::GetCursorScreenPos().y + ImGui::GetFontSize());
        ImGui::GetWindowDrawList()->AddLine(underline_start, underline_end, ImColor(light_grey));


        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            window.link_hovered();
        if (ImGui::IsItemClicked())
        {
            try
            {
                open_url(selected_firmware_update.release_page.c_str());
            }
            catch (...)
            {
                LOG_ERROR("Error opening URL: " + selected_firmware_update.release_page);
            }
        }
    }

    if (selected_firmware_update.description != "")
    {
        fw_text_pos.y += 25;
        ImGui::SetCursorScreenPos(fw_text_pos);
        ImGui::PushStyleColor(ImGuiCol_Text, white);
        ImGui::Text("%s", "Description:");
        ImGui::PopStyleColor();

        ImGui::PushTextWrapPos(pos.w - 150);
        ImGui::PushStyleColor(ImGuiCol_Border, transparent);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, transparent);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, transparent);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, transparent);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, transparent);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, regular_blue);
        auto msg = selected_firmware_update.description.c_str();
        fw_text_pos.x -= 4;
        fw_text_pos.y += 15;
        ImGui::SetCursorScreenPos(fw_text_pos);
        ImGui::InputTextMultiline("##Firmware Update Description", const_cast<char*>(msg),
            strlen(msg) + 1, ImVec2(ImGui::GetContentRegionAvail().x - 150, 75),
            ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor(7);
        ImGui::PopTextWrapPos();
    }


    if( ( _fw_update_state == fw_update_states::ready || _fw_update_state == fw_update_states::completed )
        && ( essential_fw_update_needed || recommended_fw_update_needed ) )
    {
        ImGui::SetCursorScreenPos({ pos.orig_pos.x + pos.w - 150, pos.orig_pos.y + pos.h - 115 });
        ImGui::PushStyleColor(ImGuiCol_Text, white);
        ImGui::PushStyleColor(ImGuiCol_Button, sensor_bg);

        if (ImGui::Button("Download &\n   Install", ImVec2(120, 40)) || _retry)
        {
            _retry = false;
            auto link = selected_firmware_update.download_link;
            std::thread download_thread([link, this]() {
                std::vector<uint8_t> vec;
                http_downloader client;

                if (!client.download_to_bytes_vector(link, vec,
                    [this](uint64_t dl_current_bytes, uint64_t dl_total_bytes) -> callback_result {
                    _fw_download_progress = static_cast<int>((dl_current_bytes * 100) / dl_total_bytes);
                    return callback_result::CONTINUE_DOWNLOAD;
                }))
                {
                    _fw_update_state = fw_update_states::failed_downloading;
                    LOG_ERROR("Error in download firmware version from: " + link);
                }

                _fw_image = vec;

                _fw_download_progress = 100;
            });
            download_thread.detach();

            _fw_update_state = fw_update_states::downloading;
            _progress.last_progress_time = std::chrono::system_clock::now();
        }
        if (ImGui::IsItemHovered())
        {
            RsImGui::CustomTooltip("This will download selected firmware and install it to the device");
            window.link_hovered();
        }
        ImGui::PopStyleColor(2);
    }
    else if (_fw_update_state == fw_update_states::downloading)
    {
        ImGui::SetCursorScreenPos({ pos.orig_pos.x + left_padding, pos.orig_pos.y + pos.h - 95 });
        _progress.draw(window, static_cast<int>(pos.w) - 170 - left_padding, _fw_download_progress / 3);
        if (_fw_download_progress == 100 && !_fw_image.empty())
        {
            _fw_download_progress = 0;
            _fw_update_state = fw_update_states::started;

            _update_manager = std::make_shared<firmware_update_manager>(not_model,
                *selected_profile.dev_model, selected_profile.profile.dev, selected_profile.ctx, _fw_image, true
                );
            auto invoke = [](std::function<void()> action) { action(); };
            _update_manager->start(invoke);
        }
    }
    else if (_fw_update_state == fw_update_states::started)
    {
        ImGui::SetCursorScreenPos({ pos.orig_pos.x + left_padding, pos.orig_pos.y + pos.h - 95 });
        _progress.draw(window, static_cast<int>(pos.w) - 170 - left_padding, static_cast<int>(_update_manager->get_progress() * 0.66 + 33));
        if (_update_manager->done()) {
            _fw_update_state = fw_update_states::completed;
            _fw_image.clear();
        }

        if (_update_manager->failed()) {
            _fw_update_state = fw_update_states::failed_updating;
            _fw_image.clear();
            _fw_download_progress = 0;
        }
        // Verify an error window will not pop up. ImGui cannot handle 2 PopUpModals in parallel.
        if (!error_message.empty())
        {
            LOG_ERROR("error caught during update process, details: " + error_message);
            error_message.clear();
        }

    }
    else if (_fw_update_state == fw_update_states::failed_downloading ||
        _fw_update_state == fw_update_states::failed_updating)
    {
        ImGui::SetCursorScreenPos({ pos.orig_pos.x + left_padding, pos.orig_pos.y + pos.h - 95 });
        ImGui::PushStyleColor(ImGuiCol_Text, white);
        std::string text = _fw_update_state == fw_update_states::failed_downloading ?
            "Firmware download failed, check connection and press to retry" :
            "Firmware update process failed, press to retry";
        if (ImGui::Button(text.c_str(), ImVec2(pos.w - 170 - left_padding, 25)))
        {
            _fw_update_state = fw_update_states::ready;
            _update_manager.reset();
            _fw_image.clear();
            _retry = true;
        }
        ImGui::PopStyleColor();
        _fw_download_progress = 0;
    }
    else if (_fw_update_state == fw_update_states::completed)
    {
        _fw_update_state = fw_update_states::ready;
        _update_manager.reset();
        _fw_image.clear();
        _fw_download_progress = 0;
    }

    ImGui::PopStyleColor();

    return essential_fw_update_needed;
}

