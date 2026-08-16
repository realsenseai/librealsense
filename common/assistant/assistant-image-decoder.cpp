// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "assistant-image-decoder.h"

// Self-contained rather than relying on ux-window.cpp's implementation being in the same link
// target - the unit-test target this file also needs to link into doesn't have it. STATIC keeps
// these symbols private to this TU, so there's no clash with any other definition either way.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../../third-party/stb_image.h"

#include <cstring>
#include <algorithm>

namespace rs2
{
    namespace assistant_detail
    {
        namespace
        {
            const int RGBA_CHANNELS = 4;
            const int MIN_FRAME_DELAY_MS = 20; // guards a spin-fast/zero-delay GIF frame

            bool looks_like_gif(const uint8_t* data, size_t len)
            {
                return len >= 4 && std::memcmp(data, "GIF8", 4) == 0;
            }
        }

        decoded_image decode_image_bytes(const uint8_t* data, size_t len)
        {
            decoded_image out;
            if (!data || len == 0)
                return out;

            if (looks_like_gif(data, len))
            {
                int* delays = nullptr;
                int w = 0, h = 0, frame_count = 0, comp = 0;
                auto* pixels = stbi_load_gif_from_memory(data, (int)len, &delays, &w, &h, &frame_count, &comp, RGBA_CHANNELS);
                if (pixels && frame_count > 0)
                {
                    out.width = w;
                    out.height = h;
                    size_t frame_bytes = (size_t)w * h * RGBA_CHANNELS;
                    for (int i = 0; i < frame_count; i++)
                    {
                        decoded_image_frame frame;
                        frame.rgba.assign(pixels + i * frame_bytes, pixels + (i + 1) * frame_bytes);
                        frame.delay_ms = delays ? std::max(delays[i], MIN_FRAME_DELAY_MS) : MIN_FRAME_DELAY_MS;
                        out.frames.push_back(std::move(frame));
                    }
                }
                if (pixels) stbi_image_free(pixels);
                if (delays) free(delays); // stb allocates this separately from the pixel buffer
                return out;
            }

            int w = 0, h = 0, comp = 0;
            auto* pixels = stbi_load_from_memory(data, (int)len, &w, &h, &comp, RGBA_CHANNELS);
            if (pixels)
            {
                out.width = w;
                out.height = h;
                decoded_image_frame frame;
                frame.rgba.assign(pixels, pixels + (size_t)w * h * RGBA_CHANNELS);
                frame.delay_ms = 0; // static image
                out.frames.push_back(std::move(frame));
                stbi_image_free(pixels);
            }
            return out;
        }
    }
}
