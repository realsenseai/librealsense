// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include <librealsense2/h/rs_icr_filter_dpp.h>

#include <iostream>
#include <string>
#include <thread>
#include <vector>


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

    // Shared HD resolution used both by Decimation below and by Improved Close Range Depth
    // further down.
    auto nominal_width = 1280;
    auto nominal_height = 720;

    // getting device - Decimation below is DDS-only, unlike Improved Close Range Depth further
    // down (USB-only, does its own independent device search) - so a missing DDS device skips
    // just this section rather than failing the whole program.
    auto dev = get_dds_device();
    if (!dev)
    {
        std::cout << "No RealSense DDS devices found - skipping the Decimation Filter section "
                     "below (DDS-only)." << std::endl;
    }
    else
    {
        std::cout << "Using device: " << dev.get_info(RS2_CAMERA_INFO_NAME) << std::endl;

        // getting depth sensor
        auto depth_sensor = dev.first<rs2::depth_sensor>();
        if (!depth_sensor)
        {
            std::cerr << "Device has no depth sensor!" << std::endl;
            return EXIT_FAILURE;
        }

        // setting HD resolution profile
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
    }

    std::cout << std::endl;
    std::cout << "Improved Close Range Depth (composite option)" << std::endl;
    std::cout << "=========================================" << std::endl;

    // Unlike Decimation above, this filter has no RS2_OPTION_EMBEDDED_FILTER_ENABLED scalar
    // option - its whole configuration is one atomically-exchanged struct. It's D500 USB-only
    // (not DDS), so it's looked up across every connected device, not the DDS device above.
    //
    // rs2::embedded_filter/rs2::depth_sensor have no default constructor - a 0-or-1-element
    // vector stands in for "found or not" instead of a nullable local.
    std::vector<rs2::embedded_filter> found_filter;
    std::vector<rs2::depth_sensor> found_sensor;
    for (auto&& d : rs2::context().query_devices())
    {
        auto ds = d.first<rs2::depth_sensor>();
        if (!ds)
            continue;
        for (auto&& f : ds.query_embedded_filters())
        {
            if (f.supports_composite_option(RS2_COMPOSITE_OPTION_ICR_CONTROL))
            {
                found_filter.push_back(f);
                found_sensor.push_back(ds);
                break;
            }
        }
        if (!found_filter.empty())
            break;
    }

    if (found_filter.empty())
    {
        std::cout << "No connected device exposes Improved Close Range Control - skipping." << std::endl;
    }
    else
    {
        rs2::embedded_filter& close_range_filter = found_filter.front();
        rs2::depth_sensor& close_range_depth_sensor = found_sensor.front();
        const auto id = RS2_COMPOSITE_OPTION_ICR_CONTROL;

        // Typed get - the SDK's own bound struct (get_composite_option_as<T>()), not raw bytes;
        // the same struct set_composite_option_from() below takes to write it back atomically.
        rs2_icr_filter_dpp_config original{};
        bool have_original = false;
        try
        {
            original = close_range_filter.get_composite_option_as<rs2_icr_filter_dpp_config>(id);
            have_original = true;
        }
        catch (const rs2::error& e)
        {
            // Registered but not actually functional on this device/FW is a real, expected
            // outcome - supports_composite_option() only reflects static registration, never a
            // live capability check.
            std::cout << "Registered but not functional on this device/FW: " << e.what() << std::endl;
        }

        if (!have_original)
        {
            // get_composite_option_as() failed above - nothing more to demo.
        }
        else
        {
            auto close_range_profile = get_depth_profile(close_range_depth_sensor, nominal_width, nominal_height);
            if (!close_range_profile)
            {
                std::cout << "No " << nominal_width << "x" << nominal_height
                          << " depth profile available on this device for the demo - skipping." << std::endl;
            }
            else
            {
                // Streams a short burst and returns the minimum non-zero (valid) depth value
                // seen - shows the effect without asserting anything about the actual scene.
                auto capture_min_valid_depth = [&](int frames_to_skip, int frames_to_measure) -> uint16_t
                {
                    rs2::frame_queue queue(1);
                    close_range_depth_sensor.open(close_range_profile);
                    close_range_depth_sensor.start(queue);

                    for (int i = 0; i < frames_to_skip; ++i)
                        queue.wait_for_frame();

                    uint16_t min_depth = 0;
                    for (int i = 0; i < frames_to_measure; ++i)
                    {
                        auto depth = queue.wait_for_frame().as<rs2::depth_frame>();
                        auto data = reinterpret_cast<const uint16_t*>(depth.get_data());
                        size_t pixel_count = (size_t)depth.get_width() * depth.get_height();
                        for (size_t p = 0; p < pixel_count; ++p)
                        {
                            if (data[p] != 0 && (min_depth == 0 || data[p] < min_depth))
                                min_depth = data[p];
                        }
                    }

                    close_range_depth_sensor.stop();
                    close_range_depth_sensor.close();
                    return min_depth;
                };

                std::cout << "Streaming with Improved Close Range Depth OFF..." << std::endl;
                auto cfg = original;
                cfg.enable = 0;
                close_range_filter.set_composite_option_from(id, cfg);
                auto min_depth_off = capture_min_valid_depth(5, 10);
                std::cout << "  Minimum valid depth (filter off): " << min_depth_off << " (depth units)" << std::endl;

                std::cout << "Streaming with Improved Close Range Depth ON (Downscale x2)..." << std::endl;
                cfg = original;
                cfg.enable = 1;
                cfg.filter_type = 0;      // Downscale
                cfg.downscale_ratio = 1;  // x2
                close_range_filter.set_composite_option_from(id, cfg);
                auto min_depth_on = capture_min_valid_depth(5, 10);
                std::cout << "  Minimum valid depth (filter on):  " << min_depth_on << " (depth units)" << std::endl;

                if (min_depth_on > 0 && (min_depth_off == 0 || min_depth_on < min_depth_off))
                    std::cout << "Improved Close Range Depth reported a closer minimum valid depth, as expected." << std::endl;
                else
                    std::cout << "No closer minimum valid depth observed this run - depends on what's actually "
                                 "in front of the camera." << std::endl;

                std::cout << "Restoring original configuration." << std::endl;
                close_range_filter.set_composite_option_from(id, original);
            }
        }
    }

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
