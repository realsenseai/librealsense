// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <stdio.h>
#include <memory>
#include <functional>
#include <thread>
#include <string.h>
#include <chrono>
#include <common/cli.h>


int main(int argc, char * argv[]) try
{
    rs2::rum::set_app_name( "rs-record" );
    // Parse command line arguments
    using rs2::cli;
    cli cmd( "librealsense rs-record example tool" );
    cli::value< double > time( 't', "Time", "seconds", 10., "Amount of time to record (in seconds)" );
    cli::value<std::string> out_file('f', "FullFilePath", "path", "", "the file where the data will be saved to", cli::required);

    cmd.add(time);
    cmd.add(out_file);
    auto settings = cmd.process(argc, argv);

    rs2::pipeline pipe( settings.dump() );
    rs2::config cfg;
    cfg.enable_record_to_file(out_file.getValue());

    std::mutex m;
    auto callback = [&](const rs2::frame& frame)
    {
        std::lock_guard<std::mutex> lock(m);
        auto t = std::chrono::system_clock::now();
        static auto tk = t;
        static auto t0 = t;
        if (t - tk >= std::chrono::seconds(1)) {
            std::cout << "\r" << std::setprecision(3) << std::fixed
                      << "Recording t = "  << std::chrono::duration_cast<std::chrono::seconds>(t-t0).count() << "s" << std::flush;
            tk = t;
        }
    };

    rs2::pipeline_profile profiles = pipe.start(cfg, callback);

    auto t = std::chrono::system_clock::now();
    auto t0 = t;
    while( t - t0 <= std::chrono::milliseconds( (unsigned)( time.getValue() * 1000 ) ) )
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        t = std::chrono::system_clock::now();
    }
    std::cout << "\nFinished" << std::endl;

    pipe.stop();

    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
