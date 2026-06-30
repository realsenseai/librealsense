// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:add-file ../../common/rs-config.cpp

#include <unit-tests/catch.h>
#include <common/rs-config.h>
#include <rsutils/os/special-folder.h>

#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif


namespace {

static std::string make_test_config_path()
{
#ifdef _WIN32
    std::string pid = std::to_string( GetCurrentProcessId() );
#else
    std::string pid = std::to_string( getpid() );
#endif
    auto temp_dir = rsutils::os::get_special_folder( rsutils::os::special_folder::temp_folder );
    return temp_dir + "rs_config_test_" + pid + ".json";
}

// RAII wrapper: removes the file at path on destruction
struct scoped_file
{
    std::string path;

    explicit scoped_file( std::string p )
        : path( std::move( p ) )
    {
    }

    ~scoped_file() { std::remove( path.c_str() ); }
};

}  // namespace


// Verify that a value written via set() is persisted to disk and correctly
// loaded by a fresh config_file instance. With deferred saves, the write
// happens in the destructor's final flush.
TEST_CASE( "config/regular_update", "[common]" )
{
    std::string path = make_test_config_path();
    scoped_file guard( path );

    {
        rs2::config_file cfg( path );
        cfg.set( "greeting", "hello_world" );
        // Destructor signals the background thread to stop, joins it, then
        // flushes the dirty flag to disk via atomic_write_file.
    }

    rs2::config_file reloaded( path );
    CHECK( reloaded.contains( "greeting" ) );
    CHECK( reloaded.get( "greeting", "" ) == std::string( "hello_world" ) );
}


// Verify the background thread saves dirty data on its own — without relying
// on the destructor's final flush. After waiting longer than SAVE_INTERVAL
// (1000 ms) the file must exist while the config_file is still alive.
TEST_CASE( "config/background_save", "[common]" )
{
    std::string path = make_test_config_path();
    scoped_file guard( path );

    rs2::config_file cfg( path );
    cfg.set( "bg_key", "bg_value" );

    // Wait 1500 ms — longer than SAVE_INTERVAL (1000 ms).
    std::this_thread::sleep_for( std::chrono::milliseconds( 1500 ) );

    // cfg is still alive here: if the value is on disk the background thread
    // wrote it, not the destructor.
    rs2::config_file mid_check( path );
    CHECK( mid_check.get( "bg_key", "" ) == std::string( "bg_value" ) );
}


// Rapid successive set() calls must all be coalesced into a single write with
// the final value intact — no data loss, no torn write.
TEST_CASE( "config/coalescing", "[common]" )
{
    std::string path = make_test_config_path();
    scoped_file guard( path );

    {
        rs2::config_file cfg( path );
        for( int i = 0; i < 50; ++i )
            cfg.set( "counter", std::to_string( i ).c_str() );
    }

    rs2::config_file reloaded( path );
    CHECK( reloaded.get( "counter", "" ) == std::string( "49" ) );
}


// A config_file with no mutations must not create the backing file.
TEST_CASE( "config/no_spurious_write", "[common]" )
{
    std::string path = make_test_config_path();
    scoped_file guard( path );

    {
        rs2::config_file cfg( path );
        // No set() calls — _dirty is never true, nothing written.
    }

    std::ifstream f( path );
    CHECK( ! f.good() );
}


// Multiple threads calling set() concurrently must not corrupt the JSON or crash.
TEST_CASE( "config/concurrent_sets", "[common]" )
{
    std::string path = make_test_config_path();
    scoped_file guard( path );

    {
        rs2::config_file cfg( path );

        std::vector< std::thread > threads;
        for( int i = 0; i < 8; ++i )
        {
            threads.emplace_back( [&cfg, i]()
            {
                for( int j = 0; j < 100; ++j )
                {
                    std::string key = "key_" + std::to_string( i );
                    cfg.set( key.c_str(), std::to_string( j ).c_str() );
                }
            } );
        }
        for( std::thread & t : threads )
            t.join();
    }

    // No crash is the primary success criterion; also verify all keys were persisted.
    rs2::config_file reloaded( path );
    for( int i = 0; i < 8; ++i )
    {
        std::string key = "key_" + std::to_string( i );
        CHECK( reloaded.contains( key.c_str() ) );
    }
}
