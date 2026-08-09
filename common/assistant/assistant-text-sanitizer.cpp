// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "assistant-text-sanitizer.h"
#include <regex>
#include <utility>
#include <cstring>

namespace rs2
{
    namespace assistant_detail
    {
        std::string sanitize_for_display(const std::string& raw)
        {
            // Citations are shown separately as a clickable chip list, so drop the inline CJK-
            // bracketed reference marker outright rather than leaving residue behind.
            static const std::regex citation_marker_re("\xE3\x80\x90.*?\xE3\x80\x91");
            std::string in = std::regex_replace(raw, citation_marker_re, "");

            static const std::pair<const char*, const char*> replacements[] = {
                { "\xE2\x80\x98", "'" },   // left single quote
                { "\xE2\x80\x99", "'" },   // right single quote
                { "\xE2\x80\x9C", "\"" },  // left double quote
                { "\xE2\x80\x9D", "\"" },  // right double quote
                { "\xE2\x80\x93", "-" },   // en dash
                { "\xE2\x80\x94", "-" },   // em dash
                { "\xE2\x80\xA6", "..." }, // ellipsis
                { "\xE2\x84\xA2", "" },    // trademark sign
                { "\xC2\xAE", "" },        // registered sign
                { "\xC2\xB0", " deg" },    // degree sign
                { "\xC2\xB1", "+/-" },     // plus-minus sign
                { "\xC3\x97", "x" },       // multiplication sign
            };

            std::string out;
            out.reserve(in.size());
            size_t i = 0;
            while (i < in.size())
            {
                unsigned char c = static_cast<unsigned char>(in[i]);
                if (c < 0x80) { out += in[i]; ++i; continue; }

                bool matched = false;
                for (auto&& r : replacements)
                {
                    size_t len = std::strlen(r.first);
                    if (in.compare(i, len, r.first) == 0)
                    {
                        out += r.second;
                        i += len;
                        matched = true;
                        break;
                    }
                }
                if (matched)
                    continue;

                i += (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
            }
            return out;
        }
    }
}
