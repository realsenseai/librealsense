/* License: Apache 2.0. See LICENSE file in root directory. */
/* Copyright(c) 2024 RealSense, Inc. All Rights Reserved. */

//Overloading and adding on imgui functions to accommodate realsense GUI
#include "realsense_imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

bool RsImGui::SliderIntWithSteps(const char* label, int* v, int v_min, int v_max, int v_step)
{
    float originalValue = static_cast<float>(*v);
    bool changed = false;

    float tempValue = static_cast<float>(*v); //to use SliderFloat (integers don't have any precision)

    // Create the slider with float precision during drag
    if (ImGui::SliderFloat(label, &tempValue, static_cast<float>(v_min), static_cast<float>(v_max), "%.3f")) {
        changed = true;

        // Round to nearest step while dragging
        if (ImGui::IsItemActive()) {
            tempValue = roundf(tempValue / static_cast<float>(v_step)) * static_cast<float>(v_step);
            *v = static_cast<int>(tempValue);
        }
        else {
            // When not dragging ensure we have a round integer value
            *v = static_cast<int>(roundf(tempValue));
        }

        // ensure the value is between min and max
        *v = ImClamp(*v, v_min, v_max);
    }

    return changed && (*v != originalValue);
}

float RsImGui::RoundScalar(float value, int decimal_precision)
{
    // Round past decimal precision
    // So when our value is 1.99999 with a precision of 0.001 we'll end up rounding to 2.0
    // FIXME: Investigate better rounding methods
    static const float min_steps[10] = { 1.0f, 0.1f, 0.01f, 0.001f, 0.0001f, 0.00001f, 0.000001f, 0.0000001f, 0.00000001f, 0.000000001f };
    float min_step = (decimal_precision >= 0 && decimal_precision < 10) ? min_steps[decimal_precision] : powf(10.0f, (float)-decimal_precision);
    bool negative = value < 0.0f;
    value = fabsf(value);
    float remainder = fmodf(value, min_step);
    if (remainder <= min_step * 0.5f)
        value -= remainder;
    else
        value += (min_step - remainder);
    return negative ? -value : value;
}

