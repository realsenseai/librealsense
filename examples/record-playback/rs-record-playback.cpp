// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include "example.hpp"          // Include short list of convenience functions for rendering
#include <chrono>

#include <imgui.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_glfw.h"
#include <imgui_impl_opengl3.h>
#include<realsense_imgui.h>

// Includes for time display
#include <sstream>
#include <iostream>
#include <iomanip>


// Helper function for dispaying time conveniently
std::string pretty_time(std::chrono::nanoseconds duration);
// Helper function for rendering a seek bar
void draw_seek_bar(rs2::playback& playback, int* seek_pos, float2& location, float width);

int main(int argc, char * argv[]) try
{
    // Create a simple OpenGL window for rendering:
    window app(1280, 720, "RealSense Record and Playback Example");
    // Setup Dear ImGui context
    ImGui::CreateContext();
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(app, false);
    ImGui_ImplOpenGL3_Init();

    // Create booleans to control GUI (recorded - allow play button, recording - show 'recording to file' text)
    bool recorded = false;
    bool recording = false;

    // Declare a texture for the depth image on the GPU
    texture depth_image;

    // Declare frameset and frames which will hold the data from the camera
    rs2::frameset frames;
    rs2::frame depth;

    // Declare depth colorizer for pretty visualization of depth data
    rs2::colorizer color_map;

    // Create a shared pointer to a pipeline
    auto pipe = std::make_shared<rs2::pipeline>();

    // Start streaming with default configuration
    pipe->start();

    // Initialize a shared pointer to a device with the current device on the pipeline
    rs2::device device = pipe->get_active_profile().get_device();

    // Create a variable to control the seek bar
    int seek_pos;

    // While application is running
    while(app) {
        // Flags for displaying ImGui window
        static const int flags = ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove;

        RsImGui::PushNewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize({ app.width(), app.height() });
        ImGui::Begin("app", nullptr, flags);

        // If the device is sreaming live and not from a file
        if (!device.as<rs2::playback>())
        {
            frames = pipe->wait_for_frames(); // wait for next set of frames from the camera
            depth = color_map.process(frames.get_depth_frame()); // Find and colorize the depth data
        }

        // Set options for the ImGui buttons
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, { 1, 1, 1, 1 });
        ImGui::PushStyleColor(ImGuiCol_Button, { 36 / 255.f, 44 / 255.f, 51 / 255.f, 1 });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 40 / 255.f, 170 / 255.f, 90 / 255.f, 1 });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 36 / 255.f, 44 / 255.f, 51 / 255.f, 1 });
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12);

        if (!device.as<rs2::playback>()) // Disable recording while device is playing
        {
            ImGui::SetCursorPos({ app.width() / 2 - 100, 3 * app.height() / 5 + 90});
            ImGui::Text("Click 'record' to start recording");
            ImGui::SetCursorPos({ app.width() / 2 - 100, 3 * app.height() / 5 + 110 });
            if (ImGui::Button("record", { 50, 50 }))
            {
                // If it is the start of a new recording (device is not a recorder yet)
                if (!device.as<rs2::recorder>())
                {
                    pipe->stop(); // Stop the pipeline with the default configuration
                    pipe = std::make_shared<rs2::pipeline>();
                    rs2::config cfg; // Declare a new configuration
                    cfg.enable_record_to_file("a.db3");
                    pipe->start(cfg); //File will be opened at this point
                    device = pipe->get_active_profile().get_device();
                }
                else
                { // If the recording is resumed after a pause, there's no need to reset the shared pointer
                    device.as<rs2::recorder>().resume(); // rs2::recorder allows access to 'resume' function
                }
                recording = true;
            }

            /*
            When pausing, device still holds the file.
            */
            if (device.as<rs2::recorder>())
            {
                if (recording)
                {
                    ImGui::SetCursorPos({ app.width() / 2 - 100, 3 * app.height() / 5 + 60 });
                    ImGui::TextColored({ 255 / 255.f, 64 / 255.f, 54 / 255.f, 1 }, "Recording to file 'a.db3'");
                }

                // Pause the playback if button is clicked
                ImGui::SetCursorPos({ app.width() / 2, 3 * app.height() / 5 + 110 });
                if (ImGui::Button("pause\nrecord", { 50, 50 }))
                {
                    device.as<rs2::recorder>().pause();
                    recording = false;
                }

                ImGui::SetCursorPos({ app.width() / 2 + 100, 3 * app.height() / 5 + 110 });
                if (ImGui::Button(" stop\nrecord", { 50, 50 }))
                {
                    pipe->stop(); // Stop the pipeline that holds the file and the recorder
                    pipe = std::make_shared<rs2::pipeline>(); //Reset the shared pointer with a new pipeline
                    pipe->start(); // Resume streaming with default configuration
                    device = pipe->get_active_profile().get_device();
                    recorded = true; // Now we can run the file
                    recording = false;
                }
            }
        }

        // After a recording is done, we can play it
        if (recorded) {
            ImGui::SetCursorPos({ app.width() / 2 - 100, 4 * app.height() / 5 + 30 });
            ImGui::Text("Click 'play' to start playing");
            ImGui::SetCursorPos({ app.width() / 2 - 100, 4 * app.height() / 5 + 50});
            if (ImGui::Button("play", { 50, 50 }))
            {
                if (!device.as<rs2::playback>())
                {
                    pipe->stop(); // Stop streaming with default configuration
                    pipe = std::make_shared<rs2::pipeline>();
                    rs2::config cfg;
                    cfg.enable_device_from_file("a.db3");
                    pipe->start(cfg); //File will be opened in read mode at this point
                    device = pipe->get_active_profile().get_device();
                }
                else
                {
                    device.as<rs2::playback>().resume();
                }
            }
        }

        // If device is playing a recording, we allow pause and stop
        if (device.as<rs2::playback>())
        {
            rs2::playback playback = device.as<rs2::playback>();
            if (pipe->poll_for_frames(&frames)) // Check if new frames are ready
            {
                depth = color_map.process(frames.get_depth_frame()); // Find and colorize the depth data for rendering
            }

            // Render a seek bar for the player
            float2 location = { app.width() / 4, 4 * app.height() / 5 + 110 };
            draw_seek_bar(playback , &seek_pos, location, app.width() / 2);

            ImGui::SetCursorPos({ app.width() / 2, 4 * app.height() / 5 + 50 });
            if (ImGui::Button(" pause\nplaying", { 50, 50 }))
            {
                playback.pause();
            }

            ImGui::SetCursorPos({ app.width() / 2 + 100, 4 * app.height() / 5 + 50 });
            if (ImGui::Button("  stop\nplaying", { 50, 50 }))
            {
                pipe->stop();
                pipe = std::make_shared<rs2::pipeline>();
                pipe->start();
                device = pipe->get_active_profile().get_device();
            }
        }

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();

        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Render depth frames from the default configuration, the recorder or the playback
        depth_image.render(depth, { app.width() * 0.25f, app.height() * 0.25f, app.width() * 0.5f, app.height() * 0.75f  });
    }
    // Cleanup
    RsImGui::PopNewFrame();
    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cout << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}


