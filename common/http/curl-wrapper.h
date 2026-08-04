// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace rs2
{
    namespace http
    {
        // The one and only place that includes libcurl. Owns a single easy handle and runs the
        // one-time global init. http_uploader / http_downloader build their requests through this
        // curl-agnostic API and never see a curl type, so a libcurl API change stays in one file.
        // Compiles to a no-op when curl isn't linked (BUILD_WITH_LIBCURL).
        class curl_wrapper
        {
        public:
            // Receives a chunk of the response/download body; return false to abort the transfer.
            typedef std::function< bool( const char * data, size_t len ) > write_func;
            // Throttled progress: (bytes so far, total); return false to abort. total may be 0 if unknown.
            typedef std::function< bool( uint64_t now, uint64_t total ) > progress_func;

            curl_wrapper();
            ~curl_wrapper();
            curl_wrapper( const curl_wrapper & ) = delete;
            curl_wrapper & operator=( const curl_wrapper & ) = delete;

            bool valid() const { return _curl != nullptr; }

            // GET `url`, streaming the body to `on_data`. Optional progress callback. `insecure` skips
            // SSL peer/host verification. Follows redirects; fails on HTTP >= 400. true on success.
            bool get( const std::string & url, const write_func & on_data,
                      const progress_func & on_progress = progress_func(), bool insecure = false );

            // POST `body` to `url` as application/json (response body discarded). true on success.
            bool post_json( const std::string & url, const std::string & body );

        private:
            void * _curl;
        };
    }
}
