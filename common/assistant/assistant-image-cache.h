// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "assistant-sse-event.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace rs2
{
    class texture_buffer;

    namespace assistant
    {
        enum class image_load_state { loading, loaded, failed };

        struct cached_image
        {
            image_load_state state = image_load_state::loading;
            std::vector<std::unique_ptr<texture_buffer>> frame_textures; // one per GIF frame, size 1 if static
            std::vector<int> frame_delays_ms; // parallel to frame_textures, ms; empty entries mean 0
            int width = 0, height = 0;
        };

        // Fetches and decodes images (including animated GIFs) by URL, caching the result so a
        // repeated URL isn't re-fetched. Fetch+decode runs on a background thread; texture upload
        // happens back on the UI thread via `invoke`, since GL calls aren't thread-safe.
        class assistant_image_cache : public std::enable_shared_from_this<assistant_image_cache>
        {
        public:
            // Returns the cache entry for `url`, creating it (and kicking off a background fetch)
            // the first time it's seen. Never null - check ->state before using the textures.
            cached_image* get_or_load(const std::string& url, invoke_fn invoke);

        private:
            void fetch(const std::string& url, invoke_fn invoke);

            std::unordered_map<std::string, cached_image> _entries;
        };
    }
}
