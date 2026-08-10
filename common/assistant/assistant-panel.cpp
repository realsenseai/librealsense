// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// assistant_model member functions grouped here by responsibility (panel chrome + input row), not
// a separate class - they read/write _expanded, _messages, _input_buffer etc. directly, shared
// with the launcher and messages files.

#include "assistant-model.h"
#include "device-model.h"
#include "ux-window.h"
#include "assistant-ui-utils.h"

namespace rs2
{
    bool assistant_model::draw_icon_button(const char* icon)
    {
        const float size = 24.f;
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, size * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Button, transparent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, header_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, header_color);
        bool clicked = ImGui::Button(icon, { size, size });
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        return clicked;
    }

    void assistant_model::draw_panel(ux_window& win, float bottom_clearance)
    {
        const float margin = 20.f;
        const float top_margin = 50.f; // stays clear of the viewer's own top bar
        const float collapsed_w = 380.f, collapsed_h = 520.f;
        float panel_w = _expanded ? std::min(640.f, win.width() - 2.f * margin) : collapsed_w;
        // Height and the position clamp share the same top/bottom margins, so if there isn't room
        // for the full 760 the panel shrinks to fit instead of letting its bottom edge overlap
        // whatever is docked below (the log panel).
        float available_h = win.height() - top_margin - margin - bottom_clearance;
        float panel_h = std::min(_expanded ? 760.f : collapsed_h, available_h);
        float x = win.width() - panel_w - margin;
        float y = std::max(top_margin, win.height() - panel_h - margin - bottom_clearance);

        const float rounding = 10.f;
        assistant_detail::draw_soft_shadow({ x, y }, { panel_w, panel_h }, rounding);

        ImGui::SetNextWindowPos({ x, y });
        ImGui::SetNextWindowSize({ panel_w, panel_h });
        // NoScrollbar/NoScrollWithMouse: this outer window's own content must never scroll itself -
        // only the inner "##assistant_messages" child should.
        auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, dark_window_background);
        ImGui::PushStyleColor(ImGuiCol_Border, header_color);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        ImGui::Begin("##assistant_panel", nullptr, flags);

        ImGui::PushFont(win.get_font());

        const float logo_r = 20.f;
        auto logo_anchor = ImGui::GetCursorScreenPos();
        draw_logo(logo_anchor.x + logo_r, logo_anchor.y + logo_r, logo_r);
        ImGui::Dummy({ logo_r * 2.f + 12.f, logo_r * 2.f }); // reserves room for the draw-list logo
        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::PushFont(win.get_large_font());
        ImGui::PushStyleColor(ImGuiCol_Text, white);
        ImGui::TextUnformatted("RealSense AI Assistant");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        auto status_pos = ImGui::GetCursorScreenPos();
        if (_health != assistant_health::unknown)
        {
            ImVec4 dot_color = _health == assistant_health::healthy ? green : redish;
            ImGui::GetWindowDrawList()->AddCircleFilled(
                { status_pos.x + 4.f, status_pos.y + ImGui::GetTextLineHeight() * 0.5f }, 4.f, ImColor(dot_color));
        }
        ImGui::Dummy({ 12.f, 0.f });
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        const char* status_text = _health == assistant_health::healthy ? "Online - powered by RealSense AI"
            : _health == assistant_health::unhealthy ? "Offline - powered by RealSense AI"
            : "Connecting - powered by RealSense AI";
        ImGui::TextUnformatted(status_text);
        ImGui::PopStyleColor();
        ImGui::EndGroup();

        // 3 icon buttons (new-conversation/expand/close) at draw_icon_button()'s 24px size, plus
        // ImGui's default ~8px ItemSpacing between each of the 2 gaps between them.
        const float header_icons_w = 3.f * 24.f + 2.f * 8.f;
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - header_icons_w);
        ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
        if (draw_icon_button(textual_icons::plus_circle))
            new_conversation();
        if (ImGui::IsItemHovered())
        {
            RsImGui::CustomTooltip("New conversation");
            win.link_hovered();
        }
        ImGui::SameLine();
        if (draw_icon_button(_expanded ? textual_icons::compress : textual_icons::expand))
            _expanded = !_expanded;
        if (ImGui::IsItemHovered())
        {
            RsImGui::CustomTooltip(_expanded ? "Collapse" : "Expand");
            win.link_hovered();
        }
        ImGui::SameLine();
        if (draw_icon_button(textual_icons::times))
            _open = false;
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::Separator();

        const float input_row_h = 44.f;
        float avail_h = ImGui::GetContentRegionAvail().y - input_row_h;
        if (_messages.empty())
            draw_greeting(win, avail_h);
        else
            draw_messages(win, avail_h);
        draw_input_row(win, ImGui::GetContentRegionAvail().x);

        ImGui::End();
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(2);
    }

    void assistant_model::draw_input_row(ux_window& win, float avail_w)
    {
        const float send_btn_w = 40.f;
        const float input_rounding = 10.f;

        ImGui::PushStyleColor(ImGuiCol_FrameBg, scrollbar_bg);
        ImGui::PushStyleColor(ImGuiCol_Border, header_color);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, input_rounding);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.f, 8.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushItemWidth(avail_w - send_btn_w - 8.f);
        if (_focus_input_next_frame)
        {
            ImGui::SetKeyboardFocusHere();
            _focus_input_next_frame = false;
        }
        bool enter_pressed = ImGui::InputText("##assistant_input", _input_buffer, sizeof(_input_buffer),
            ImGuiInputTextFlags_EnterReturnsTrue);
        float input_h = ImGui::GetItemRectSize().y;
        ImGui::PopItemWidth();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        ImGui::SameLine();

        bool has_text = _input_buffer[0] != '\0';
        ImGui::PushStyleColor(ImGuiCol_Button, regular_blue);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, light_blue);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, light_blue);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, input_rounding);
        bool send_clicked = false, stop_clicked = false;
        if (_waiting_for_response)
        {
            stop_clicked = ImGui::Button(textual_icons::stop, { send_btn_w, input_h });
            if (ImGui::IsItemHovered())
            {
                RsImGui::CustomTooltip("Stop generating");
                win.link_hovered();
            }
        }
        else
        {
            ImGui::BeginDisabled(!has_text);
            send_clicked = ImGui::Button(textual_icons::paper_plane, { send_btn_w, input_h });
            ImGui::EndDisabled();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if ((enter_pressed || send_clicked) && has_text && !_waiting_for_response)
            send_current_input();
        if (stop_clicked)
            cancel_current_request();
    }
}
