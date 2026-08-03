// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <string>

namespace rs2
{
    namespace http
    {
        // Service class for POSTing a body to an HTTP(S) URL. Uses libcurl as the client-side
        // transfer library; all curl usage is confined here, so a libcurl API change stays local
        // (mirrors http_downloader). Compiles to a no-op when curl isn't linked.
        class http_uploader
        {
        public:
            http_uploader();
            ~http_uploader();

            // POST json_body to url as "application/json"; true on success.
            bool upload( const std::string & url, const std::string & json_body );

        private:
            void * _curl;
        };
    }
}