bool RsImGui::CustomComboBox(const char* label, int* current_item, const char* const items[], int items_count)
{
    bool value_changed = false;

    // the preview value - selected item
    const char* preview_value = (*current_item >= 0 && *current_item < items_count) ? items[*current_item] : "Select an item";
    if (ImGui::BeginCombo(label, ""))
    {
        //insert combobox items
        for (int i = 0; i < items_count; i++)
        {
            const bool is_selected = (i == *current_item);
            if (ImGui::Selectable(items[i], is_selected))
            {
                *current_item = i;
                value_changed = true;
            }
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Center the text in the combo box when closed
    ImVec2 pos = ImGui::GetItemRectMin();
    ImVec2 size = ImGui::GetItemRectSize();
    float arrowWidth = 20.0f; // Width reserved for the arrow
    float availableWidth = size.x - arrowWidth;

    ImVec2 textSize = ImGui::CalcTextSize(preview_value);
    float centerX = pos.x + (availableWidth - textSize.x) * 0.5f;
    float centerY = pos.y + (size.y - textSize.y) * 0.5f;

    // Ensure text stays within bounds
    if (centerX + textSize.x > pos.x + availableWidth)
        centerX = pos.x; // Align left if overflowing

    if (!ImGui::IsItemActive())
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(pos, ImVec2(pos.x + size.x - arrowWidth, pos.y + size.y));
        drawList->AddText(ImVec2(centerX, centerY), ImGui::GetColorU32(ImGuiCol_Text), preview_value);
        drawList->PopClipRect();
    }

    return value_changed;
}

bool RsImGui::SliderBehavior(const ImRect& frame_bb, ImGuiID id, float* v, float v_min, float v_max, float power, int decimal_precision, ImGuiSliderFlags flags, bool render_bg)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImGuiStyle& style = g.Style;

    const bool is_non_linear = fabsf(power - 1.0f) > 0.0001f;
    const bool is_horizontal = (flags & ImGuiSliderFlags_Vertical) == 0;


    if (!render_bg)
    {
        // Draw frame
        ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
    }


    const float grab_padding = 2.0f;
    const float slider_sz = is_horizontal ? (frame_bb.GetWidth() - grab_padding * 2.0f) : (frame_bb.GetHeight() - grab_padding * 2.0f);
    float grab_sz;
    if (decimal_precision > 0)
        grab_sz = ImMin(style.GrabMinSize, slider_sz);
    else
        grab_sz = ImMin(ImMax(1.0f * (slider_sz / (v_max - v_min + 1.0f)), style.GrabMinSize), slider_sz);  // Integer sliders, if possible have the grab size represent 1 unit
    const float slider_usable_sz = slider_sz - grab_sz;
    const float slider_usable_pos_min = (is_horizontal ? frame_bb.Min.x : frame_bb.Min.y) + grab_padding + grab_sz * 0.5f;
    const float slider_usable_pos_max = (is_horizontal ? frame_bb.Max.x : frame_bb.Max.y) - grab_padding - grab_sz * 0.5f;

    // For logarithmic sliders that cross over sign boundary we want the exponential increase to be symmetric around 0.0f
    float linear_zero_pos = 0.0f;   // 0.0->1.0f
    if (v_min * v_max < 0.0f)
    {
        // Different sign
        const float linear_dist_min_to_0 = powf(fabsf(0.0f - v_min), 1.0f / power);
        const float linear_dist_max_to_0 = powf(fabsf(v_max - 0.0f), 1.0f / power);
        linear_zero_pos = linear_dist_min_to_0 / (linear_dist_min_to_0 + linear_dist_max_to_0);
    }
    else
    {
        // Same sign
        linear_zero_pos = v_min < 0.0f ? 1.0f : 0.0f;
    }

    // Process clicking on the slider
    bool value_changed = false;
    if (g.ActiveId == id)
    {
        if (g.IO.MouseDown[0])
        {
            const float mouse_abs_pos = is_horizontal ? g.IO.MousePos.x : g.IO.MousePos.y;
            float normalized_pos = ImClamp((mouse_abs_pos - slider_usable_pos_min) / slider_usable_sz, 0.0f, 1.0f);
            if (!is_horizontal)
                normalized_pos = 1.0f - normalized_pos;

            float new_value;
            if (is_non_linear)
            {
                // Account for logarithmic scale on both sides of the zero
                if (normalized_pos < linear_zero_pos)
                {
                    // Negative: rescale to the negative range before powering
                    float a = 1.0f - (normalized_pos / linear_zero_pos);
                    a = powf(a, power);
                    new_value = ImLerp(ImMin(v_max, 0.0f), v_min, a);
                }
                else
                {
                    // Positive: rescale to the positive range before powering
                    float a;
                    if (fabsf(linear_zero_pos - 1.0f) > 1.e-6f)
                        a = (normalized_pos - linear_zero_pos) / (1.0f - linear_zero_pos);
                    else
                        a = normalized_pos;
                    a = powf(a, power);
                    new_value = ImLerp(ImMax(v_min, 0.0f), v_max, a);
                }
            }
            else
            {
                // Linear slider
                new_value = ImLerp(v_min, v_max, normalized_pos);
            }

            // Round past decimal precision, verify that it remains within min/max range
            new_value = RsImGui::RoundScalar(new_value, decimal_precision);
            if (new_value > v_max)  new_value = v_max;
            if (new_value < v_min)  new_value = v_min;

            if (*v != new_value)
            {
                *v = new_value;
                value_changed = true;
            }
        }
        else
        {
            ImGui::ClearActiveID();
        }
    }

    // Calculate slider grab positioning
    float grab_t;
    if (is_non_linear)
    {
        float v_clamped = ImClamp(*v, v_min, v_max);
        if (v_clamped < 0.0f)
        {
            const float f = 1.0f - (v_clamped - v_min) / (ImMin(0.0f, v_max) - v_min);
            grab_t = (1.0f - powf(f, 1.0f / power)) * linear_zero_pos;
        }
        else
        {
            const float f = (v_clamped - ImMax(0.0f, v_min)) / (v_max - ImMax(0.0f, v_min));
            grab_t = linear_zero_pos + powf(f, 1.0f / power) * (1.0f - linear_zero_pos);
        }
    }
    else
    {
        // Linear slider
        grab_t = (ImClamp(*v, v_min, v_max) - v_min) / (v_max - v_min);
    }

    // Draw
    if (!is_horizontal)
        grab_t = 1.0f - grab_t;
    const float grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
    ImRect grab_bb;
    if (is_horizontal)
        grab_bb = ImRect(ImVec2(grab_pos - grab_sz * 0.5f, frame_bb.Min.y + grab_padding), ImVec2(grab_pos + grab_sz * 0.5f, frame_bb.Max.y - grab_padding));
    else
        grab_bb = ImRect(ImVec2(frame_bb.Min.x + grab_padding, grab_pos - grab_sz * 0.5f), ImVec2(frame_bb.Max.x - grab_padding, grab_pos + grab_sz * 0.5f));

    if (render_bg)
    {
        auto bb = frame_bb;
        ImRect fill_br = frame_bb;
        auto slider_height = bb.Max.y - bb.Min.y;
        auto slider_width = bb.Max.x - bb.Min.x;
        ImVec2 graber_size = {};
        float width = (grab_bb.Max.x - grab_bb.Min.x);
        float height = (grab_bb.Max.y - grab_bb.Min.y);
        float radius = 1.0;
        if (is_horizontal)
        {
            bb.Min.y = bb.Min.y + (slider_height / 3);
            bb.Max.y = bb.Max.y - (slider_height / 3);
            if (bb.Max.y - bb.Min.y < 1.0f)
            {
                bb.Min.y -= 0.5;
                bb.Max.y += 0.5;
            }
            float grab_paddingl = 2.0f;
            //Horizontal fills from left to right
            fill_br.Min = bb.Min;
            fill_br.Max = ImVec2(ImLerp(bb.Min.x, bb.Max.x - grab_paddingl, *v / 100), bb.Max.y);
            graber_size = { grab_bb.Max.x - (width / 2.0f) , grab_bb.Max.y - (height / 2.0f) };
            radius = height / 2.5f;
        }
        else
        {
            bb.Min.x = bb.Min.x + (slider_width / 3);
            bb.Max.x = bb.Max.x - (slider_width / 3);
            if (bb.Max.x - bb.Min.x < 1.0f)
            {
                bb.Min.x -= 0.5;
                bb.Max.x += 0.5;
            }
            //Vertical fills from down upwards
            fill_br.Min = bb.Min;
            fill_br.Min.y = grab_bb.Min.y;
            fill_br.Max = bb.Max;
            graber_size = { grab_bb.Max.x - (width / 2.0f) , grab_bb.Max.y - (height / 2.0f) };
            radius = width;// / 2.5;
        }
        // Draw frame
        ImGui::RenderFrame(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), false, 10.0f);
        ImGui::RenderFrame(fill_br.Min, fill_br.Max, ImGui::ColorConvertFloat4ToU32({ 0, 112.f / 255, 197.f / 255, 1 }), false, 10.0f);
        window->DrawList->AddCircleFilled(graber_size, radius, ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), 16);
    }
    else
    {
        window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);
    }

    return value_changed;
}

