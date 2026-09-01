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
    namespace
    {
        struct launcher_geometry
        {
            float x, y, btn_w, btn_h, logo_r, dot_r, left_pad, logo_gap, dot_gap, text_w;
        };

        // Pure layout math for the collapsed launcher pill, kept separate from draw_launcher_button()
        // below so that function only has to deal with ImGui calls and state, not geometry arithmetic.
        launcher_geometry compute_launcher_geometry(ux_window& win, float bottom_clearance, const char* label)
        {
            launcher_geometry g;
            g.btn_h = 44.f;
            g.left_pad = 10.f;
            g.logo_gap = 10.f;
            g.dot_gap = 10.f;
            g.logo_r = g.btn_h * 0.5f - 8.f;
            g.dot_r = 4.f;
            const float right_pad = 14.f;

            ImGui::PushFont(win.get_font());
            g.text_w = ImGui::CalcTextSize(label).x;
            ImGui::PopFont();

            g.btn_w = g.left_pad + g.logo_r * 2.f + g.logo_gap + g.text_w + g.dot_gap + g.dot_r * 2.f + right_pad;
            const float margin = 20.f;
            g.x = win.width() - g.btn_w - margin;
            g.y = win.height() - g.btn_h - margin - bottom_clearance;
            return g;
        }
    }

    void assistant_model::draw_launcher_button(ux_window& win, float bottom_clearance)
    {
        const char* label = "Ask RealSenseAI";
        const float pill_rounding = 999.f; // always clamps to a true stadium/pill, regardless of btn_h
        auto g = compute_launcher_geometry(win, bottom_clearance, label);

        assistant_detail::draw_soft_shadow({ g.x, g.y }, { g.btn_w, g.btn_h }, g.btn_h * 0.5f);

        ImGui::SetNextWindowPos({ g.x, g.y });
        ImGui::SetNextWindowSize({ g.btn_w, g.btn_h });
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // else default padding clips the button
        auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
        ImGui::Begin("##assistant_launcher", nullptr, flags);

        ImGui::PushStyleColor(ImGuiCol_Button, _open ? header_window_bg : sensor_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, header_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, header_color);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, pill_rounding);
        bool clicked = ImGui::Button("##ask_ai", { g.btn_w, g.btn_h });
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

        float cy = bmin.y + g.btn_h * 0.5f;
        float logo_cx = bmin.x + g.left_pad + g.logo_r;
        draw_logo(logo_cx, cy, g.logo_r);

        ImGui::PushFont(win.get_font());
        ImGui::PushStyleColor(ImGuiCol_Text, _open ? light_blue : light_grey);
        float text_x = logo_cx + g.logo_r + g.logo_gap;
        ImGui::SetCursorScreenPos({ text_x, cy - ImGui::GetTextLineHeight() * 0.5f });
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::PopFont();

        if (_health != assistant_health::unknown)
        {
            float dot_cx = text_x + g.text_w + g.dot_gap + g.dot_r;
            ImVec4 dot_color = _health == assistant_health::healthy ? green : redish;
            ImGui::GetWindowDrawList()->AddCircleFilled({ dot_cx, cy }, g.dot_r, ImColor(dot_color));
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
        dl->AddCircleFilled({ cx, cy }, radius, ImColor(white));
        if (auto tex = _mark_tex.get_gl_handle())
        {
            float inset = radius * 0.8f;
            dl->AddImage((void*)(intptr_t)tex, { cx - inset, cy - inset }, { cx + inset, cy + inset });
        }
    }
}
