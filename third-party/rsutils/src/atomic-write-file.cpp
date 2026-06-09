// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include <rsutils/os/atomic-write-file.h>

#include <fstream>
#include <cstdio>
#include <string>
#include <atomic>
#include <thread>
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


bool atomic_write_file( const std::string & filename, const std::string & content )
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
    bool ok = MoveFileExA( temp_filename.c_str(), filename.c_str(),
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) != 0;
#else
    bool ok = std::rename( temp_filename.c_str(), filename.c_str() ) == 0;
#endif

    if( ! ok )
        std::remove( temp_filename.c_str() );

    return ok;
}


}  // namespace os
}  // namespace rsutils
