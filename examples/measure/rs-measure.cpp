// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017-24 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include "example.hpp"          // Include short list of convenience functions for rendering

#include <common/cli.h>

// This example will require several standard data-structures and algorithms:
#define _USE_MATH_DEFINES
#include <math.h>
#include <queue>
#include <unordered_set>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>

using pixel = std::pair<int, int>;

// Distance 3D is used to calculate real 3D distance between two pixels
float dist_3d(const rs2::depth_frame& frame, pixel u, pixel v);

// Toggle helper class will be used to render the two buttons
// controlling the edges of our ruler
struct toggle
{
    toggle() : x(0.f), y(0.f) {}
    toggle(float xl, float yl)
        : x(std::min(std::max(xl, 0.f), 1.f)),
          y(std::min(std::max(yl, 0.f), 1.f))
    {}

    // Move from [0,1] space to pixel space of specific frame
    pixel get_pixel(rs2::depth_frame frm) const
    {
        int px = static_cast<int>(x * frm.get_width());
        int py = static_cast<int>(y * frm.get_height());
        return{ px, py };
    }

    void render(const window& app)
    {
        glColor4f(0.f, 0.0f, 0.0f, 0.2f);
        render_circle(app, 10);
        render_circle(app, 8);
        glColor4f(1.f, 0.9f, 1.0f, 1.f);
        render_circle(app, 6);
    }

    void render_circle(const window& app, float r)
    {
        const float segments = 16;
        glBegin(GL_TRIANGLE_STRIP);
        for (auto i = 0; i <= segments; i++)
        {
            auto t = 2 * PI_FL * float(i) / segments;

            glVertex2f(x * app.width() + cos(t) * r,
                y * app.height() + sin(t) * r);

            glVertex2f(x * app.width(),
                y * app.height());
        }
        glEnd();
    }

    // This helper function is used to find the button
    // closest to the mouse cursor
    // Since we are only comparing this distance, sqrt can be safely skipped
    float dist_2d(const toggle& other) const
    {
        return static_cast<float>(pow(x - other.x, 2) + pow(y - other.y, 2));
    }

    float x;
    float y;
    bool selected = false;
};

// Application state shared between the main-thread and GLFW events
struct state
{
    bool mouse_down = false;
    toggle ruler_start;
    toggle ruler_end;
};

// Helper function to register to UI events
void register_glfw_callbacks(window& app, state& app_state);

// Distance rendering functions:

// Simple distance is the classic pythagorean distance between 3D points
// This distance ignores the topology of the object and can cut both through
// air and through solid
void render_simple_distance(const rs2::depth_frame& depth,
                            const state& s,
                            const window& app);

