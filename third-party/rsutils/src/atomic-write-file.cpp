// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include <rsutils/os/atomic-write-file.h>
#include <rsutils/easylogging/easyloggingpp.h>

#include <fstream>
#include <cstdio>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif


namespace rsutils {
namespace os {


static std::string make_temp_filename( const std::string & filename )
{
    static std::atomic< uint64_t > counter{ 0 };
    std::string temp = filename + ".";
#ifdef _WIN32
    temp += std::to_string( GetCurrentProcessId() );
#else
    temp += std::to_string( getpid() );
#endif
    temp += "." + std::to_string( std::hash< std::thread::id >{}( std::this_thread::get_id() ) );
    temp += "." + std::to_string( counter.fetch_add( 1, std::memory_order_relaxed ) );
    temp += ".tmp";
    return temp;
}


bool atomic_write_file( const std::string & filename, const std::string & content, int max_retries, int retry_delay_ms )
{
    const std::string temp_filename = make_temp_filename( filename );

    std::ofstream out( temp_filename.c_str() );
    if( ! out.is_open() )
        return false;

    out.write( content.data(), static_cast< std::streamsize >( content.size() ) );

    if( ! out )
    {
        out.close();
        std::remove( temp_filename.c_str() );
        return false;
    }

    out.close();

    if( ! out )  // catch flush-at-close errors
    {
        std::remove( temp_filename.c_str() );
        return false;
    }

#ifdef _WIN32
    bool ok = false;
    DWORD last_err = 0;
    for( int attempt = 0; attempt < max_retries && !ok; ++attempt )
    {
        ok = MoveFileExA( temp_filename.c_str(), filename.c_str(),
                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) != 0;
        if( !ok )
        {
            last_err = GetLastError();
            if( last_err != ERROR_SHARING_VIOLATION && last_err != ERROR_ACCESS_DENIED )
                break;
            if( attempt + 1 < max_retries )
            {
                LOG_WARNING( "MoveFileExA sharing conflict (err " << last_err << "), retry "
                             << ( attempt + 1 ) << "/" << max_retries << " for '" << filename << "'" );
                std::this_thread::sleep_for( std::chrono::milliseconds( retry_delay_ms ) );
            }
        }
    }
    if( !ok )
        LOG_WARNING( "MoveFileExA failed for '" << filename << "', last error: " << last_err );
#else
    bool ok = std::rename( temp_filename.c_str(), filename.c_str() ) == 0;
#endif

    if( ! ok )
        std::remove( temp_filename.c_str() );

    return ok;
}


}  // namespace os
}  // namespace rsutils
