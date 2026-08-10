// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "assistant-markdown.h"
#include "device-model.h"
#include "ux-window.h"
#include "os.h"

namespace rs2
{
    namespace assistant_detail
    {
        // A deliberately lightweight, line-oriented renderer: each markdown line becomes one
        // flowing plain-text line, except headings (bigger font) and horizontal rules (a real
        // separator) - the two cases that are per-line, not inline-mixed-style, formatting.
        void draw_markdown_body(ux_window& win, const std::string& text, float wrap_width, markdown_cache& cache)
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
