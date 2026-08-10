// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <string>

namespace rs2
{
    namespace assistant_detail
    {
        // UTF-8 lead bytes encode their sequence length in their high bits; continuation bytes (the
        // 2nd/3rd/4th byte of a multi-byte sequence) always fall in the 0x80-0xBF range.
        constexpr unsigned char UTF8_CONTINUATION_MIN = 0x80;
        constexpr unsigned char UTF8_2BYTE_LEAD_MIN = 0xC0;
        constexpr unsigned char UTF8_3BYTE_LEAD_MIN = 0xE0;
        constexpr unsigned char UTF8_4BYTE_LEAD_MIN = 0xF0;

        inline bool is_utf8_continuation_byte(unsigned char c)
        {
            return c >= UTF8_CONTINUATION_MIN && c < UTF8_2BYTE_LEAD_MIN;
        }

        // Drops inline citation markers and maps common Unicode punctuation (smart quotes, dashes,
        // (R)/(TM), degree signs, ...) to ASCII look-alikes, since the loaded UI font is ASCII-only.
        std::string sanitize_for_display(const std::string& raw);
    }
}
