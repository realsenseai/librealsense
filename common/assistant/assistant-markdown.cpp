// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "assistant-markdown.h"
#include "device-model.h"
#include "ux-window.h"
#include "os.h"
#include <regex>
#include <cctype>

namespace rs2
{
    namespace
    {
        // A rule line is only -/*/_ and spaces, but must contain at least one of them - a line of
        // pure spaces is blank, not a rule.
        bool is_rule_line(const std::string& s)
        {
            return !s.empty() && s.find_first_not_of("-*_ ") == std::string::npos
                && s.find_first_of("-*_") != std::string::npos;
        }

        // Structural line checks only; strip_markdown_line() below does the actual stripping.
        // '**bold**' and links become styled spans instead - see find_first_span().
        bool starts_heading_or_rule(const std::string& s)
        {
            if (s.empty()) return false;
            if (s.rfind("#", 0) == 0) return true;
            return is_rule_line(s);
        }

        bool starts_bullet(const std::string& s)
        {
            return s.rfind("- ", 0) == 0 || s.rfind("* ", 0) == 0;
        }

        // Merges a soft mid-sentence '\n' into the previous logical line (joined by a space) so our
        // own word-wrap re-flows it, instead of treating it as a paragraph break with visible gaps.
        // A line breaks the merge if it or the next one is blank, a heading, a rule, or a bullet.
        std::vector<std::string> to_logical_lines(const std::string& text)
        {
            std::vector<std::string> physical;
            size_t start = 0;
            while (start <= text.size())
            {
                auto end = text.find('\n', start);
                physical.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
                if (end == std::string::npos) break;
                start = end + 1;
            }

            std::vector<std::string> logical;
            bool prev_continuable = false; // can the previous logical line accept a merge?
            for (auto&& p : physical)
            {
                bool p_is_break = p.empty() || starts_heading_or_rule(p) || starts_bullet(p);
                if (!logical.empty() && prev_continuable && !p_is_break)
                    logical.back() += " " + p;
                else
                    logical.push_back(p);

                prev_continuable = !p.empty() && !starts_heading_or_rule(p);
            }
            return logical;
        }

        // Strips '#'/bullet markers and '**' outright. Bold is intentionally not styled (inline
        // styling that can wrap mid-phrase broke the word-layout engine); links stay styled, via
        // find_first_span() below.
        std::string strip_markdown_line(std::string line)
        {
            if (line.rfind("#", 0) == 0)
            {
                auto text_start = line.find_first_not_of("# ");
                line = text_start == std::string::npos ? "" : line.substr(text_start);
            }
            else if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0)
            {
                line = line.substr(2); // bullet marker itself is drawn via ImGui::Bullet(), not text
            }

            std::string out;
            out.reserve(line.size());
            for (size_t i = 0; i < line.size(); )
            {
                if (line.compare(i, 2, "**") == 0) { i += 2; continue; }
                out += line[i++];
            }
            return out;
        }

        enum class span_kind { link };

        struct span_match
        {
            size_t start = std::string::npos;
            size_t length = 0;
            span_kind kind;
            std::string label; // display text (the link's label, or its url if there was no label)
            std::string url;
        };

        // Finds the next "[label](url)" or bare "https://..." occurrence at or after `from`.
        span_match find_first_span(const std::string& line, size_t from)
        {
            static const std::regex md_link_re(R"(\[([^\]]+)\]\((https?://[^)\s]+)\))");
            static const std::regex bare_url_re(R"(https?://[^\s)\]]+)");

            std::string tail = line.substr(from);
            std::smatch md_m, url_m;
            bool has_md = std::regex_search(tail, md_m, md_link_re);
            bool has_url = std::regex_search(tail, url_m, bare_url_re);
            bool link_is_md = has_md && (!has_url || (size_t)md_m.position() <= (size_t)url_m.position());

            span_match m;
            if (has_md || has_url)
            {
                m.kind = span_kind::link;
                if (link_is_md)
                {
                    m.start = from + md_m.position();
                    m.length = md_m.length();
                    m.label = md_m[1].str();
                    m.url = md_m[2].str();
                }
                else
                {
                    m.start = from + url_m.position();
                    m.length = url_m.length();
                    m.label = url_m.str();
                    m.url = url_m.str();
                }
            }
            return m;
        }

        enum class word_kind { plain, link };

        struct word_token
        {
            std::string text;
            word_kind kind;
            std::string url; // link only
        };

        void push_words(const std::string& s, word_kind kind, const std::string& url, std::vector<word_token>& out)
        {
            size_t i = 0;
            while (i < s.size())
            {
                while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
                size_t start = i;
                while (i < s.size() && !std::isspace((unsigned char)s[i])) i++;
                if (i > start)
                    out.push_back({ s.substr(start, i - start), kind, url });
            }
        }

        // Breaks a line into individual words (never a multi-word chunk), each tagged with its
        // style, so the caller can lay out and wrap one word at a time instead of handing a whole
        // run to ImGui::TextWrapped.
        std::vector<word_token> tokenize_line(const std::string& line)
        {
            std::vector<word_token> words;
            size_t pos = 0;
            while (pos <= line.size())
            {
                auto m = find_first_span(line, pos);
                std::string plain = line.substr(pos, m.start == std::string::npos ? std::string::npos : m.start - pos);
                push_words(plain, word_kind::plain, {}, words);
                if (m.start == std::string::npos)
                    break;
                push_words(m.label, word_kind::link, m.url, words);
                pos = m.start + m.length;
            }
            return words;
        }
    }

    namespace assistant_detail
    {
        // Parsing (regex link/URL matching, per-word splitting) only reruns when the source text
        // changed since the last call - once a message stops streaming its text is immutable, so
        // this makes re-drawing an old message on every frame just replay cached words, not re-parse.
        static void rebuild_markdown_cache(const std::string& text, markdown_cache& cache)
        {
            cache.source = text;
            cache.lines.clear();

            for (auto&& raw_line : to_logical_lines(text))
            {
                markdown_line ml;
                bool is_heading = raw_line.rfind("#", 0) == 0;

                if (raw_line.empty())
                    ml.kind = markdown_line::kind::blank;
                else if (!is_heading && is_rule_line(raw_line))
                    ml.kind = markdown_line::kind::rule;
                else
                {
                    ml.kind = is_heading ? markdown_line::kind::heading
                        : starts_bullet(raw_line) ? markdown_line::kind::bullet
                        : markdown_line::kind::text;
                    for (auto&& w : tokenize_line(strip_markdown_line(raw_line)))
                        ml.words.push_back({ w.text, w.kind == word_kind::link, w.url });
                }
                cache.lines.push_back(std::move(ml));
            }
        }

        // A deliberately lightweight, line-oriented renderer: each markdown line becomes one
        // flowing plain-text line, except headings (bigger font) and horizontal rules (a real
        // separator) - the two cases that are per-line, not inline-mixed-style, formatting.
        void draw_markdown_body(ux_window& win, const std::string& text, float wrap_width, markdown_cache& cache)
        {
            if (cache.source != text)
                rebuild_markdown_cache(text, cache);

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
