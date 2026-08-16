// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "assistant-markdown.h"
#include "device-model.h"
#include "ux-window.h"
#include "os.h"
#include <algorithm>

namespace rs2
{
    namespace
    {
        // Which frame of an animated image should be showing right now, cycling through
        // frame_delays_ms based on wall-clock time - same idea as the streaming "..." dots.
        size_t current_frame_index(const assistant::cached_image& img, double time_seconds)
        {
            if (img.frame_delays_ms.size() <= 1)
                return 0;

            long long total_ms = 0;
            for (int d : img.frame_delays_ms) total_ms += d;
            if (total_ms <= 0)
                return 0;

            long long elapsed_ms = (long long)(time_seconds * 1000.0) % total_ms;
            long long acc = 0;
            for (size_t i = 0; i < img.frame_delays_ms.size(); i++)
            {
                acc += img.frame_delays_ms[i];
                if (elapsed_ms < acc)
                    return i;
            }
            return img.frame_delays_ms.size() - 1;
        }
    }

    namespace assistant_detail
    {
        // A deliberately lightweight, line-oriented renderer: each markdown line becomes one
        // flowing plain-text line, except headings, rules, and images - drawn as their own
        // per-line blocks rather than inline-mixed-style text.
        void draw_markdown_body(ux_window& win, const std::string& text, float wrap_width,
            markdown_cache& cache, assistant::assistant_image_cache& images, const assistant::invoke_fn& invoke)
        {
            if (cache.source != text)
            {
                cache.source = text;
                cache.lines = parse_markdown(text);
            }

            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);

            for (auto&& line : cache.lines)
            {
                if (line.kind == markdown_line::kind::blank)
                {
                    ImGui::Spacing();
                    continue;
                }
                if (line.kind == markdown_line::kind::rule)
                {
                    ImGui::Separator();
                    continue;
                }
                if (line.kind == markdown_line::kind::image)
                {
                    auto* img = images.get_or_load(line.image_url, invoke);
                    if (img->state == assistant::image_load_state::loading)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_grey, 0.6f));
                        ImGui::TextUnformatted("Loading image...");
                        ImGui::PopStyleColor();
                    }
                    else if (img->state == assistant::image_load_state::failed)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
                        ImGui::TextUnformatted(line.image_url.c_str());
                        ImGui::PopStyleColor();
                        if (ImGui::IsItemClicked())
                            open_url(line.image_url.c_str());
                        if (ImGui::IsItemHovered())
                        {
                            RsImGui::CustomTooltip(line.image_url.c_str());
                            win.link_hovered();
                        }
                    }
                    else // loaded
                    {
                        const float max_display_h = 300.f;
                        float scale = std::min(wrap_width / (float)img->width, max_display_h / (float)img->height);
                        scale = std::min(scale, 1.f); // never upscale past native size
                        ImVec2 disp_size{ img->width * scale, img->height * scale };

                        size_t frame = current_frame_index(*img, win.time());
                        auto tex = img->frame_textures[frame]->get_gl_handle();
                        ImGui::Image((void*)(intptr_t)tex, disp_size);
                        if (ImGui::IsItemClicked())
                            open_url(line.image_url.c_str());
                        if (ImGui::IsItemHovered())
                        {
                            RsImGui::CustomTooltip(line.image_url.c_str());
                            win.link_hovered();
                        }
                    }
                    continue;
                }

                ImGui::PushFont(line.kind == markdown_line::kind::heading ? win.get_large_font() : win.get_font());
                float base_left_x = ImGui::GetCursorPosX();
                if (line.kind == markdown_line::kind::bullet)
                {
                    ImGui::Bullet();
                    ImGui::SameLine();
                }

                // One word per TextUnformatted call, not stitched styled runs (SameLine(0,0)):
                // that approach breaks once a run wraps internally, since TextWrapped anchors
                // continuation lines to the run's own start.
                float line_left_x = ImGui::GetCursorPosX();
                // A bullet's hanging indent shifts line_left_x right of base_left_x, so the same
                // budget the caller measured from base_left_x now leaves less room before the
                // panel's right edge - subtract the indent or bullet text would run past it.
                float effective_wrap_width = wrap_width - (line_left_x - base_left_x);
                float space_w = ImGui::CalcTextSize(" ").x;
                float used_w = 0.f;
                bool at_line_start = true;
                for (auto&& word : line.words)
                {
                    float word_w = ImGui::CalcTextSize(word.text.c_str()).x;

                    if (word_w > effective_wrap_width)
                    {
                        // Wider than a whole line by itself (typically a long URL with nothing to
                        // break on) - this word-granularity engine can't wrap it any further, so
                        // let ImGui's own wrapper force a mid-word break instead of clipping it.
                        if (!at_line_start)
                            ImGui::SetCursorPosX(line_left_x);
                        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + effective_wrap_width);
                        if (!word.is_link)
                        {
                            ImGui::TextWrapped("%s", word.text.c_str());
                        }
                        else
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
                            ImGui::TextWrapped("%s", word.text.c_str());
                            ImGui::PopStyleColor();
                            if (ImGui::IsItemClicked())
                                open_url(word.url.c_str());
                            if (ImGui::IsItemHovered())
                            {
                                RsImGui::CustomTooltip(word.url.c_str());
                                win.link_hovered();
                            }
                        }
                        ImGui::PopTextWrapPos();
                        ImGui::SetCursorPosX(line_left_x);
                        at_line_start = true;
                        used_w = 0.f;
                        continue;
                    }

                    float needed = (at_line_start ? 0.f : space_w) + word_w;
                    if (!at_line_start && used_w + needed > effective_wrap_width)
                    {
                        // Y is already correct: the previous word's own ItemSize() advanced it
                        // (SameLine() is what cancels that to stay on a row). Adding another
                        // line-height here was the actual cause of the earlier "large gaps" bug.
                        ImGui::SetCursorPosX(line_left_x);
                        at_line_start = true;
                        used_w = 0.f;
                        needed = word_w;
                    }
                    if (!at_line_start)
                        ImGui::SameLine(0.f, space_w);

                    if (!word.is_link)
                    {
                        ImGui::TextUnformatted(word.text.c_str());
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
                        ImGui::TextUnformatted(word.text.c_str());
                        ImGui::PopStyleColor();
                        if (ImGui::IsItemClicked())
                            open_url(word.url.c_str());
                        if (ImGui::IsItemHovered())
                        {
                            RsImGui::CustomTooltip(word.url.c_str());
                            win.link_hovered();
                        }
                    }

                    used_w += needed;
                    at_line_start = false;
                }

                ImGui::PopFont();
            }

            ImGui::PopStyleColor();
        }
    }
}
