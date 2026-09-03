/* License: Apache 2.0. See LICENSE file in root directory. */
/* Copyright(c) 2024 RealSense, Inc. All Rights Reserved. */
#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include <functional>

namespace RsImGui
{
    class ScopePushFont
    {
    public:
        ScopePushFont(ImFont* font) { ImGui::PushFont(font); }
        ~ScopePushFont() { ImGui::PopFont(); }
    };
    class ScopePushStyleColor
    {
    public:
        ScopePushStyleColor(ImGuiCol idx, const ImVec4& col) { ImGui::PushStyleColor(idx, col); }
        ~ScopePushStyleColor() { ImGui::PopStyleColor(); }
    };
    class ScopePushStyleVar
    {
    public:
        ScopePushStyleVar(ImGuiStyleVar idx, float val) { ImGui::PushStyleVar(idx, val); }
        ScopePushStyleVar(ImGuiStyleVar idx, const ImVec2& val) { ImGui::PushStyleVar(idx, val); }
        ~ScopePushStyleVar() { ImGui::PopStyleVar(); }
    };
    bool          SliderIntWithSteps(const char* label, int* v, int v_min, int v_max, int v_step = 1);
    float         RoundScalar(float value, int decimal_precision);
    bool          CustomComboBox(const char* label, int* current_item, const char* const items[], int items_count);
    bool          SliderBehavior(const ImRect& frame_bb, ImGuiID id, float* v, float v_min, float v_max, float power, int decimal_precision, ImGuiSliderFlags flags, bool render_bg);
    bool          VSliderFloat(const char* label, const ImVec2& size, float* v, float v_min, float v_max, const char* display_format, float power, bool render_bg);
    bool          SliderIntTofloat(const char* label, int* v, int v_min, int v_max, const char* display_format = "%.0f");
    void          PushNewFrame();
    void          PopNewFrame();
    void          RsImButton(const std::function<void()> &f, bool disable_button = false);
    bool          TreeNode(const char* label, bool should_be_open);
    void          CustomTooltip( const char * fmt, const char * label );
    void          CustomTooltip( const char * label );
    void          CustomTooltip(const char* fmt, float value);
    }

// Macros to create unique function names using the current line number (__LINE__)
#define IMGUI_CONCAT_(x,y) x##y
#define IMGUI_CONCAT(x,y) IMGUI_CONCAT_(x,y)
#define RsImGui_ScopePushFont(f) RsImGui::ScopePushFont IMGUI_CONCAT(scope_push_font, __LINE__) (f)
#define RsImGui_ScopePushStyleColor(idx, col) RsImGui::ScopePushStyleColor IMGUI_CONCAT(scope_push_style_color, __LINE__) (idx, col)
#define RsImGui_ScopePushStyleVar(idx, val) RsImGui::ScopePushStyleVar IMGUI_CONCAT(scope_push_style_var, __LINE__) (idx, val)