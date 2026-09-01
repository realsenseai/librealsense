// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace rs2
{
    namespace assistant_detail
    {
        struct decoded_image_frame
        {
            std::vector<uint8_t> rgba; // width * height * 4 bytes
            int delay_ms = 0;          // 0 for a static (single-frame) image
        };

        struct decoded_image
        {
            int width = 0;
            int height = 0;
            std::vector<decoded_image_frame> frames; // empty means decoding failed
        };

        // Decodes an in-memory image file (PNG/JPEG/BMP/... or an animated GIF, detected by
        // content, not by URL/extension). No curl/ImGui/GL dependency, so it's unit-testable.
        decoded_image decode_image_bytes(const uint8_t* data, size_t len);
    }
}
