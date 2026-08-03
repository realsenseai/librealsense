// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#ifdef BUILD_WITH_LIBCURL
#include <curl/curl.h>
#endif

#include "http-uploader.h"
#include <rsutils/easylogging/easyloggingpp.h>


namespace rs2
{
    namespace http
    {

#ifndef BUILD_WITH_LIBCURL

        // Dummy - built without libcurl.
        http_uploader::http_uploader() : _curl( nullptr ) {}
        http_uploader::~http_uploader() {}
        bool http_uploader::upload( const std::string &, const std::string & ) { return false; }

#else

        // Discard the response body: report all bytes consumed so curl doesn't treat it as a
        // write error (any other return value aborts the transfer).
        static size_t discard_response( void *, size_t size, size_t nmemb, void * ) { return size * nmemb; }

        http_uploader::http_uploader() : _curl( curl_easy_init() ) {}

        http_uploader::~http_uploader()
        {
            if( _curl )
                curl_easy_cleanup( static_cast< CURL * >( _curl ) );
        }

        bool http_uploader::upload( const std::string & url, const std::string & json_body )
        {
            if( ! _curl )
                return false;
            CURL * curl = static_cast< CURL * >( _curl );

            curl_slist * headers = curl_slist_append( nullptr, "Content-Type: application/json" );

            curl_easy_setopt( curl, CURLOPT_URL, url.c_str() );
            curl_easy_setopt( curl, CURLOPT_POST, 1L );
            curl_easy_setopt( curl, CURLOPT_POSTFIELDS, json_body.c_str() );
            curl_easy_setopt( curl, CURLOPT_POSTFIELDSIZE, (long)json_body.size() );
            curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers );
            curl_easy_setopt( curl, CURLOPT_CONNECTTIMEOUT, 5L );
            curl_easy_setopt( curl, CURLOPT_TIMEOUT, 15L );  // overall cap so a stalled transfer can't hang the thread
            curl_easy_setopt( curl, CURLOPT_NOSIGNAL, 1L );
            curl_easy_setopt( curl, CURLOPT_FAILONERROR, 1L );
            curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, discard_response );

            auto res = curl_easy_perform( curl );
            bool ok = ( res == CURLE_OK );
            if( ! ok )
                LOG_ERROR( "HTTP upload to " << url << " failed: " << curl_easy_strerror( res ) );

            curl_slist_free_all( headers );
            return ok;
        }

#endif  // BUILD_WITH_LIBCURL

    }
}
