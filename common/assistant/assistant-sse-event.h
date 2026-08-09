// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <rsutils/json.h>
#include <string>
#include <functional>
#include <exception>

namespace rs2
{
    namespace assistant
    {
        enum class sse_event_type { conversation_id, chunk, tool_use, annotations, usage, done, error, unknown };

        // One parsed "data: {...}" frame from the assistant's SSE stream. `payload` is the raw
        // JSON body; callers pull out the fields relevant to `type` (see API.md for the shapes).
        struct sse_event
        {
            sse_event_type type;
            rsutils::json payload;
        };

        using invoke_fn = std::function<void(std::function<void()>)>;
        using event_callback = std::function<void(const sse_event&)>;
        using error_callback = std::function<void(const std::string& message)>;

        // invoke() blocks its background-thread caller until the UI thread drains it, throwing if
        // that doesn't happen in time (e.g. the viewer is shutting down). Route every background-
        // thread invoke() call through this instead, so a stalled UI thread can't std::terminate() it.
        inline void safe_invoke(const invoke_fn& invoke, std::function<void()> action)
        {
            try
            {
                invoke(std::move(action));
            }
            catch (const std::exception&)
            {
                // best-effort UI notification only; nothing else to do if the UI thread isn't listening
            }
        }
    }
}