int main(int argc, char * argv[]) try
{
    rs2::rum::set_app_name( "rs-measure" );
    auto settings = rs2::cli( "rs-measure example" )
        .process( argc, argv );

    rs2::context ctx( settings.dump() );
    std::string serial;
    if( ! device_with_streams( ctx, { RS2_STREAM_COLOR, RS2_STREAM_DEPTH }, serial ) )
        return EXIT_SUCCESS;

    // OpenGL textures for the color and depth frames
    texture depth_image, color_image;

    // Colorizer is used to visualize depth data
    rs2::colorizer color_map;
    // Use black to white color map
    color_map.set_option(RS2_OPTION_COLOR_SCHEME, 2.f);
    // Decimation filter reduces the amount of data (while preserving best samples)
    rs2::decimation_filter dec;
    // If the demo is too slow, make sure you run in Release (-DCMAKE_BUILD_TYPE=Release)
    // but you can also increase the following parameter to decimate depth more (reducing quality)
    dec.set_option(RS2_OPTION_FILTER_MAGNITUDE, 2);
    // Define transformations from and to Disparity domain
    rs2::disparity_transform depth2disparity;
    rs2::disparity_transform disparity2depth(false);
    // Define spatial filter (edge-preserving)
    rs2::spatial_filter spat;
    // Enable hole-filling
    // Hole filling is an agressive heuristic and it gets the depth wrong many times
    // However, this demo is not built to handle holes
    // (the shortest-path will always prefer to "cut" through the holes since they have zero 3D distance)
    spat.set_option(RS2_OPTION_HOLES_FILL, 5); // 5 = fill all the zero pixels
    // Define temporal filter
    rs2::temporal_filter temp;
    // Spatially align all streams to depth viewport
    // We do this because:
    //   a. Usually depth has wider FOV, and we only really need depth for this demo
    //   b. We don't want to introduce new holes
    rs2::align align_to(RS2_STREAM_DEPTH);

    // Declare RealSense pipeline, encapsulating the actual device and sensors
    rs2::pipeline pipe( ctx );

    rs2::config cfg;
    if (!serial.empty())
        cfg.enable_device(serial);
    cfg.enable_stream(RS2_STREAM_DEPTH); // Enable default depth
    // For the color stream, set format to RGBA
    // To allow blending of the color frame on top of the depth frame
    cfg.enable_stream(RS2_STREAM_COLOR, RS2_FORMAT_RGBA8);
    auto profile = pipe.start(cfg);

    auto sensor = profile.get_device().first<rs2::depth_sensor>();

    // Set the device to High Accuracy preset of the D400 stereoscopic cameras
    if (sensor && sensor.is<rs2::depth_stereo_sensor>())
    {
        sensor.set_option(RS2_OPTION_VISUAL_PRESET, RS2_RS400_VISUAL_PRESET_HIGH_ACCURACY);
    }

    auto stream = profile.get_stream(RS2_STREAM_DEPTH).as<rs2::video_stream_profile>();

    // Create a simple OpenGL window for rendering:
    window app(stream.width(), stream.height(), "RealSense Measure Example");

    // Define application state and position the ruler buttons
    state app_state;
    app_state.ruler_start = { 0.45f, 0.5f };
    app_state.ruler_end   = { 0.55f, 0.5f };
    register_glfw_callbacks(app, app_state);

    // After initial post-processing, frames will flow into this queue:
    rs2::frame_queue postprocessed_frames;

    // Alive boolean will signal the worker threads to finish-up
    std::atomic_bool alive{ true };

    // Video-processing thread will fetch frames from the camera,
    // apply post-processing and send the result to the main thread for rendering
    // It recieves synchronized (but not spatially aligned) pairs
    // and outputs synchronized and aligned pairs
    std::thread video_processing_thread([&]() {
        while (alive)
        {
            // Fetch frames from the pipeline and send them for processing
            rs2::frameset data;
            if (pipe.poll_for_frames(&data))
            {
                // First make the frames spatially aligned
                data = data.apply_filter(align_to);

                // Decimation will reduce the resultion of the depth image,
                // closing small holes and speeding-up the algorithm
                data = data.apply_filter(dec);

                // To make sure far-away objects are filtered proportionally
                // we try to switch to disparity domain
                data = data.apply_filter(depth2disparity);

                // Apply spatial filtering
                data = data.apply_filter(spat);

                // Apply temporal filtering
                data = data.apply_filter(temp);

                // If we are in disparity domain, switch back to depth
                data = data.apply_filter(disparity2depth);

                //// Apply color map for visualization of depth
                data = data.apply_filter(color_map);

                // Send resulting frames for visualization in the main thread
                postprocessed_frames.enqueue(data);
            }
        }
    });

    rs2::frameset current_frameset;
    while(app) // Application still alive?
    {
        // Fetch the latest available post-processed frameset
        postprocessed_frames.poll_for_frame(&current_frameset);

        if (current_frameset)
        {
            auto depth = current_frameset.get_depth_frame();
            auto color = current_frameset.get_color_frame();
            auto colorized_depth = current_frameset.first(RS2_STREAM_DEPTH, RS2_FORMAT_RGB8);

            glEnable(GL_BLEND);
            // Use the Alpha channel for blending
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // First render the colorized depth image
            depth_image.render(colorized_depth, { 0, 0, app.width(), app.height() });

            // Render the color frame (since we have selected RGBA format
            // pixels out of FOV will appear transparent)
            color_image.render(color, { 0, 0, app.width(), app.height() });

            // Render the simple pythagorean distance
            render_simple_distance(depth, app_state, app);

            // Render the ruler
            app_state.ruler_start.render(app);
            app_state.ruler_end.render(app);

            glColor3f(1.f, 1.f, 1.f);
            glDisable(GL_BLEND);
        }
    }

    // Signal threads to finish and wait until they do
    alive = false;
    video_processing_thread.join();

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

float dist_3d(const rs2::depth_frame& frame, pixel u, pixel v)
{
    float upixel[2]; // From pixel
    float upoint[3]; // From point (in 3D)

    float vpixel[2]; // To pixel
    float vpoint[3]; // To point (in 3D)

    // Copy pixels into the arrays (to match rsutil signatures)
    upixel[0] = static_cast<float>(u.first);
    upixel[1] = static_cast<float>(u.second);
    vpixel[0] = static_cast<float>(v.first);
    vpixel[1] = static_cast<float>(v.second);

    // Query the frame for distance
    // Note: this can be optimized
    // It is not recommended to issue an API call for each pixel
    // (since the compiler can't inline these)
    // However, in this example it is not one of the bottlenecks
    auto udist = frame.get_distance(static_cast<int>(upixel[0]), static_cast<int>(upixel[1]));
    auto vdist = frame.get_distance(static_cast<int>(vpixel[0]), static_cast<int>(vpixel[1]));

    // Deproject from pixel to point in 3D
    rs2_intrinsics intr = frame.get_profile().as<rs2::video_stream_profile>().get_intrinsics(); // Calibration data
    rs2_deproject_pixel_to_point(upoint, &intr, upixel, udist);
    rs2_deproject_pixel_to_point(vpoint, &intr, vpixel, vdist);

    // Calculate euclidean distance between the two points
    return sqrt(pow(upoint[0] - vpoint[0], 2.f) +
                pow(upoint[1] - vpoint[1], 2.f) +
                pow(upoint[2] - vpoint[2], 2.f));
}

void draw_line(float x0, float y0, float x1, float y1, int width)
{
    glPushAttrib(GL_ENABLE_BIT);
    glLineStipple(1, 0x00ff);
    glEnable(GL_LINE_STIPPLE);
    glLineWidth(static_cast<float>(width));
    glBegin(GL_LINE_STRIP);
    glVertex2f(x0, y0);
    glVertex2f(x1, y1);
    glEnd();
    glPopAttrib();
}

void render_simple_distance(const rs2::depth_frame& depth,
                            const state& s,
                            const window& app)
{
    pixel center;

    glColor4f(0.f, 0.0f, 0.0f, 0.2f);
    draw_line(s.ruler_start.x * app.width(),
        s.ruler_start.y * app.height(),
        s.ruler_end.x   * app.width(),
        s.ruler_end.y   * app.height(), 9);

    glColor4f(0.f, 0.0f, 0.0f, 0.3f);
    draw_line(s.ruler_start.x * app.width(),
        s.ruler_start.y * app.height(),
        s.ruler_end.x   * app.width(),
        s.ruler_end.y   * app.height(), 7);

    glColor4f(1.f, 1.0f, 1.0f, 1.f);
    draw_line(s.ruler_start.x * app.width(),
              s.ruler_start.y * app.height(),
              s.ruler_end.x   * app.width(),
              s.ruler_end.y   * app.height(), 3);

    auto from_pixel = s.ruler_start.get_pixel(depth);
    auto to_pixel =   s.ruler_end.get_pixel(depth);
    float air_dist = dist_3d(depth, from_pixel, to_pixel);

    center.first  = (from_pixel.first + to_pixel.first) / 2;
    center.second = (from_pixel.second + to_pixel.second) / 2;

    std::stringstream ss;
    ss << int(air_dist * 100) << " cm";
    auto str = ss.str();
    auto x = (float(center.first)  / depth.get_width())  * app.width() + 15;
    auto y = (float(center.second) / depth.get_height()) * app.height() + 15;

    auto w = stb_easy_font_width((char*)str.c_str());

    // Draw dark background for the text label
    glColor4f(0.f, 0.f, 0.f, 0.4f);
    glBegin(GL_TRIANGLES);
    glVertex2f(x - 3, y - 10);
    glVertex2f(x + w + 2, y - 10);
    glVertex2f(x + w + 2, y + 2);
    glVertex2f(x + w + 2, y + 2);
    glVertex2f(x - 3, y + 2);
    glVertex2f(x - 3, y - 10);
    glEnd();

    // Draw white text label
    glColor4f(1.f, 1.f, 1.f, 1.f);
    draw_text(static_cast<int>(x), static_cast<int>(y), str.c_str());
}

// Implement drag & drop behavior for the buttons:
void register_glfw_callbacks(window& app, state& app_state)
{
    app.on_left_mouse = [&](bool pressed)
    {
        app_state.mouse_down = pressed;
    };

    app.on_mouse_move = [&](double x, double y)
    {
        toggle cursor{ float(x) / app.width(), float(y) / app.height() };
        std::vector<toggle*> toggles{
            &app_state.ruler_start,
            &app_state.ruler_end };

        if (app_state.mouse_down)
        {
            toggle* best = toggles.front();
            for (auto&& t : toggles)
            {
                if (t->dist_2d(cursor) < best->dist_2d(cursor))
                {
                    best = t;
                }
            }
            best->selected = true;
        }
        else
        {
            for (auto&& t : toggles) t->selected = false;
        }

        for (auto&& t : toggles)
        {
            if (t->selected) *t = cursor;
        }
    };
}