std::string pretty_time(std::chrono::nanoseconds duration)
{
    using namespace std::chrono;
    auto hhh = duration_cast<hours>(duration);
    duration -= hhh;
    auto mm = duration_cast<minutes>(duration);
    duration -= mm;
    auto ss = duration_cast<seconds>(duration);
    duration -= ss;
    auto ms = duration_cast<milliseconds>(duration);

    std::ostringstream stream;
    stream << std::setfill('0') << std::setw(hhh.count() >= 10 ? 2 : 1) << hhh.count() << ':' <<
        std::setfill('0') << std::setw(2) << mm.count() << ':' <<
        std::setfill('0') << std::setw(2) << ss.count();
    return stream.str();
}


void draw_seek_bar(rs2::playback& playback, int* seek_pos, float2& location, float width)
{
    int64_t playback_total_duration = playback.get_duration().count();
    auto progress = playback.get_position();
    double part = (1.0 * progress) / playback_total_duration;
    *seek_pos = static_cast<int>(std::max(0.0, std::min(part, 1.0)) * 100);
    auto playback_status = playback.current_status();
    ImGui::PushItemWidth(width);
    ImGui::SetCursorPos({ location.x, location.y });
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12);
    if (ImGui::SliderInt("##seek bar", seek_pos, 0, 100, "", true))
    {
        //Seek was dragged
        if (playback_status != RS2_PLAYBACK_STATUS_STOPPED) //Ignore seek when playback is stopped
        {
            auto duration_db = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(playback.get_duration());
            auto single_percent = duration_db.count() / 100;
            auto seek_time = std::chrono::duration<double, std::nano>((*seek_pos) * single_percent);
            playback.seek(std::chrono::duration_cast<std::chrono::nanoseconds>(seek_time));
        }
    }
    std::string time_elapsed = pretty_time(std::chrono::nanoseconds(progress));
    ImGui::SetCursorPos({ location.x + width + 10, location.y });
    ImGui::Text("%s", time_elapsed.c_str());
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
}
