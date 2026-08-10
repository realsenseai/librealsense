// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "assistant-markdown.h"
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
            span_kind kind = span_kind::link; // unused unless start != npos, but never left indeterminate
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
        std::vector<markdown_line> parse_markdown(const std::string& text)
        {
            std::vector<markdown_line> lines;

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
                lines.push_back(std::move(ml));
            }
            return lines;
        }
    }
}
