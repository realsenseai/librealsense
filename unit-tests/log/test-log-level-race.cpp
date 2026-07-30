// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "log-common.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <cstdio>

// Regression test for RSDSO-21284 (github.com/realsenseai/librealsense/issues/14761):
// changing the log level from one thread while another thread logs used to segfault.
// The LIBRS_LOG_/LIBRS_LOG_STR_ macros checked logger->enabled() without holding the
// logger's lock, racing against Logger::configure() (triggered by rs2::log_to_console())
// deleting and replacing the logger's TypedConfigurations from another thread.
TEST_CASE( "changing log level while logging from another thread", "[log]" )
{
    std::atomic<bool> stop( false );

    // Mimics an application repeatedly changing the console log level -- e.g. to suppress
    // warnings while setting up a second camera -- while a first camera is streaming.
    std::thread level_changer( [&]() {
        while( ! stop )
        {
            rs2::log_to_console( RS2_LOG_SEVERITY_FATAL );
            rs2::log_to_console( RS2_LOG_SEVERITY_WARN );
        }
    } );

    // Mimics the streaming thread logging frame-callback info (e.g. log_callback_end()). Level changes
    // aren't atomic from this thread's point of view, so some DEBUG lines may still leak to the console.
    std::vector< std::thread > loggers;
    for( int i = 0; i < 4; ++i )
        loggers.emplace_back( [&]() {
            while( ! stop )
                rs2::log( RS2_LOG_SEVERITY_DEBUG, "callback finished" );
        } );

    std::this_thread::sleep_for( std::chrono::milliseconds( 300 ) );
    stop = true;

    REQUIRE_NOTHROW( level_changer.join() );
    for( auto & t : loggers )
        REQUIRE_NOTHROW( t.join() );
}


// Same race, but through rs2::log_to_file(): Logger::configure() mutates m_unflushedCount before taking its
// lock, and isFlushNeeded() (only reached when logging to a file) reads it under lock on another thread.
TEST_CASE( "changing log file level while logging from another thread", "[log]" )
{
    const char * log_file_path = ".//log-level-race-file.log";
    std::remove( log_file_path );

    std::atomic<bool> stop( false );

    std::thread level_changer( [&]() {
        while( ! stop )
        {
            rs2::log_to_file( RS2_LOG_SEVERITY_DEBUG, log_file_path );
            rs2::log_to_file( RS2_LOG_SEVERITY_ERROR, log_file_path );
        }
    } );

    std::vector< std::thread > loggers;
    for( int i = 0; i < 4; ++i )
        loggers.emplace_back( [&]() {
            while( ! stop )
                rs2::log( RS2_LOG_SEVERITY_DEBUG, "callback finished" );
        } );

    std::this_thread::sleep_for( std::chrono::milliseconds( 300 ) );
    stop = true;

    REQUIRE_NOTHROW( level_changer.join() );
    for( auto & t : loggers )
        REQUIRE_NOTHROW( t.join() );

    std::remove( log_file_path );  // best-effort: ELPP may still hold the file open on some platforms
}
