// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rum-uploader.h"

#include <librealsense2/rs.hpp>  // rs2::rum::is_cloud_enabled
#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/os/special-folder.h>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

#ifdef CHECK_FOR_UPDATES
#include <curl/curl.h>
#include <mutex>
#endif


namespace rs2 {
namespace rum_uploader {


static char const * RUM_CLOUD_ENDPOINT = "https://telemetry.realsenseai.com/v1/rum";


std::string endpoint()
{
    char const * override_url = std::getenv( "RS2_RUM_ENDPOINT" );
    if( override_url && *override_url )
        return override_url;
    return RUM_CLOUD_ENDPOINT;
}


std::string saved_report()
{
    auto path = rsutils::os::get_special_folder( rsutils::os::special_folder::app_data ) + "rum/rum.json";
    std::ifstream f( path, std::ios::binary );
    if( ! f )
        return std::string();
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}


#ifdef CHECK_FOR_UPDATES

namespace {

std::mutex curl_init_mutex;  // curl_easy_init() is not thread-safe

size_t discard_response( void *, size_t size, size_t nmemb, void * )
{
    return size * nmemb;  // ignore the server's response body
}

}  // namespace


bool upload( std::string const & json_report, std::string const & endpoint )
{
    // Fail-safe: the network primitive itself refuses to send without consent, so no
    // caller can ever leak data off the machine by forgetting to gate.
    if( ! rs2::rum::is_cloud_enabled() )
    {
        LOG_WARNING( "RUM upload refused: cloud upload not consented" );
        return false;
    }

    CURL * curl = nullptr;
    {
        std::lock_guard< std::mutex > lock( curl_init_mutex );
        curl = curl_easy_init();
    }
    if( ! curl )
        return false;

    curl_slist * headers = curl_slist_append( nullptr, "Content-Type: application/json" );

    curl_easy_setopt( curl, CURLOPT_URL, endpoint.c_str() );
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
        LOG_ERROR( "RUM upload to " << endpoint << " failed: " << curl_easy_strerror( res ) );

    curl_slist_free_all( headers );
    {
        std::lock_guard< std::mutex > lock( curl_init_mutex );
        curl_easy_cleanup( curl );
    }
    return ok;
}

#else  // CHECK_FOR_UPDATES

bool upload( std::string const &, std::string const & )
{
    LOG_WARNING( "RUM upload unavailable: built without HTTP support (CHECK_FOR_UPDATES=OFF)" );
    return false;
}

#endif  // CHECK_FOR_UPDATES


namespace {

std::thread saved_upload_thread;   // joined via join_saved_upload()

long long now_seconds()
{
    return std::chrono::duration_cast< std::chrono::seconds >(
        std::chrono::system_clock::now().time_since_epoch() ).count();
}

}  // namespace


void start_saved_upload( int cadence_hours, long long last_upload_unix,
                         std::function< void( long long ) > on_uploaded )
{
    saved_upload_thread = std::thread( [cadence_hours, last_upload_unix, on_uploaded = std::move( on_uploaded )]()
    {
        try
        {
            if( ! rs2::rum::is_cloud_enabled() )
                return;
            if( now_seconds() - last_upload_unix < (long long)cadence_hours * 3600 )
                return;  // within the cadence window
            auto report = saved_report();   // prior, completed session (the live aggregate is empty at boot)
            if( report.empty() )
                return;  // nothing saved yet
            if( upload( report, endpoint() ) )
            {
                if( on_uploaded )
                    on_uploaded( now_seconds() );
                LOG_INFO( "RUM report uploaded to " << endpoint() );
            }
        }
        catch( std::exception const & e ) { LOG_ERROR( "RUM upload error: " << e.what() ); }
    } );
}


void join_saved_upload()
{
    if( saved_upload_thread.joinable() )
        saved_upload_thread.join();
}


}  // namespace rum_uploader
}  // namespace rs2
