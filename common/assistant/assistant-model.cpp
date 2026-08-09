// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "assistant-model.h"
#include "ux-window.h"
#include "assistant-text-sanitizer.h"

namespace rs2
{
    assistant_model::assistant_model() {}

    void assistant_model::invoke(std::function<void()> action)
    {
        // Mirrors notification_model::invoke: block the calling (background) thread until the UI
        // thread has drained and executed `action`, so callers never race ahead of state they just
        // asked to be mutated.
        auto sptr_q = std::make_shared<single_consumer_queue<bool>>();
        std::weak_ptr<single_consumer_queue<bool>> wptr_q(sptr_q);

        dispatch_queue.enqueue([wptr_q, action]() {
            try
            {
                action();
                if (auto q = wptr_q.lock()) q->enqueue(true);
            }
            catch (...)
            {
                if (auto q = wptr_q.lock()) q->enqueue(false);
            }
        });

        bool res;
        if (!sptr_q->dequeue(&res, 100000) || !res)
            throw std::runtime_error("Assistant invoke operation failed!");
    }

    void assistant_model::draw(ux_window& win, float bottom_clearance)
    {
        std::function<void()> action;
        while (dispatch_queue.try_dequeue(&action)) action();

#ifndef ENABLE_AI_ASSISTANT
        return; // not built into this copy of the viewer - no launcher, no panel, nothing to show
#endif

        if (!_health_check_started)
        {
            _health_check_started = true;
            std::weak_ptr<assistant_model> self = shared_from_this();
            _client->check_health(
                [self](std::function<void()> a) { if (auto s = self.lock()) s->invoke(a); },
                [self](bool healthy) { if (auto s = self.lock()) s->_health = healthy ? assistant_health::healthy : assistant_health::unhealthy; });
        }

        // The panel takes the launcher's place while open (its own close button gets you back to
        // the launcher) rather than sitting behind/above it - simpler, and avoids the launcher
        // peeking out from behind the panel's rounded corners.
        if (_open)
            draw_panel(win, bottom_clearance);
        else
            draw_launcher_button(win, bottom_clearance);
    }

    void assistant_model::send_current_input()
    {
        std::string text(_input_buffer);
        auto first = text.find_first_not_of(" \t\n\r");
        if (first == std::string::npos)
            return;
        text = text.substr(first, text.find_last_not_of(" \t\n\r") - first + 1);

        assistant_chat_message user_msg;
        user_msg.role = assistant_chat_message::user_role;
        user_msg.text = text;
        _messages.push_back(user_msg);

        _last_user_message = text;
        _input_buffer[0] = '\0';
        dispatch_send(text);
    }

    void assistant_model::resend_last_user_message()
    {
        if (_last_user_message.empty() || _waiting_for_response)
            return;
        dispatch_send(_last_user_message);
    }

    void assistant_model::cancel_current_request()
    {
        // run() deliberately returns without calling on_error on a user-requested cancel, so
        // nothing will update _waiting_for_response/streaming on its own - do it directly here
        // instead of waiting for a callback that isn't coming.
        _client->cancel();
        if (!_messages.empty() && _messages.back().role == assistant_chat_message::assistant_role)
            _messages.back().streaming = false;
        _waiting_for_response = false;
    }

    void assistant_model::new_conversation()
    {
        // A request still in flight will find _messages empty when its callbacks fire and just
        // no-op (see the guard at the top of handle_event/handle_error) - the old stream is
        // abandoned rather than appended to a conversation the user has already cleared.
        _messages.clear();
        _conversation_id.clear();
        _last_user_message.clear();
        _waiting_for_response = false;
    }

    void assistant_model::send_reaction(int value)
    {
        if (_conversation_id.empty())
            return; // nothing to attribute the reaction to yet

        std::weak_ptr<assistant_model> self = shared_from_this();
        _client->send_reaction(_conversation_id, value,
            [self](std::function<void()> a) { if (auto s = self.lock()) s->invoke(a); },
            [](const std::string&) {}); // best-effort feedback; a failed send isn't surfaced to the user
    }

    void assistant_model::dispatch_send(const std::string& text)
    {
        assistant_chat_message reply;
        reply.role = assistant_chat_message::assistant_role;
        reply.streaming = true;
        _messages.push_back(reply);

        _waiting_for_response = true;
        _request_started = std::chrono::steady_clock::now();

        std::weak_ptr<assistant_model> self = shared_from_this();
        _client->send(text, _conversation_id,
            [self](std::function<void()> action) { if (auto s = self.lock()) s->invoke(action); },
            [self](const assistant::sse_event& event) { if (auto s = self.lock()) s->handle_event(event); },
            [self](const std::string& message) { if (auto s = self.lock()) s->handle_error(message); });
    }

    void assistant_model::handle_event(const assistant::sse_event& event)
    {
        if (_messages.empty() || _messages.back().role != assistant_chat_message::assistant_role)
            return;
        auto& msg = _messages.back();
        if (!msg.streaming)
            return; // cancelled or already finished - drop a stale event queued before that happened

        switch (event.type)
        {
        case assistant::sse_event_type::conversation_id:
            _conversation_id = event.payload.value("conversationId", std::string());
            break;
        case assistant::sse_event_type::chunk:
        {
            msg.text += event.payload.value("content", std::string());

            // Re-sanitizing only a bounded trailing window, not the whole message, keeps this
            // O(1) per chunk instead of O(n) - generous enough to cover a citation marker or
            // multi-byte char split across chunks.
            const size_t window = 1024;
            size_t tail_start = msg.text.size() > window ? msg.text.size() - window : 0;
            while (tail_start > 0 && (unsigned char)msg.text[tail_start] >= 0x80 && (unsigned char)msg.text[tail_start] < 0xC0)
                tail_start--; // never cut the window mid-UTF8-sequence
            msg.text = msg.text.substr(0, tail_start) + assistant_detail::sanitize_for_display(msg.text.substr(tail_start));
            break;
        }
        case assistant::sse_event_type::annotations:
            if (auto annotations = event.payload.nested("annotations"))
            {
                for (auto&& a : annotations)
                {
                    assistant_citation c;
                    c.label = assistant_detail::sanitize_for_display(a.value("label", std::string()));
                    c.url = a.value("url", std::string());

                    // Inline citation placeholder, already shown as a clickable chip below the
                    // answer - drop it from the flowing text.
                    auto marker = assistant_detail::sanitize_for_display(a.value("textToReplace", std::string()));
                    if (!marker.empty())
                    {
                        auto pos = msg.text.find(marker);
                        if (pos != std::string::npos)
                            msg.text.erase(pos, marker.size());
                    }

                    if (!c.url.empty())
                        msg.citations.push_back(c);
                }
            }
            break;
        case assistant::sse_event_type::done:
            msg.streaming = false;
            msg.latency_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - _request_started).count();
            _waiting_for_response = false;
            break;
        case assistant::sse_event_type::error:
            msg.errored = true;
            msg.streaming = false;
            msg.error_text = event.payload.value("message", std::string("The assistant reported an error."));
            _waiting_for_response = false;
            break;
        default:
            break; // toolUse / usage / unknown - not used in v1
        }
    }

    void assistant_model::handle_error(const std::string& message)
    {
        if (_messages.empty() || _messages.back().role != assistant_chat_message::assistant_role)
            return;
        auto& msg = _messages.back();
        msg.errored = true;
        msg.streaming = false;
        if (msg.error_text.empty())
            msg.error_text = message;
        _waiting_for_response = false;
    }
}
