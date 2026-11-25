// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp> // RealSense Cross Platform API
#include <iostream>

#include <common/cli.h>

// Hello RealSense example demonstrates the basics of connecting to a RealSense device
// and taking advantage of depth data
int main(int argc, char * argv[]) try
{
    rs2::rum::set_app_name( "rs-on-chip-calib" );
    auto settings = rs2::cli( "on-chip-calib example" ).process( argc, argv );

    // Create a pipeline - this serves as a top-level API for streaming and processing frames
    rs2::pipeline pipe( settings.dump() );

    // Configure and start the pipeline
    rs2::config cfg;
    cfg.enable_stream( RS2_STREAM_DEPTH, 256, 144, RS2_FORMAT_Z16, 90 );
    rs2::pipeline_profile prof = pipe.start( cfg );
    pipe.wait_for_frames(); // Wait for streaming to start, will throw on timeout
    
    // Control calibration parameters using a JSON input
    //   Currently for OCC only type 0 is used.
    //   Speed can be 0 - very fast, 1 - fast, 2 - medium, 3 - slow (default). Timeout should be adjusted accordingly.
    //   Scan parameter 0 - intrinsic (default), 1 - extrinsic.
    std::stringstream ss;
    ss << "{"
       << "\n \"calib type\":" << 0
       << ",\n \"speed\":" << 2
       << ",\n \"scan parameter\":" << 0
       << "\n}";
    std::string json = ss.str();
    std::cout << "Starting OCC with configuration:\n" << json << std::endl;

    // Get device with calibration API
    rs2::device dev = prof.get_device();
    rs2::auto_calibrated_device cal_dev = dev.as< rs2::auto_calibrated_device >();

    // Run calibration
    float health;
    int timeout_ms = 9000;
    rs2::calibration_table res = cal_dev.run_on_chip_calibration( json, &health, [&]( const float progress ) {
        std::cout << "progress = " << progress << "%" << std::endl;
    }, timeout_ms );

    std::cout << "Completed successfully" << std::endl;

    // Device is currently using calibration results, but they are not saved and will be lost after HW reset/power cycling.
    std::cout << std::endl << "Keep results? Yes/No" << std::endl;
    std::string keep;
    std::cin >> keep;
    for( auto & c : keep )
        c = tolower( c );
    if( keep == "y" || keep == "yes" )
    {
        cal_dev.set_calibration_table( res );
        cal_dev.write_calibration();
        std::cout << "Results saved to flash" << std::endl;
    }
    else
    {
        // To return using previous calibration parameters you can `get_calibration_table` before calibrating and save
        // the old table back. Or just reset the camera to avoid flash writes
        std::cout << "Results not saved" << std::endl;
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
