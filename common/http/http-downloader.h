// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "curl-wrapper.h"
#include <cstdint>
#include <string>
#include <functional>
#include <sstream>
#include <vector>

namespace rs2
{
    namespace http
    {

        enum class callback_result { CONTINUE_DOWNLOAD, STOP_DOWNLOAD };
        typedef std::function<callback_result(uint64_t dl_current_bytes, uint64_t dl_total_bytes)> user_callback_func_type;

        // Service class for downloading a file from an HTTP URL. The transfer runs through
        // curl_wrapper, which confines all libcurl usage; this class only shapes the request and
        // routes the body to a stream / vector / file.
        class http_downloader
        {
        public:
            //  The optional callback function provides 2 major capabilities:
            //    - Current status about the download progress
            //    - Control the download process (stop/continue) using the return value of the callback function
            bool download_to_stream(const std::string& url, std::stringstream &output, user_callback_func_type user_callback_func = user_callback_func_type());
            bool download_to_bytes_vector(const std::string& url, std::vector<uint8_t> &output, user_callback_func_type user_callback_func = user_callback_func_type());
            bool download_to_file(const std::string& url, const std::string &file_name, user_callback_func_type user_callback_func = user_callback_func_type());

        private:
            curl_wrapper _curl;
        };
    }
}
