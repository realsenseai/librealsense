// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "http-downloader.h"
#include <rsutils/easylogging/easyloggingpp.h>
#include <fstream>


namespace rs2
{
    namespace http
    {

        // Adapt the caller's callback_result progress callback to curl_wrapper's bool convention
        // (true = keep going). Empty in -> empty out (no progress meter).
        static curl_wrapper::progress_func adapt_progress( const user_callback_func_type & user_callback_func )
        {
            if( ! user_callback_func )
                return curl_wrapper::progress_func();
            return [user_callback_func]( uint64_t now, uint64_t total )
            {
                return user_callback_func( now, total ) == callback_result::CONTINUE_DOWNLOAD;
            };
        }

        bool http_downloader::download_to_stream( const std::string & url, std::stringstream & output, user_callback_func_type user_callback_func )
        {
            auto sink = [&output]( const char * data, size_t len ) { output.write( data, len ); return true; };
            // SSL verification disabled here to preserve this path's original behavior.
            return _curl.get( url, sink, adapt_progress( user_callback_func ), true /*insecure*/ );
        }

        bool http_downloader::download_to_bytes_vector( const std::string & url, std::vector<uint8_t> & output, user_callback_func_type user_callback_func )
        {
            auto sink = [&output]( const char * data, size_t len )
            {
                output.insert( output.end(), data, data + len );
                return true;
            };
            return _curl.get( url, sink, adapt_progress( user_callback_func ) );
        }

        bool http_downloader::download_to_file( const std::string & url, const std::string & file_name, user_callback_func_type user_callback_func )
        {
            std::ofstream out_file( file_name, std::ios::out | std::ios::binary );
            if( ! out_file.good() )
            {
                LOG_ERROR( "Download error - Cannot open local file: " + file_name );
                return false;
            }
            auto sink = [&out_file]( const char * data, size_t len ) { out_file.write( data, len ); return true; };
            return _curl.get( url, sink, adapt_progress( user_callback_func ) );
        }
    }
}
