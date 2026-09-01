// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#ifdef ENABLE_AI_ASSISTANT
#include <curl/curl.h>
#include <curl/easy.h>
#include <thread>
#include "assistant-chat-config.h"
#endif

#include "assistant-chat-client.h"
#include <rsutils/string/from.h>

namespace rs2
{
    namespace assistant
    {
#ifndef ENABLE_AI_ASSISTANT
        // Dummy implementation - the assistant was not built into this copy of the viewer.
        assistant_chat_client::assistant_chat_client() {}
        assistant_chat_client::~assistant_chat_client() {}
        // Unlike the real implementation these run synchronously on the caller's (UI) thread, so
        // callbacks are called directly - routing through `invoke` would enqueue onto
        // assistant_model's dispatch_queue and deadlock waiting for the UI thread to drain itself.
        void assistant_chat_client::send(const std::string&, const std::string&,
            invoke_fn, event_callback, error_callback on_error)
        {
            on_error("The AI Assistant was not built into this copy of RealSense Viewer.");
        }
        void assistant_chat_client::cancel() {}
        size_t assistant_chat_client::on_curl_data(const char*, size_t) { return 0; }
        void assistant_chat_client::run(std::string, std::string, invoke_fn, event_callback, error_callback) {}
        // Deliberately never calls on_result: "unhealthy" means a real check against a real URL
        // failed, which isn't true here - the status dot should stay hidden, not show red.
        void assistant_chat_client::check_health(invoke_fn, std::function<void(bool)>) {}
        void assistant_chat_client::send_reaction(const std::string&, int, invoke_fn, error_callback) {}

#else

        static const long REQUEST_TIMEOUT_SEC = 120L; // overall cap; SSE answers can stream for a while

        namespace
        {
            std::string build_chat_request_body(const std::string& message, const std::string& conversation_id)
            {
                rsutils::json body_json;
                body_json["message"] = message;
                if (!conversation_id.empty())
                    body_json["conversationId"] = conversation_id;
                return body_json.dump();
            }

            // Wires up everything curl needs for the POST /api/chat/stream request; the caller
            // still owns `headers`'s lifetime (curl_slist_free_all) since it must outlive perform().
            void configure_chat_request(CURL* curl, const std::string& url, const std::string& body,
                curl_slist* headers, curl_write_callback write_cb, void* write_userdata)
            {
                curl_easy_reset(curl);
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
                curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT_SEC);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, REQUEST_TIMEOUT_SEC);
                curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, write_userdata);
            }

            // Maps a finished transfer's curl/HTTP result to on_error, if it wasn't a clean 2xx (that
            // case, including a 2xx with no explicit SSE 'done'/'error' event, has nothing to report).
            void report_transfer_result(CURLcode res, long http_status, const invoke_fn& invoke, const error_callback& on_error)
            {
                if (res != CURLE_OK)
                {
                    std::string message_text = rsutils::string::from() << "Couldn't reach the assistant: " << curl_easy_strerror(res);
                    safe_invoke(invoke, [on_error, message_text]() { on_error(message_text); });
                }
                else if (http_status == 429)
                {
                    safe_invoke(invoke, [on_error]() { on_error("Too many requests - please wait a moment and try again."); });
                }
                else if (http_status >= 400)
                {
                    std::string message_text = rsutils::string::from() << "The assistant returned an error (HTTP " << http_status << ").";
                    safe_invoke(invoke, [on_error, message_text]() { on_error(message_text); });
                }
            }
        }

        size_t assistant_chat_client::write_trampoline(char* ptr, size_t size, size_t nmemb, void* userdata)
        {
            return static_cast<assistant_chat_client*>(userdata)->on_curl_data(ptr, size * nmemb);
        }

        assistant_chat_client::assistant_chat_client()
        {
            _curl = curl_easy_init();
        }

        assistant_chat_client::~assistant_chat_client()
        {
            if (_curl)
                curl_easy_cleanup(static_cast<CURL*>(_curl));
        }

        void assistant_chat_client::send(const std::string& message, const std::string& conversation_id,
            invoke_fn invoke, event_callback on_event, error_callback on_error)
        {
            if (_busy)
            {
                safe_invoke(invoke, [on_error]() { on_error("The assistant is still answering the previous message."); });
                return;
            }
            _busy = true;
            _cancel_requested = false;

            auto me = shared_from_this();
            std::thread t([me, message, conversation_id, invoke, on_event, on_error]() mutable {
                try
                {
                    me->run(message, conversation_id, invoke, on_event, on_error);
                }
                catch (const std::exception& ex)
                {
                    auto what = std::string(ex.what());
                    safe_invoke(invoke, [on_error, what]() { on_error(what); });
                }
                catch (...)
                {
                    safe_invoke(invoke, [on_error]() { on_error("Unknown error while talking to the assistant."); });
                }
                me->_busy = false;
            });
            t.detach();
        }

        void assistant_chat_client::cancel()
        {
            _cancel_requested = true;
        }

        size_t assistant_chat_client::on_curl_data(const char* ptr, size_t bytes)
        {
            if (_cancel_requested)
                return 0; // returning less than `bytes` tells curl to abort the transfer

            bool ok = _sse_parser.feed(ptr, bytes, [this](const sse_event& event) {
                auto on_event = _active_on_event;
                if (on_event)
                    safe_invoke(_active_invoke, [on_event, event]() { on_event(event); });
            });
            if (!ok)
            {
                auto on_error = _active_on_error;
                if (on_error)
                    safe_invoke(_active_invoke, [on_error]() { on_error("The assistant's response was too large to process."); });
                return 0; // abort the transfer, same as a user-requested cancel
            }
            return bytes;
        }

        void assistant_chat_client::run(std::string message, std::string conversation_id,
            invoke_fn invoke, event_callback on_event, error_callback on_error)
        {
            if (!_curl)
            {
                safe_invoke(invoke, [on_error]() { on_error("Could not initialize the HTTP client."); });
                return;
            }

            _sse_parser.reset();
            _active_invoke = invoke;
            _active_on_event = on_event;
            _active_on_error = on_error;

            std::string body = build_chat_request_body(message, conversation_id);
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, "X-RS-Integration: viewer");

            std::string chat_url = std::string(BASE_URL) + "/api/chat/stream";
            auto* curl = static_cast<CURL*>(_curl);
            configure_chat_request(curl, chat_url, body, headers, write_trampoline, this);

            auto res = curl_easy_perform(curl);

            long http_status = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
            curl_slist_free_all(headers);

            _active_invoke = nullptr;
            _active_on_event = nullptr;
            _active_on_error = nullptr;

            if (_cancel_requested || _sse_parser.overflowed())
                return; // on_curl_data already reported this (or it's a silent user cancel)

            report_transfer_result(res, http_status, invoke, on_error);
        }
#endif
    }
}
