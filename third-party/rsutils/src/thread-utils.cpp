// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include <rsutils/concurrency/thread-utils.h>

#include <atomic>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#elif defined( __APPLE__ )
#include <pthread.h>
#else
#include <pthread.h>
#endif


namespace rsutils {
namespace concurrency {


// Global callback storage, protected by a mutex for thread-safe registration
static std::mutex s_callback_mutex;
static thread_start_callback_fn s_thread_start_callback;


void set_thread_start_callback( thread_start_callback_fn callback )
{
    std::lock_guard< std::mutex > lock( s_callback_mutex );
    s_thread_start_callback = std::move( callback );
}


static thread_start_callback_fn get_thread_start_callback()
{
    std::lock_guard< std::mutex > lock( s_callback_mutex );
    return s_thread_start_callback;
}


void set_current_thread_name( std::string const & name )
{
    if( name.empty() )
        return;

#ifdef _WIN32
    // SetThreadDescription is available on Windows 10 1607+
    // Convert narrow string to wide string
    int len = static_cast< int >( name.size() );
    int wlen = MultiByteToWideChar( CP_UTF8, 0, name.c_str(), len, nullptr, 0 );
    if( wlen > 0 )
    {
        std::wstring wname( wlen, L'\0' );
        MultiByteToWideChar( CP_UTF8, 0, name.c_str(), len, &wname[0], wlen );
        SetThreadDescription( GetCurrentThread(), wname.c_str() );
    }
#elif defined( __APPLE__ )
    // macOS: pthread_setname_np takes only the name (applies to current thread)
    // No documented length limit, but 64 chars is typical
    pthread_setname_np( name.c_str() );
#else
    // Linux: pthread_setname_np has a 16-byte limit (15 chars + null terminator)
    char truncated[16];
    std::strncpy( truncated, name.c_str(), 15 );
    truncated[15] = '\0';
    pthread_setname_np( pthread_self(), truncated );
#endif
}


std::thread create_thread( thread_category category, std::string const & name, std::function< void() > fn )
{
    // Build the prefixed name outside the thread to avoid races on 'name' reference
    std::string prefixed_name = "rs:" + ( name.empty() ? std::string( "unnamed" ) : name );

    return std::thread(
        [category, thread_name = std::move( prefixed_name ), thread_fn = std::move( fn )]()
        {
            set_current_thread_name( thread_name );

            auto cb = get_thread_start_callback();
            if( cb )
                cb( category, thread_name );

            thread_fn();
        } );
}


}  // namespace concurrency
}  // namespace rsutils
