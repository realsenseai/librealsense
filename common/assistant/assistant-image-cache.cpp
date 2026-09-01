// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#ifdef ENABLE_AI_ASSISTANT
#include <curl/curl.h>
#include <curl/easy.h>
#include <thread>
#include "assistant-chat-config.h"
#endif

#include "assistant-image-cache.h"
#include "assistant-image-decoder.h"
#include "rendering.h"

namespace rs2
{
    namespace assistant
    {
#ifndef ENABLE_AI_ASSISTANT
        // Dummy implementation - never starts a fetch or calls invoke (which would deadlock the UI
        // thread if invoked synchronously from it); the entry just stays permanently "failed".
        cached_image* assistant_image_cache::get_or_load(const std::string& url, invoke_fn)
        {
            auto& entry = _entries[url];
            entry.state = image_load_state::failed;
            return &entry;
        }
        void assistant_image_cache::fetch(const std::string&, invoke_fn) {}

#else

        namespace
        {
            size_t append_to_buffer(char* ptr, size_t size, size_t nmemb, void* userdata)
            {
                auto* buf = static_cast<std::vector<uint8_t>*>(userdata);
                size_t n = size * nmemb;
                if (buf->size() + n > MAX_IMAGE_BYTES)
                    return 0; // abort the transfer: response grew past the size cap
                buf->insert(buf->end(), ptr, ptr + n);
                return n;
            }
        }

        cached_image* assistant_image_cache::get_or_load(const std::string& url, invoke_fn invoke)
        {
            auto it = _entries.find(url);
            if (it != _entries.end())
                return &it->second;

            auto& entry = _entries[url]; // default-constructed: state == loading
            fetch(url, invoke);
            return &entry;
        }

        void assistant_image_cache::fetch(const std::string& url, invoke_fn invoke)
        {
            auto me = shared_from_this();
            std::thread t([me, url, invoke]() {
                std::vector<uint8_t> bytes;
                bool ok = false;
                if (auto* curl = curl_easy_init())
                {
                    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT_SEC);
                    curl_easy_setopt(curl, CURLOPT_TIMEOUT, IMAGE_FETCH_TIMEOUT_SEC);
                    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_to_buffer);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bytes);

                    auto res = curl_easy_perform(curl);
                    long status = 0;
                    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
                    curl_easy_cleanup(curl);
                    ok = (res == CURLE_OK && status == 200);
                }

                assistant_detail::decoded_image decoded;
                if (ok)
                    decoded = assistant_detail::decode_image_bytes(bytes.data(), bytes.size());

                safe_invoke(invoke, [me, url, decoded = std::move(decoded)]() {
                    auto found = me->_entries.find(url);
                    if (found == me->_entries.end())
                        return; // shouldn't happen; defensive
                    auto& entry = found->second;

                    if (decoded.frames.empty())
                    {
                        entry.state = image_load_state::failed;
                        return;
                    }

                    entry.width = decoded.width;
                    entry.height = decoded.height;
                    for (auto&& frame : decoded.frames)
                    {
                        auto tex = std::unique_ptr<texture_buffer>(new texture_buffer());
                        tex->upload_image(decoded.width, decoded.height, (void*)frame.rgba.data());
                        entry.frame_textures.push_back(std::move(tex));
                        entry.frame_delays_ms.push_back(frame.delay_ms);
                    }
                    entry.state = image_load_state::loaded;
                });
            });
            t.detach();
        }
#endif
    }
}
