// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#pragma once
#include <iostream>
#include <string>
#include <map>
#include <librealsense2/rs.hpp>
#include <algorithm>
#include <chrono>
#include <thread>

//////////////////////////////
// Demos Helpers            //
//////////////////////////////

// USB devices are normally visible immediately, while Ethernet/DDS discovery is
// asynchronous. Keep the wait bounded so examples still fail predictably when no
// camera is connected.
inline rs2::device_list wait_for_devices( rs2::context & ctx, int timeout_seconds = 30 )
{
    constexpr int device_mask = RS2_PRODUCT_LINE_ANY | RS2_PRODUCT_LINE_SW_ONLY;
    for( int waited = 0; waited < timeout_seconds; ++waited )
    {
        auto devices = ctx.query_devices( device_mask );
        if( devices.size() )
            return devices;

        if( waited == 0 )
            std::cout << "Waiting for RealSense device (USB instant; Ethernet/DDS discovery up to "
                      << timeout_seconds << "s)..." << std::endl;
        std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
    }

    auto devices = ctx.query_devices( device_mask );
    if( ! devices.size() )
        throw std::runtime_error( "No RealSense device found before discovery timeout" );
    return devices;
}

// Find devices with specified streams
bool device_with_streams( rs2::context & ctx, std::vector< rs2_stream > stream_requests, std::string & out_serial )
{
    auto devs = wait_for_devices( ctx );
    std::vector <rs2_stream> unavailable_streams = stream_requests;
    for (auto dev : devs)
    {
        std::map<rs2_stream, bool> found_streams;
        for (auto& type : stream_requests)
        {
            found_streams[type] = false;
            for (auto& sensor : dev.query_sensors())
            {
                for (auto& profile : sensor.get_stream_profiles())
                {
                    if (profile.stream_type() == type)
                    {
                        found_streams[type] = true;
                        unavailable_streams.erase(std::remove(unavailable_streams.begin(), unavailable_streams.end(), type), unavailable_streams.end());
                        if (dev.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
                            out_serial = dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
                    }
                }
            }
        }
        // Check if all streams are found in current device
        bool found_all_streams = true;
        for (auto& stream : found_streams)
        {
            if (!stream.second)
            {
                found_all_streams = false;
                break;
            }
        }
        if (found_all_streams)
            return true;
    }
    // After scanning all devices, not all requested streams were found
    for (auto& type : unavailable_streams)
    {
        switch (type)
        {
        case RS2_STREAM_POSE:
        case RS2_STREAM_FISHEYE:
            std::cerr << "Connect T26X and rerun the demo" << std::endl;
            break;
        case RS2_STREAM_DEPTH:
            std::cerr << "The demo requires Realsense camera with DEPTH sensor" << std::endl;
            break;
        case RS2_STREAM_COLOR:
            std::cerr << "The demo requires Realsense camera with RGB sensor" << std::endl;
            break;
        default:
            throw std::runtime_error("The requested stream: " + std::to_string(type) + ", for the demo is not supported by connected devices!"); // stream type
        }
    }
    return false;
}
