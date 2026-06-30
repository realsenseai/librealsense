// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>

#include <iostream>
#include <string>
#include <thread>


rs2::device get_dds_device()
{
    // Create RealSense context
    rs2::context ctx;

    // Find devices
    auto devices = ctx.query_devices();
    if (devices.size() == 0)
    {
        std::cerr << "No RealSense devices found!" << std::endl;
        throw std::runtime_error("No RealSense devices found!");
    }

    // Find first DDS device
    rs2::device dev;
    for (auto&& d : devices)
    {
        if (strcmp(d.get_info(RS2_CAMERA_INFO_CONNECTION_TYPE), "DDS") == 0)
        {
            dev = d;
            break;
        }
    }
    return dev;
}

rs2::stream_profile get_depth_profile(rs2::depth_sensor depth_sensor, int nominal_width, int nominal_height)
{
    auto depth_profiles = depth_sensor.get_stream_profiles();
    rs2::stream_profile depth_profile;
    for (auto& p : depth_profiles)
    {
        if (p.format() == RS2_FORMAT_Z16 && p.fps() == 30)
        {
            auto vsp = p.as<rs2::video_stream_profile>();
            if (vsp.height() == nominal_height && vsp.width() == nominal_width)
            {
                depth_profile = p;
                break;
            }
        }
    }
    return depth_profile;
}


// scenario:
// get dds device, depth sensor
// query embedded filters
// get filters' options
// set filters' options to other params
// get filters' options to verify the change
// setting back to initial values
int main( int argc, char * argv[] )
try
{
    std::cout << "RealSense Embedded Filters Example" << std::endl;
    std::cout << "=========================================" << std::endl;

    // getting device
    auto dev = get_dds_device();
    if (!dev)
    {
        std::cerr << "No RealSense DDS devices found!" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Using device: " << dev.get_info(RS2_CAMERA_INFO_NAME) << std::endl;
    
    // getting depth sensor
    auto depth_sensor = dev.first<rs2::depth_sensor>();
    if (!depth_sensor)
    {
        std::cerr << "Device has no depth sensor!" << std::endl;
        return EXIT_FAILURE;
    }

    // setting HD resolution profile
    auto nominal_width = 1280;
    auto nominal_height = 720;
    auto depth_profile = get_depth_profile(depth_sensor, nominal_width, nominal_height);

    if (!depth_profile)
    {
        std::cerr << "No suitable depth profile found!" << std::endl;
        return EXIT_FAILURE;
    }


    auto embedded_filters = depth_sensor.query_embedded_filters();
    for(auto& filter : embedded_filters)
    {
        std::cout << "Embedded filter supported: " << rs2_embedded_filter_type_to_string(filter.get_type()) << std::endl;
    }

    std::cout << std::endl;
    std::cout << "Decimation Filter" << std::endl;
    std::cout << "=========================================" << std::endl;

    rs2::embedded_decimation_filter dec_filter = depth_sensor.get_embedded_filter< rs2::embedded_decimation_filter>();

    auto dec_filter_options = dec_filter.get_supported_options();
    for (auto& option : dec_filter_options)
    {
        std::cout << "Decimation filter option supported: " << dec_filter.get_option_name(option) << std::endl;
    }

    // getting initial values
    std::cout << "Initial values:" << std::endl;
    auto enabled = dec_filter.get_option(RS2_OPTION_EMBEDDED_FILTER_ENABLED);
    auto magnitude = dec_filter.get_option(RS2_OPTION_FILTER_MAGNITUDE);
    std::cout << "Decimation filter enabled: " << enabled << std::endl;
    std::cout << "Decimation filter magnitude: " << magnitude << std::endl;
    std::cout << std::endl;

    std::cout << "Setting toggle ON" << std::endl;
    dec_filter.set_option(RS2_OPTION_EMBEDDED_FILTER_ENABLED, 1);
    std::cout << "Decimation filter enabled: " << dec_filter.get_option(RS2_OPTION_EMBEDDED_FILTER_ENABLED) << std::endl;

    // below line won't run because option is read-only
    try {
        dec_filter.set_option(RS2_OPTION_FILTER_MAGNITUDE, 2.f);
    }
    catch (...)
    {
        // expected - option is read-only
    }

    std::cout << "Setting toggle back to initial value: " << enabled << std::endl;
    dec_filter.set_option(RS2_OPTION_EMBEDDED_FILTER_ENABLED, enabled);

    return EXIT_SUCCESS;
}
catch( const rs2::error & e )
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    "
              << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch( const std::exception & e )
{
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
}
