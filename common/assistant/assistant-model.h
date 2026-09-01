// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "assistant-chat-client.h"
#include "assistant-markdown.h"
#include "rendering.h"
#include <rsutils/concurrency/concurrency.h>
#include <vector>
#include <string>
#include <chrono>
#include <memory>

namespace rs2
{
    class ux_window;

    enum class assistant_health { unknown, healthy, unhealthy };

    struct assistant_citation
    {
        std::string label;
        std::string url;
    };

    struct assistant_chat_message
    {
        enum role_t { user_role, assistant_role };

        explicit assistant_chat_message(role_t role) : role(role) {}

        role_t role;
        std::string text;
        std::vector<assistant_citation> citations;
        bool streaming = false;
        bool errored = false;
        std::string error_text;
        std::chrono::system_clock::time_point created_time = std::chrono::system_clock::now();
        double latency_ms = 0.0;
        int reaction = 0; // 1 = thumbs up, -1 = thumbs down, 0 = none sent
        assistant_detail::markdown_cache md_cache; // draw_markdown_body()'s per-message parse cache
    };

    // Floating "Ask RealSenseAI" launcher + chat panel. Mirrors notification_model's dispatch-queue
    // pattern: background SSE events are marshaled onto the UI thread via `invoke`/`dispatch_queue`
    // and only mutate state from draw(). Methods are grouped below by which .cpp file defines them.
    class assistant_model : public std::enable_shared_from_this<assistant_model>
    {
    public:
        assistant_model();

        // `bottom_clearance` is the height of whatever is docked along the window's bottom edge
        // (the log/output panel) so the launcher and chat panel float above it instead of behind it.
        void draw(ux_window& win, float bottom_clearance = 0.f);

        void invoke(std::function<void()> action);

    private:
        single_consumer_queue<std::function<void()>> dispatch_queue;

        // assistant-launcher.cpp
        void draw_launcher_button(ux_window& win, float bottom_clearance);
        void draw_logo(float cx, float cy, float radius); // white disc + RealSense mark, lazy-loaded

        // assistant-panel.cpp
        void draw_panel(ux_window& win, float bottom_clearance);
        bool draw_icon_button(const char* icon); // small circular badge button, used in the header
        void draw_input_row(ux_window& win, float avail_w);

        // assistant-messages.cpp
        void draw_greeting(ux_window& win, float avail_h);
        void draw_messages(ux_window& win, float avail_h);
        void draw_message(ux_window& win, assistant_chat_message& msg, size_t index, float wrap_width);

        void send_current_input();
        void resend_last_user_message();
        void cancel_current_request();
        void new_conversation();
        void send_reaction(int value); // API attributes reactions to the conversation's latest answer
        void dispatch_send(const std::string& text);
        void handle_event(const assistant::sse_event& event);
        void handle_error(const std::string& message);

        bool _open = false;
        bool _expanded = false;
        bool _focus_input_next_frame = false; // set for one frame when the panel just opened
        assistant_health _health = assistant_health::unknown;
        bool _health_check_started = false;
        std::vector<assistant_chat_message> _messages;
        char _input_buffer[2048] = {};
        std::string _conversation_id;
        bool _waiting_for_response = false;
        bool _cancelling = false; // true from cancel until _client->busy() confirms the abort landed
        std::chrono::steady_clock::time_point _request_started;
        std::string _last_user_message; // kept for the retry affordance on error

        std::shared_ptr<assistant::assistant_chat_client> _client = std::make_shared<assistant::assistant_chat_client>();
        std::shared_ptr<assistant::assistant_image_cache> _image_cache = std::make_shared<assistant::assistant_image_cache>();

        texture_buffer _mark_tex;
        bool _mark_loaded = false;
    };
}
