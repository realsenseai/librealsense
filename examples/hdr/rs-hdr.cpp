// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include "example-imgui.hpp"    // Include short list of convenience functions for rendering
#include <iostream>

#include "imgui_impl_glfw.h"
#include <imgui_impl_opengl3.h>
#include<realsense_imgui.h>

// HDR Example demonstrates how to use the HDR feature - only for D400 product line devices
int main() try
{

    rs2::context ctx;
    // Wait before scanning product lines — DDS devices are not instant like USB.
    rs2::device_hub( ctx ).wait_for_device();
    rs2::device_list devices_list = ctx.query_devices();
    size_t device_count = devices_list.size();
    if (!device_count)
    {
        std::cout << "No device detected. Is it plugged in?\n";
        return EXIT_SUCCESS;
    }

    rs2::device device;
    bool device_found = false;
    for (auto&& dev : devices_list)
    {
        // finding a device of D400 product line for working with HDR feature
        if (dev.supports(RS2_CAMERA_INFO_PRODUCT_LINE) &&
            std::string(dev.get_info(RS2_CAMERA_INFO_PRODUCT_LINE)) == "D400")
        {
            device = dev;
            device_found = true;
            break;
        }
    }

    if (!device_found)
    {
        std::cout << "No device from D400 product line detected. Is it plugged in?\n";
        return EXIT_SUCCESS;
    }

    rs2::depth_sensor depth_sensor = device.query_sensors().front();

    // disable auto exposure before sending HDR configuration
    if (depth_sensor.get_option(RS2_OPTION_ENABLE_AUTO_EXPOSURE))
        depth_sensor.set_option(RS2_OPTION_ENABLE_AUTO_EXPOSURE, 0);

    // setting the HDR sequence size to 2 frames
    if (depth_sensor.supports(RS2_OPTION_SEQUENCE_SIZE))
        depth_sensor.set_option(RS2_OPTION_SEQUENCE_SIZE, 2);
    else
    {
        std::cout << "Firmware and/or SDK versions must be updated for the HDR feature to be supported.\n";
        return EXIT_SUCCESS;
    }

    // configuring id for this hdr config (value must be in range [0,3])
    depth_sensor.set_option(RS2_OPTION_SEQUENCE_NAME, 0);

    // configuration for the first HDR sequence ID
    depth_sensor.set_option(RS2_OPTION_SEQUENCE_ID, 1);
    depth_sensor.set_option(RS2_OPTION_EXPOSURE, 8000); // setting exposure to 8000, so sequence 1 will be set to high exposure
    depth_sensor.set_option(RS2_OPTION_GAIN, 25); // setting gain to 25, so sequence 1 will be set to high gain

    // configuration for the second HDR sequence ID
    depth_sensor.set_option(RS2_OPTION_SEQUENCE_ID, 2);
    depth_sensor.set_option(RS2_OPTION_EXPOSURE, 18);  // setting exposure to 18, so sequence 2 will be set to low exposure
    depth_sensor.set_option(RS2_OPTION_GAIN, 16); // setting gain to 16, so sequence 2 will be set to low gain

    // turning ON the HDR with the above configuration
    depth_sensor.set_option(RS2_OPTION_HDR_ENABLED, 1);

    // Declare depth colorizer for pretty visualization of depth data
    rs2::colorizer color_map;

    // Declare RealSense pipeline, encapsulating the actual device and sensors
    rs2::pipeline pipe;

    // Start streaming with depth and infrared configuration
    // The HDR merging algorithm can work with both depth and infrared,or only with depth,
    // but the resulting stream is better when both depth and infrared are used.
    rs2::config cfg;
    cfg.enable_stream(RS2_STREAM_DEPTH);
    cfg.enable_stream(RS2_STREAM_INFRARED, 1);
    pipe.start(cfg);

    // initializing the merging filter
    rs2::hdr_merge merging_filter;

    // initializing the frameset
    rs2::frameset data;

    // flag used to see the original stream or the merged one
    int frames_without_hdr_metadata_params = 0;

    // init parameters to set view's window
    unsigned width = 1280;
    unsigned height = 720;
    std::string title = "RealSense HDR Example";
    unsigned tiles_in_row = 4;
    unsigned tiles_in_col = 2;

    // init view window
    window app(width, height, title.c_str(), tiles_in_row, tiles_in_col);

    // Setup Dear ImGui context
    ImGui::CreateContext();
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(app, false);
    ImGui_ImplOpenGL3_Init();

    // init hdr_widgets object
    // hdr_widgets holds the sliders, the text boxes and the frames_map
    hdr_widgets hdr_widgets(depth_sensor);

    while (app) // application is still alive
    {

        data = pipe.wait_for_frames();    // Wait for next set of frames from the camera

        auto frame = data.get_depth_frame();

        if (!frame.supports_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_SIZE) ||
            !frame.supports_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_ID))
        {
            ++frames_without_hdr_metadata_params;
            if (frames_without_hdr_metadata_params > 20)
            {
                std::cout << "Firmware and/or SDK versions must be updated for the HDR feature to be supported.\n";
                return EXIT_SUCCESS;
            }
            app.show(data.apply_filter(color_map));
            continue;
        }

        // merging the frames from the different HDR sequence IDs
        auto merged_frame = merging_filter.process(data).apply_filter(color_map);   // Find and colorize the depth data;

        //get frames data
        auto hdr_seq_size = frame.get_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_SIZE);
        auto hdr_seq_id = frame.get_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_ID);

        //get frames
        auto infrared_frame = data.get_infrared_frame();
        auto depth_frame = data.get_depth_frame().apply_filter(color_map);
        auto hdr_frame = merged_frame.as<rs2::frameset>().get_depth_frame().apply_filter(color_map);

        //update frames in frames map in hdr_widgets
        hdr_widgets.update_frames_map(infrared_frame, depth_frame, hdr_frame, hdr_seq_id, hdr_seq_size);
        RsImGui::PushNewFrame();
        //render hdr widgets sliders and text boxes
        hdr_widgets.render_widgets();

        //the show method, when applied on frame map, break it to frames and upload each frame into its specific tile
        app.show(hdr_widgets.get_frames_map());
    }
    RsImGui::PopNewFrame();
    return EXIT_SUCCESS;
}
catch (const rs2::error& e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
