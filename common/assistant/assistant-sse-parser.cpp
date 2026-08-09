// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "assistant-sse-parser.h"
#include <rsutils/easylogging/easyloggingpp.h>

namespace rs2
{
    namespace assistant
    {
        namespace
        {
            sse_event_type parse_event_type(const std::string& type)
            {
                if (type == "conversationId") return sse_event_type::conversation_id;
                if (type == "chunk") return sse_event_type::chunk;
                if (type == "toolUse") return sse_event_type::tool_use;
                if (type == "annotations") return sse_event_type::annotations;
                if (type == "usage") return sse_event_type::usage;
                if (type == "done") return sse_event_type::done;
                if (type == "error") return sse_event_type::error;
                return sse_event_type::unknown;
            }
        }

        void sse_frame_parser::feed(const char* data, size_t len, const std::function<void(const sse_event&)>& on_event)
        {
            // libcurl delivers arbitrary byte chunks with no alignment to SSE frame/line boundaries,
            // so bytes accumulate in _buffer until a complete frame ("...\n\n") can be pulled out.
            _buffer.append(data, len);

            for (;;)
            {
                auto frame_end = _buffer.find("\n\n");
                if (frame_end == std::string::npos)
                    break;

                std::string frame = _buffer.substr(0, frame_end);
                _buffer.erase(0, frame_end + 2);

                // Join every "data: ..." line in the frame (tolerate an optional leading space).
                std::string data_json;
                size_t line_start = 0;
                while (line_start <= frame.size())
                {
                    auto line_end = frame.find('\n', line_start);
                    auto line = frame.substr(line_start, line_end == std::string::npos ? std::string::npos : line_end - line_start);
                    if (line.rfind("data:", 0) == 0)
                    {
                        auto value = line.substr(5);
                        if (!value.empty() && value.front() == ' ')
                            value.erase(0, 1);
                        data_json += value;
                    }
                    if (line_end == std::string::npos)
                        break;
                    line_start = line_end + 1;
                }

                if (data_json.empty())
                    continue;

                // A malformed or unexpectedly-shaped frame must never throw out of feed() - it can
                // run on libcurl's own call stack, which can't safely propagate a C++ exception.
                try
                {
                    auto j = rsutils::json::parse(data_json, nullptr, false);
                    if (j.is_discarded() || !j.is_object())
                    {
                        LOG_WARNING("Assistant: could not parse SSE frame: " << data_json);
                        continue;
                    }
                    on_event({ parse_event_type(j.value("type", std::string())), j });
                }
                catch (const std::exception& ex)
                {
                    LOG_WARNING("Assistant: error handling SSE frame: " << ex.what());
                }
            }
        }
    }
}
