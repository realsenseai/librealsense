// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "curl-wrapper.h"
#include <string>

namespace rs2
{
    namespace http
    {
        // POSTs a body to an HTTP(S) URL. A thin façade over curl_wrapper (which owns all the
        // libcurl usage), mirroring http_downloader. Compiles to a no-op when curl isn't linked.
        class http_uploader
        {
        public:
            // POST json_body to url as "application/json"; true on success.
            bool upload( const std::string & url, const std::string & json_body )
            {
                return _curl.post_json( url, json_body );
            }

        private:
            curl_wrapper _curl;
        };
    }
}
