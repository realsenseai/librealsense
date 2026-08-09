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
            void reset() { _buffer.clear(); } // drop any partial frame left over from a prior request
            void feed(const char* data, size_t len, const std::function<void(const sse_event&)>& on_event);

        private:
            std::string _buffer; // raw bytes accumulated across calls until a full frame is seen
        };
    }
}