bool RsImGui::VSliderFloat(const char* label, const ImVec2& size, float* v, float v_min, float v_max, const char* display_format, float power, bool render_bg)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    const ImRect frame_bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + size.x, window->DC.CursorPos.y + size.y));
    const ImRect bb(frame_bb.Min,ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x + frame_bb.Max .x: frame_bb.Max.x, frame_bb.Max.y));

    ImGui::ItemSize(bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(frame_bb, id))
        return false;

    const bool hovered = ImGui::IsItemHovered();
    if (hovered)
        ImGui::SetHoveredID(id);

    if (!display_format)
        display_format = "%.3f";
    int decimal_precision = ImParseFormatPrecision(display_format, 3);

    if (hovered && g.IO.MouseClicked[0])
    {
        ImGui::SetActiveID(id, window);
        ImGui::FocusWindow(window);
    }

    // Actual slider behavior + render grab
    bool value_changed = RsImGui::SliderBehavior(frame_bb, id, v, v_min, v_max, power, decimal_precision, ImGuiSliderFlags_Vertical, render_bg);

    // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
    // For the vertical slider we allow centered text to overlap the frame padding
    char value_buf[64];
    char* value_buf_end = value_buf + ImFormatString(value_buf, IM_ARRAYSIZE(value_buf), display_format, *v);
    ImGui::RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, value_buf, value_buf_end, NULL);
    if (label_size.x > 0.0f)
        ImGui::RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    return value_changed;
}

