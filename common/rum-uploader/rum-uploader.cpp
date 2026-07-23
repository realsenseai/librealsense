// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rum-uploader.h"

#include <librealsense2/rs.hpp>  // rs2::rum::is_cloud_enabled
#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/os/special-folder.h>
#include <fstream>
#include <sstream>
#include <thread>

#ifdef ENABLE_STATS
#include <curl/curl.h>
#endif


namespace rs2 {


// No production endpoint yet; upload to the local dev-server stub (see dev-server/) for now.
// TODO: use the real cloud endpoint once it exists.
static char const * RUM_ENDPOINT = "http://127.0.0.1:8080/v1/rum";


std::string rum_uploader::saved_report()
{
    auto path = rsutils::os::get_special_folder( rsutils::os::special_folder::app_data ) + "rum/rum.json";
    // Read the file exactly as saved (binary = no newline translation).
    std::ifstream f( path, std::ios::binary );
    if( ! f )
        return std::string();
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}


#ifdef ENABLE_STATS

namespace {

size_t discard_response( void *, size_t size, size_t nmemb, void * )
{
    return size * nmemb;  // ignore the server's response body
}

}  // namespace


bool rum_uploader::upload( std::string const & json_report )
{
    // Refuse to send without consent, so no caller can leak data by forgetting to check.
    if( ! rs2::rum::is_cloud_enabled() )
    {
        LOG_WARNING( "RUM upload refused: cloud upload not consented" );
        return false;
    }

    CURL * curl = curl_easy_init();
    if( ! curl )
        return false;

    curl_slist * headers = curl_slist_append( nullptr, "Content-Type: application/json" );

    curl_easy_setopt( curl, CURLOPT_URL, RUM_ENDPOINT );
    curl_easy_setopt( curl, CURLOPT_POST, 1L );
    curl_easy_setopt( curl, CURLOPT_POSTFIELDS, json_report.c_str() );
    curl_easy_setopt( curl, CURLOPT_POSTFIELDSIZE, (long)json_report.size() );
    curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers );
    curl_easy_setopt( curl, CURLOPT_CONNECTTIMEOUT, 5L );
    curl_easy_setopt( curl, CURLOPT_TIMEOUT, 15L );  // overall cap so a stalled transfer can't hang the thread
    curl_easy_setopt( curl, CURLOPT_NOSIGNAL, 1L );
    curl_easy_setopt( curl, CURLOPT_FAILONERROR, 1L );
    curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, discard_response );

    auto res = curl_easy_perform( curl );
    bool ok = ( res == CURLE_OK );
    if( ! ok )
        LOG_ERROR( "RUM upload to " << RUM_ENDPOINT << " failed: " << curl_easy_strerror( res ) );

    curl_slist_free_all( headers );
    curl_easy_cleanup( curl );
    return ok;
}

#else  // ENABLE_STATS

bool rum_uploader::upload( std::string const & )
{
    LOG_WARNING( "RUM upload unavailable: built without HTTP support (ENABLE_STATS not defined)" );
    return false;
}

#endif  // ENABLE_STATS


void rum_uploader::start()
{
    _thread = std::thread( []()
    {
        try
        {
            if( ! rs2::rum::is_cloud_enabled() )
                return;
            auto report = saved_report();   // prior session (nothing live yet at boot)
            if( report.empty() )
                return;  // nothing saved yet
            if( upload( report ) )
                LOG_INFO( "RUM report uploaded to " << RUM_ENDPOINT );
        }
        catch( std::exception const & e ) { LOG_ERROR( "RUM upload error: " << e.what() ); }
    } );
}


void rum_uploader::upload_async( std::string report, std::function< void( bool ) > on_done )
{
    if( _uploading.exchange( true ) )
        return;  // an upload is already running; don't block the caller
    if( _thread.joinable() )
        _thread.join();  // previous upload finished; join is instant
    _thread = std::thread( [this, report = std::move( report ), on_done = std::move( on_done )]()
    {
        bool ok = false;
        try
        {
            ok = upload( report );
            if( ok )
                LOG_INFO( "RUM report uploaded to " << RUM_ENDPOINT );
            else
                LOG_ERROR( "RUM upload failed" );
        }
        catch( std::exception const & e ) { LOG_ERROR( "RUM upload error: " << e.what() ); }
        if( on_done )
            on_done( ok );
        _uploading = false;
    } );
}


rum_uploader::~rum_uploader()
{
    if( _thread.joinable() )
        _thread.join();
}


}  // namespace rs2
