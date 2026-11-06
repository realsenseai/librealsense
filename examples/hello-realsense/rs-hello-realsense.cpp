// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019-24 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp> // RealSense Cross Platform API
#include <iostream>

#include <common/cli.h>
#include "../example-utils.hpp"

// Hello RealSense example demonstrates the basics of connecting to a RealSense device
// and taking advantage of depth data
int main(int argc, char * argv[]) try
{
    auto settings = rs2::cli( "hello-realsense example" )
        .process( argc, argv );

    rs2::context ctx( settings.dump() );
    wait_for_devices( ctx );

    // Create a Pipeline - this serves as a top-level API for streaming and processing frames
    rs2::pipeline p( ctx );

    // Configure and start the pipeline
    p.start();

    while (true)
    {
        // Block program until frames arrive
        rs2::frameset frames = p.wait_for_frames();

        // Try to get a frame of a depth image
        rs2::depth_frame depth = frames.get_depth_frame();

        // Get the depth frame's dimensions
        auto width = depth.get_width();
        auto height = depth.get_height();

        // Query the distance from the camera to the object in the center of the image
        float dist_to_center = depth.get_distance(width / 2, height / 2);

        // Print the distance
        std::cout << "The camera is facing an object " << dist_to_center << " meters away \r";
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
