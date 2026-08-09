// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "assistant-sse-event.h"
#include "assistant-sse-parser.h"
#include <atomic>
#include <memory>

namespace rs2
{
    namespace assistant
    {
        // Streams one POST /api/chat/stream request on a detached background thread, parsing the
        // response's SSE incrementally. Has no ImGui/UI knowledge - every event/error is delivered
        // through the caller-supplied `invoke`, so callbacks always run where `invoke` marshals them.

        // check_health()/send_reaction() are unrelated one-shot requests, defined in
        // assistant-chat-http.cpp; the streaming lifecycle here (send/cancel/run) owns _curl/_busy.
        class assistant_chat_client : public std::enable_shared_from_this<assistant_chat_client>
        {
        public:
            assistant_chat_client();
            ~assistant_chat_client();

            // Ignored (calls on_error immediately) if a previous send() is still in flight -
            // callers should disable their "send" UI while busy() is true.
            void send(const std::string& message, const std::string& conversation_id,
                invoke_fn invoke, event_callback on_event, error_callback on_error);

            // Best-effort abort of an in-flight request; the write-callback checks this flag and
            // tells curl to abort on its next invocation.
            void cancel();

            bool busy() const { return _busy; }

            // One-shot GET /api/health probe on its own background thread/curl handle (independent
            // of send()'s), so it never contends with an in-flight chat request. `on_result` is
            // called with true only on an HTTP 200 response.
            void check_health(invoke_fn invoke, std::function<void(bool)> on_result);

            // Fire-and-forget POST /api/reactions on the conversation's latest answer. `value` is
            // 1 (up), -1 (down), or 0 (clear a previously-sent reaction). Runs on its own background
            // thread/curl handle, independent of send()'s.
            void send_reaction(const std::string& conversation_id, int value,
                invoke_fn invoke, error_callback on_error);

        private:
            void run(std::string message, std::string conversation_id,
                invoke_fn invoke, event_callback on_event, error_callback on_error);

            // Called from curl's write-callback (background thread only, one request at a time).
            size_t on_curl_data(const char* ptr, size_t bytes);
            static size_t write_trampoline(char* ptr, size_t size, size_t nmemb, void* userdata);

            void* _curl = nullptr;               // opaque CURL*; keeps <curl/curl.h> out of this header
            sse_frame_parser _sse_parser;
            invoke_fn _active_invoke;             // valid only while a request is in flight (busy() == true)
            event_callback _active_on_event;
            error_callback _active_on_error;
            std::atomic<bool> _cancel_requested{ false };
            std::atomic<bool> _busy{ false };
        };
    }
}
