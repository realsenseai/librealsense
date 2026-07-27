// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "log-common.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

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

    // Mimics the streaming thread logging frame-callback info (e.g. log_callback_end()).
    // Severity is kept below WARN so nothing actually gets printed to the console.
    std::vector< std::thread > loggers;
    for( int i = 0; i < 4; ++i )
        loggers.emplace_back( [&]() {
            while( ! stop )
                rs2::log( RS2_LOG_SEVERITY_DEBUG, "callback finished" );
        } );

    std::this_thread::sleep_for( std::chrono::seconds( 2 ) );
    stop = true;

    level_changer.join();
    for( auto & t : loggers )
        t.join();
}
