// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "assistant-image-cache.h"
#include <string>
#include <vector>

namespace rs2
{
    class ux_window;

    namespace assistant_detail
    {
        struct markdown_word
        {
            std::string text;
            bool is_link = false;
            std::string url; // set only when is_link
        };

        struct markdown_line
        {
            enum class kind { blank, rule, heading, bullet, text, image } kind = kind::text;
            std::vector<markdown_word> words; // unused for blank/rule/image
            std::string image_url; // set only for kind::image
        };

        // Owned by the message itself (see assistant_chat_message::md_cache) and reused across
        // frames: draw_markdown_body() only re-parses when `source` no longer matches the text.
        struct markdown_cache
        {
            std::string source;
            std::vector<markdown_line> lines;
        };

        // Pure parsing (no ImGui/UI dependency, defined in assistant-markdown-parser.cpp) - split
        // out from rendering specifically so it's unit-testable without pulling in ImGui.
        std::vector<markdown_line> parse_markdown(const std::string& text);

        // Renders one assistant reply's markdown as flowing ImGui text: headings get a bigger font,
        // rules become a real separator, links are styled/clickable, images/gifs are fetched via
        // `images` and drawn inline. `cache` must be the same instance across calls for a message.
        void draw_markdown_body(ux_window& win, const std::string& text, float wrap_width,
            markdown_cache& cache, assistant::assistant_image_cache& images, const assistant::invoke_fn& invoke);
    }
}
