// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <string>

namespace rs2
{
    namespace assistant_detail
    {
        // Drops inline citation markers and maps common Unicode punctuation (smart quotes, dashes,
        // (R)/(TM), degree signs, ...) to ASCII look-alikes, since the loaded UI font is ASCII-only.
        std::string sanitize_for_display(const std::string& raw);
    }
}
