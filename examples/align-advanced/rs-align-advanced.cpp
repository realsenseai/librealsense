// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include "example-imgui.hpp"
#include <common/cli.h>

#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include "imgui_impl_glfw.h"
#include <imgui_impl_opengl3.h>
#include <realsense_imgui.h>

void render_slider(rect location, float& clipping_dist);
void remove_background(rs2::video_frame& other, const rs2::depth_frame& depth_frame, float depth_scale, float clipping_dist);
float get_depth_scale(rs2::device dev);
rs2_stream find_stream_to_align(const std::vector<rs2::stream_profile>& streams);
bool streams_alignable(const std::vector<rs2::stream_profile>& streams);
void configure_alignable_streams(const rs2::device& dev, rs2::config& cfg);

int main(int argc, char * argv[]) try
{
    auto settings = rs2::cli( "rs-aligned-advanced example" )
        .process( argc, argv );

    // Create and initialize GUI related objects
    window app(1280, 720, "RealSense Align (Advanced) Example"); // Simple window handling
    // Setup Dear ImGui context
    ImGui::CreateContext();
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(app, false);
    ImGui_ImplOpenGL3_Init();
    rs2::colorizer c;                     // Helper to colorize depth images
    texture renderer;                     // Helper for renderig images

    // Create a pipeline to easily configure and start the camera
    rs2::pipeline pipe( settings.dump() );

    //Pipeline could choose a device whose default profile has no color/IR stream to align depth to
    //(e.g. some GMSL devices default to depth-only). Resolve the default profile up front, without
    //starting the device, so we can request depth + a non-depth video stream explicitly in that
    //case and start only once.
    rs2::config cfg;
    rs2::pipeline_profile default_profile = cfg.resolve(pipe);
    if (!streams_alignable(default_profile.get_streams()))
        configure_alignable_streams(default_profile.get_device(), cfg);

    //The start function returns the pipeline profile which the pipeline used to start the device
    rs2::pipeline_profile profile = pipe.start(cfg);

    // Each depth camera might have different units for depth pixels, so we get it here
    // Using the pipeline's profile, we can retrieve the device that the pipeline uses
    float depth_scale = get_depth_scale(profile.get_device());

    rs2_stream align_to = find_stream_to_align(profile.get_streams());

    // Create a rs2::align object.
    // rs2::align allows us to perform alignment of depth frames to others frames
    //The "align_to" is the stream type to which we plan to align depth frames.
    rs2::align align(align_to);

    // Define a variable for controlling the distance to clip
    float depth_clipping_distance = 1.f;

    while (app) // Application still alive?
    {
        // Using the align object, we block the application until a frameset is available
        rs2::frameset frameset = pipe.wait_for_frames();

        //Get processed aligned frame
        auto processed = align.process(frameset);

        // Trying to get both other and aligned depth frames
        rs2::video_frame other_frame = processed.first(align_to);
        rs2::depth_frame aligned_depth_frame = processed.get_depth_frame();

        //If one of them is unavailable, continue iteration
        if (!aligned_depth_frame || !other_frame)
        {
            continue;
        }
        // Passing both frames to remove_background so it will "strip" the background
        // NOTE: in this example, we alter the buffer of the other frame, instead of copying it and altering the copy
        //       This behavior is not recommended in real application since the other frame could be used elsewhere
        remove_background(other_frame, aligned_depth_frame, depth_scale, depth_clipping_distance);

        // Taking dimensions of the window for rendering purposes
        float w = static_cast<float>(app.width());
        float h = static_cast<float>(app.height());

        // At this point, "other_frame" is an altered frame, stripped form its background
        // Calculating the position to place the frame in the window
        rect altered_other_frame_rect{ 0, 0, w, h };
        altered_other_frame_rect = altered_other_frame_rect.adjust_ratio({ static_cast<float>(other_frame.get_width()),static_cast<float>(other_frame.get_height()) });

        // Render aligned image
        renderer.render(other_frame, altered_other_frame_rect);

        // The example also renders the depth frame, as a picture-in-picture
        // Calculating the position to place the depth frame in the window
        rect pip_stream{ 0, 0, w / 5, h / 5 };
        pip_stream = pip_stream.adjust_ratio({ static_cast<float>(aligned_depth_frame.get_width()),static_cast<float>(aligned_depth_frame.get_height()) });
        pip_stream.x = altered_other_frame_rect.x + altered_other_frame_rect.w - pip_stream.w - (std::max(w, h) / 25);
        pip_stream.y = altered_other_frame_rect.y + (std::max(w, h) / 25);

        // Render depth (as picture in pipcture)
        renderer.upload(c.process(aligned_depth_frame));
        renderer.show(pip_stream);

        // Using ImGui library to provide a slide controller to select the depth clipping distance
        RsImGui::PushNewFrame();
        render_slider({ 5.f, 0, w, h }, depth_clipping_distance);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    }
    // Cleanup
    RsImGui::PopNewFrame();
    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}

float get_depth_scale(rs2::device dev)
{
    // Go over the device's sensors
    for (rs2::sensor& sensor : dev.query_sensors())
    {
        // Check if the sensor if a depth sensor
        if (rs2::depth_sensor dpt = sensor.as<rs2::depth_sensor>())
        {
            return dpt.get_depth_scale();
        }
    }
    throw std::runtime_error("Device does not have a depth sensor");
}

