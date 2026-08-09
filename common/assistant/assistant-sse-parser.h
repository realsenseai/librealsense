// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "assistant-sse-event.h"

namespace rs2
{
    namespace assistant
    {
        // Buffers raw bytes from a chunked HTTP response and extracts complete "data: ...\n\n" SSE
        // frames, parsing each into an sse_event. No curl/threading knowledge - feed() is plain
        // synchronous code, safe to call from whatever thread owns the bytes.
        class sse_frame_parser
        {
        public:
            void reset() { _buffer.clear(); _overflowed = false; } // drop a partial frame from a prior request

            // Returns false (and stops buffering) if the response grew past a hard cap without ever
            // completing a frame - a stalled/malformed server should not be able to grow this forever.
            bool feed(const char* data, size_t len, const std::function<void(const sse_event&)>& on_event);
            bool overflowed() const { return _overflowed; }

        private:
            std::string _buffer; // raw bytes accumulated across calls until a full frame is seen
            bool _overflowed = false;
        };
    }
}