bool RsImGui::SliderIntTofloat(const char* label, int* v, int v_min, int v_max, const char* display_format)
{
    if (!display_format)
        display_format = "%.0f";
    float v_f = (float)*v;
    bool value_changed = ImGui::SliderFloat(label, &v_f, (float)v_min, (float)v_max, display_format, ImGuiSliderFlags_None);
    *v = (int)v_f;
    return value_changed;
}

void RsImGui::PushNewFrame()
{
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void RsImGui::PopNewFrame()
{
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void RsImGui::RsImButton(const std::function<void()>& f, bool disable_button)
{
    if (disable_button)
        ImGui::BeginDisabled();

    f();

    if (disable_button)
        ImGui::EndDisabled();
}

bool RsImGui::TreeNode(const char* label, bool should_be_open)
{
    // A node shown open on demand keeps the open state the user left it in: that state is put back
    // behind ImGui's back every frame, so once the caller stops asking, the tree is as it was. A
    // click while it is held open is the user overruling the caller, and is honored until then.
    ImGuiID const id = ImGui::GetID(label);
    ImGuiID const collapsed_id = id ^ 0x9E3779B9u;   // "closed by hand while held open"
    ImGuiStorage* storage = ImGui::GetStateStorage();

    if (!should_be_open)
    {
        storage->SetInt(collapsed_id, 0);
        return ImGui::TreeNode(label);
    }

    bool const collapsed_by_user = storage->GetInt(collapsed_id, 0) != 0;
    int const was_open = storage->GetInt(id, 0);
    if (!collapsed_by_user)
        ImGui::SetNextItemOpen(true);
    bool const open = ImGui::TreeNode(label);
    if (ImGui::IsItemToggledOpen())
        storage->SetInt(collapsed_id, open ? 0 : 1);
    else if (!collapsed_by_user)
        storage->SetInt(id, was_open);
    return open;
}

void RsImGui::CustomTooltip( const char * fmt, const char * label )
{
    ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 0.90f, 0.90f, 0.90f, 1.00f ) );
    ImGui::PushStyleColor( ImGuiCol_PopupBg, ImVec4( 0.05f, 0.05f, 0.10f, 0.90f ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 8, 8 ) );
    ImGui::SetTooltip( fmt, label );
    ImGui::PopStyleColor( 2 );
    ImGui::PopStyleVar();
}

void RsImGui::CustomTooltip( const char * label )
{
    RsImGui::CustomTooltip( "%s", label );
}

void RsImGui::CustomTooltip(const char* fmt, float value)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.05f, 0.05f, 0.10f, 0.90f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::SetTooltip(fmt, value);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}