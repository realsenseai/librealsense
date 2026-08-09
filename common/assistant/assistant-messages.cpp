// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// assistant_model member functions grouped here by responsibility (message list/bubbles), not a
// separate class - they read/write _messages, _waiting_for_response etc. directly, shared with
// the panel and launcher files.

#include "assistant-model.h"
#include "device-model.h"
#include "ux-window.h"
#include "os.h"
#include "assistant-markdown.h"
#include <rsutils/string/from.h>

namespace rs2
{
    void assistant_model::draw_greeting(ux_window& win, float avail_h)
    {
        // A fixed-size child (rather than Dummy() spacers) always consumes exactly avail_h, so the
        // input row drawn after it stays flush at the panel's bottom edge regardless of content.
        ImGui::BeginChild("##assistant_greeting", { 0.f, avail_h }, false, ImGuiWindowFlags_NoScrollbar);

        const char* line1 = "Ask me anything about RealSense products.";
        const char* line2 = "Try: \"What's the depth range of the D435i?\"";

        ImGui::PushFont(win.get_font());
        float line1_w = ImGui::CalcTextSize(line1).x;
        float line2_w = ImGui::CalcTextSize(line2).x;
        float content_h = ImGui::GetTextLineHeightWithSpacing() * 2.f;
        ImGui::SetCursorPosY(std::max(0.f, (avail_h - content_h) * 0.5f));

        ImGui::SetCursorPosX(std::max(0.f, (ImGui::GetContentRegionAvail().x - line1_w) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, white);
        ImGui::TextUnformatted(line1);
        ImGui::PopStyleColor();

        ImGui::SetCursorPosX(std::max(0.f, (ImGui::GetContentRegionAvail().x - line2_w) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_grey, 0.6f));
        ImGui::TextUnformatted(line2);
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::EndChild();
    }

    void assistant_model::draw_messages(ux_window& win, float avail_h)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, dark_window_background);
        ImGui::BeginChild("##assistant_messages", { 0, avail_h }, false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        float wrap_width = ImGui::GetContentRegionAvail().x - 4.f;
        for (size_t i = 0; i < _messages.size(); i++)
            draw_message(win, _messages[i], i, wrap_width);

        if (!_messages.empty() && _messages.back().streaming)
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    void assistant_model::draw_message(ux_window& win, assistant_chat_message& msg, size_t index, float wrap_width)
    {
        ImGui::PushID(static_cast<int>(index));
        ImGui::Spacing();

        if (msg.role == assistant_chat_message::user_role)
        {
            const float h_pad = 10.f, v_pad = 8.f;
            float max_text_w = wrap_width * 0.8f - 2.f * h_pad;

            // PushFont before CalcTextSize: measuring with the default font while rendering with
            // win.get_font() sizes the bubble - and its wrap boundary - for the wrong font's metrics.
            ImGui::PushFont(win.get_font());
            ImVec2 text_size = ImGui::CalcTextSize(msg.text.c_str(), nullptr, false, max_text_w);
            float bubble_w = text_size.x + 2.f * h_pad;
            float bubble_h = text_size.y + 2.f * v_pad;

            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - bubble_w);
            auto pos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(pos, { pos.x + bubble_w, pos.y + bubble_h },
                ImColor(header_window_bg), 12.f);

            ImGui::SetCursorScreenPos({ pos.x + h_pad, pos.y + v_pad });
            ImGui::PushStyleColor(ImGuiCol_Text, white);
            // PushTextWrapPos takes a window-LOCAL x, not a screen coordinate.
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + max_text_w);
            ImGui::TextWrapped("%s", msg.text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::PopFont();

            ImGui::SetCursorScreenPos({ pos.x, pos.y + bubble_h + 4.f });
        }
        else
        {
            ImGui::PushFont(win.get_font());
            ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
            std::string name = rsutils::string::from() << " " << textual_icons::comments << "  RealSenseAI Assistant";
            ImGui::TextUnformatted(name.c_str());
            ImGui::PopStyleColor();

            if (!msg.errored)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_grey, 0.6f));
                ImGui::TextUnformatted("AI-generated content may be incorrect");
                ImGui::PopStyleColor();
            }
            ImGui::PopFont();

            if (msg.streaming && msg.text.empty() && !msg.errored)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
                int dots = 1 + (int(win.time() * 2.0) % 3);
                ImGui::TextUnformatted(std::string(dots, '.').c_str());
                ImGui::PopStyleColor();
            }
            else if (msg.errored)
            {
                ImGui::PushFont(win.get_font());
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap_width);
                ImGui::PushStyleColor(ImGuiCol_Text, redish);
                ImGui::TextWrapped("%s", msg.error_text.c_str());
                ImGui::PopStyleColor();
                ImGui::PopTextWrapPos();

                if (index + 1 == _messages.size() && !_waiting_for_response)
                {
                    if (ImGui::SmallButton(textual_icons::refresh))
                        resend_last_user_message();
                    if (ImGui::IsItemHovered())
                    {
                        RsImGui::CustomTooltip("Try again");
                        win.link_hovered();
                    }
                }
                ImGui::PopFont();
            }
            else
            {
                assistant_detail::draw_markdown_body(win, msg.text, wrap_width, msg.md_cache);

                if (!msg.citations.empty())
                {
                    ImGui::PushFont(win.get_font());
                    ImGui::Spacing();
                    for (auto&& c : msg.citations)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, light_blue);
                        ImGui::TextUnformatted(c.label.empty() ? c.url.c_str() : c.label.c_str());
                        ImGui::PopStyleColor();
                        if (ImGui::IsItemClicked())
                            open_url(c.url.c_str());
                        if (ImGui::IsItemHovered())
                        {
                            RsImGui::CustomTooltip(c.url.c_str());
                            win.link_hovered();
                        }
                    }
                    ImGui::PopFont();
                }

                if (!msg.streaming)
                {
                    ImGui::PushFont(win.get_font());
                    ImGui::PushStyleColor(ImGuiCol_Text, alpha(light_grey, 0.6f));
                    std::string footer = rsutils::string::from() << int(msg.latency_ms) << " ms";
                    ImGui::TextUnformatted(footer.c_str());
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    bool copy_clicked = ImGui::SmallButton(textual_icons::copy);
                    if (ImGui::IsItemHovered())
                    {
                        RsImGui::CustomTooltip("Copy");
                        win.link_hovered();
                    }

                    // Reactions are attributed by the API to "the conversation's latest answer", so
                    // only the last message can meaningfully take one.
                    if (index + 1 == _messages.size())
                    {
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Text, msg.reaction == 1 ? green : light_grey);
                        if (ImGui::SmallButton(textual_icons::thumbs_up))
                        {
                            msg.reaction = (msg.reaction == 1) ? 0 : 1;
                            send_reaction(msg.reaction);
                        }
                        ImGui::PopStyleColor();
                        if (ImGui::IsItemHovered())
                        {
                            RsImGui::CustomTooltip("Good answer");
                            win.link_hovered();
                        }

                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Text, msg.reaction == -1 ? redish : light_grey);
                        if (ImGui::SmallButton(textual_icons::thumbs_down))
                        {
                            msg.reaction = (msg.reaction == -1) ? 0 : -1;
                            send_reaction(msg.reaction);
                        }
                        ImGui::PopStyleColor();
                        if (ImGui::IsItemHovered())
                        {
                            RsImGui::CustomTooltip("Bad answer");
                            win.link_hovered();
                        }
                    }

                    ImGui::PopFont();
                    if (copy_clicked)
                        glfwSetClipboardString(win, msg.text.c_str());
                }
            }
        }

        ImGui::PopID();
    }
}
