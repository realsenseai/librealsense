// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// assistant_model member functions grouped here by responsibility (the collapsed launcher button),
// not a separate class - they read/write _open, _health etc. directly, shared with the panel files.

#include "assistant-model.h"
#include "device-model.h"
#include "ux-window.h"
#include "assistant-ui-utils.h"
#include <stb_image.h>
#include "res/realsense-mark.hpp"

namespace rs2
{
    void assistant_model::draw_launcher_button(ux_window& win, float bottom_clearance)
    {
        const char* label = "Ask RealSenseAI";
        const float btn_h = 44.f;
        const float left_pad = 10.f, logo_gap = 10.f, text_gap = 10.f, dot_gap = 10.f, right_pad = 14.f;
        const float logo_r = btn_h * 0.5f - 8.f;
        const float dot_r = 4.f;
        const float pill_rounding = 999.f; // always clamps to a true stadium/pill, regardless of btn_h

        ImGui::PushFont(win.get_font());
        float text_w = ImGui::CalcTextSize(label).x;
        ImGui::PopFont();

        const float btn_w = left_pad + logo_r * 2.f + logo_gap + text_w + dot_gap + dot_r * 2.f + right_pad;
        const float margin = 20.f;
        const float x = win.width() - btn_w - margin, y = win.height() - btn_h - margin - bottom_clearance;

        assistant_detail::draw_soft_shadow({ x, y }, { btn_w, btn_h }, btn_h * 0.5f);

        ImGui::SetNextWindowPos({ x, y });
        ImGui::SetNextWindowSize({ btn_w, btn_h });
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // else default padding clips the button
        auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
        ImGui::Begin("##assistant_launcher", nullptr, flags);

        ImGui::PushStyleColor(ImGuiCol_Button, _open ? header_window_bg : sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, header_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, header_color);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, pill_rounding);
        bool clicked = ImGui::Button("##ask_ai", { btn_w, btn_h });
        bool hovered = ImGui::IsItemHovered();
        auto bmin = ImGui::GetItemRectMin();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        if (clicked)
        {
            _open = !_open;
            if (_open)
                _focus_input_next_frame = true;
        }

        float cy = bmin.y + btn_h * 0.5f;
        float logo_cx = bmin.x + left_pad + logo_r;
        draw_logo(logo_cx, cy, logo_r);

        ImGui::PushFont(win.get_font());
        ImGui::PushStyleColor(ImGuiCol_Text, _open ? light_blue : light_grey);
        float text_x = logo_cx + logo_r + logo_gap;
        ImGui::SetCursorScreenPos({ text_x, cy - ImGui::GetTextLineHeight() * 0.5f });
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::PopFont();

        if (_health != assistant_health::unknown)
        {
            float dot_cx = text_x + text_w + dot_gap + dot_r;
            ImVec4 dot_color = _health == assistant_health::healthy ? green : redish;
            ImGui::GetWindowDrawList()->AddCircleFilled({ dot_cx, cy }, dot_r, ImColor(dot_color));
        }

        if (hovered)
        {
            RsImGui::CustomTooltip("Ask the RealSenseAI Assistant");
            win.link_hovered();
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    // A white disc with the RealSense pinwheel mark on top. The mark's texture is lazy-loaded once
    // from an embedded, pre-cropped, transparent PNG.
    void assistant_model::draw_logo(float cx, float cy, float radius)
    {
        if (!_mark_loaded)
        {
            _mark_loaded = true;
            int w, h, comp;
            auto* pixels = stbi_load_from_memory(realsense_mark, (int)realsense_mark_size, &w, &h, &comp, 4);
            if (pixels)
            {
                _mark_tex.upload_image(w, h, pixels);
                stbi_image_free(pixels);
            }
        }

        auto* dl = ImGui::GetWindowDrawList();
        dl->AddCircleFilled({ cx, cy }, radius, ImColor(255, 255, 255, 255));
        if (auto tex = _mark_tex.get_gl_handle())
        {
            float inset = radius * 0.8f;
            dl->AddImage((void*)(intptr_t)tex, { cx - inset, cy - inset }, { cx + inset, cy + inset });
        }
    }
}
