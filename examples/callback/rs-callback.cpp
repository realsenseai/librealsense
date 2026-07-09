// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017-24 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API

#include <common/cli.h>

#include <iostream>
#include <map>
#include <chrono>
#include <mutex>
#include <thread>

// The callback example demonstrates asynchronous usage of the pipeline
int main(int argc, char * argv[]) try
{
    auto settings = rs2::cli( "rs-callback example" )
        .process( argc, argv );

    std::map<int, int> counters;
    std::map<int, std::string> stream_names;
    std::mutex mutex;

    // Define frame callback
    // The callback is executed on a sensor thread and can be called simultaneously from multiple sensors
    // Therefore any modification to common memory should be done under lock
    auto callback = [&](const rs2::frame& frame)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (rs2::frameset fs = frame.as<rs2::frameset>())
        {
            // With callbacks, all synchronized stream will arrive in a single frameset
            for (const rs2::frame& f : fs)
                counters[f.get_profile().unique_id()]++;
        }
        else
        {
            // Stream that bypass synchronization (such as IMU) will produce single frames
            counters[frame.get_profile().unique_id()]++;
        }
    };

    // Shared context so DDS wait and pipeline use the same discovery session
    rs2::context ctx( settings.dump() );
    rs2::device_hub( ctx ).wait_for_device(); // USB: instant; Ethernet/DDS: async discovery

    // Declare RealSense pipeline, encapsulating the actual device and sensors.
    rs2::pipeline pipe( ctx );

    // Start streaming through the callback with default recommended configuration
    // The default video configuration contains Depth and Color streams
    // If a device is capable to stream IMU data, both Gyro and Accelerometer are enabled by default
    //
    rs2::pipeline_profile profiles = pipe.start(callback);

    // Collect the enabled streams names
    for (auto p : profiles.get_streams())
        stream_names[p.unique_id()] = p.stream_name();

    std::cout << "RealSense callback sample" << std::endl << std::endl;

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::lock_guard<std::mutex> lock(mutex);

        std::cout << "\r";
        for (auto p : counters)
        {
            std::cout << stream_names[p.first] << "[" << p.first << "]: " << p.second << " [frames] || ";
        }
    }

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