void render_slider(rect location, float& clipping_dist)
{
    // Some trickery to display the control nicely
    static const int flags = ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove;
    const int pixels_to_buttom_of_stream_text = 25;
    const float slider_window_width = 30;

    ImGui::SetNextWindowPos({ location.x, location.y + pixels_to_buttom_of_stream_text });
    ImGui::SetNextWindowSize({ slider_window_width + 20, location.h - (pixels_to_buttom_of_stream_text * 2) });

    //Render the vertical slider
    ImGui::Begin("slider", nullptr, flags);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImColor(215.f / 255, 215.0f / 255, 215.0f / 255).Value);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImColor(215.f / 255, 215.0f / 255, 215.0f / 255).Value);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImColor(215.f / 255, 215.0f / 255, 215.0f / 255).Value);
    auto slider_size = ImVec2(slider_window_width / 2, location.h - (pixels_to_buttom_of_stream_text * 2) - 20);
    RsImGui::VSliderFloat("##vslider", slider_size, &clipping_dist, 0.0f, 6.0f, "", 1.0f, true);
    if (ImGui::IsItemHovered())
        RsImGui::CustomTooltip("Depth Clipping Distance: %.3f", clipping_dist);
    ImGui::PopStyleColor(3);

    //Display bars next to slider
    float bars_dist = (slider_size.y / 6.0f);
    for (int i = 0; i <= 6; i++)
    {
        ImGui::SetCursorPos({ slider_size.x, i * bars_dist });
        std::string bar_text = "- " + std::to_string(6-i) + "m";
        ImGui::Text("%s", bar_text.c_str());
    }
    ImGui::End();
}

void remove_background(rs2::video_frame& other_frame, const rs2::depth_frame& depth_frame, float depth_scale, float clipping_dist)
{
    const uint16_t* p_depth_frame = reinterpret_cast<const uint16_t*>(depth_frame.get_data());
    uint8_t* p_other_frame = reinterpret_cast<uint8_t*>(const_cast<void*>(other_frame.get_data()));

    int width = other_frame.get_width();
    int height = other_frame.get_height();
    int other_bpp = other_frame.get_bytes_per_pixel();

    #pragma omp parallel for schedule(dynamic) //Using OpenMP to try to parallelise the loop
    for (int y = 0; y < height; y++)
    {
        auto depth_pixel_index = y * width;
        for (int x = 0; x < width; x++, ++depth_pixel_index)
        {
            // Get the depth value of the current pixel
            auto pixels_distance = depth_scale * p_depth_frame[depth_pixel_index];

            // Check if the depth value is invalid (<=0) or greater than the threashold
            if (pixels_distance <= 0.f || pixels_distance > clipping_dist)
            {
                // Calculate the offset in other frame's buffer to current pixel
                auto offset = depth_pixel_index * other_bpp;

                // Set pixel to "background" color (0x999999)
                std::memset(&p_other_frame[offset], 0x99, other_bpp);
            }
        }
    }
}

rs2_stream find_stream_to_align(const std::vector<rs2::stream_profile>& streams)
{
    //Given a vector of streams, we try to find a depth stream and another stream to align depth with.
    //We prioritize color streams to make the view look better.
    //If color is not available, we take another stream that (other than depth)
    rs2_stream align_to = RS2_STREAM_ANY;
    bool depth_stream_found = false;
    bool color_stream_found = false;
    for (rs2::stream_profile sp : streams)
    {
        rs2_stream profile_stream = sp.stream_type();
        if (profile_stream == RS2_STREAM_DEPTH)
        {
            depth_stream_found = true;
        }
        else if (sp.is<rs2::video_stream_profile>())
        {
            if (!color_stream_found)         //Prefer color
                align_to = profile_stream;

            if (profile_stream == RS2_STREAM_COLOR)
            {
                color_stream_found = true;
            }
        }
        //Motion streams (ACCEL/GYRO/POSE) are neither depth nor alignable video - skip them
    }

    if(!depth_stream_found)
        throw std::runtime_error("No Depth stream available");

    if (align_to == RS2_STREAM_ANY)
        throw std::runtime_error("No stream found to align with Depth");

    return align_to;
}

bool streams_alignable(const std::vector<rs2::stream_profile>& streams)
{
    bool depth_found = false, video_found = false;
    for (auto& sp : streams)
    {
        if (sp.stream_type() == RS2_STREAM_DEPTH)
            depth_found = true;
        else if (sp.is<rs2::video_stream_profile>())
            video_found = true;
    }
    return depth_found && video_found;
}

//Explicitly enables depth and a non-depth video stream on cfg, picked from whatever the device
//actually offers. Once any stream is explicitly enabled, streams that aren't are no longer picked
//up by default, so both must be requested together.
void configure_alignable_streams(const rs2::device& dev, rs2::config& cfg)
{
    bool depth_found = false, video_found = false;
    for (auto& sensor : dev.query_sensors())
    {
        for (auto& sp : sensor.get_stream_profiles())
        {
            if (!depth_found && sp.stream_type() == RS2_STREAM_DEPTH)
            {
                cfg.enable_stream(RS2_STREAM_DEPTH, sp.stream_index());
                depth_found = true;
            }
            else if (!video_found && sp.stream_type() != RS2_STREAM_DEPTH && sp.is<rs2::video_stream_profile>())
            {
                cfg.enable_stream(sp.stream_type(), sp.stream_index());
                video_found = true;
            }
        }
        if (depth_found && video_found)
            break;
    }

    if (!depth_found)
        throw std::runtime_error("No Depth stream available");
    if (!video_found)
        throw std::runtime_error("Device has no non-depth video stream to align with Depth");
}

