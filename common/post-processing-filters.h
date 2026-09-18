// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once
#include <librealsense2/rs.hpp>
#include <librealsense2-gl/rs_processing_gl.hpp>
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include "opengl3.h"
#include <GLFW/glfw3.h>


namespace rs2
{
    class viewer_model;
    class processing_block_model;
    class subdevice_model;

    class post_processing_filters
    {
    public:
        post_processing_filters(viewer_model& viewer)
            : processing_block([&](rs2::frame f, const rs2::frame_source& source)
                {
                    process(std::move(f), source);
                }),
            viewer(viewer),
                    depth_stream_active(false),
                    resulting_queue_max_size(20),
                    resulting_queue(static_cast<unsigned int>(resulting_queue_max_size)),
                    render_thread(),
                    render_thread_active(false),
                    pc(new gl::pointcloud()),
                    uploader(new gl::uploader()),
                    disp_to_depth(false)
        {
            std::string s;
            pc_gen = std::make_shared<processing_block_model>(nullptr, "Pointcloud Engine", pc, [=](rs2::frame f) { return pc->calculate(f); }, s);
            processing_block.start(resulting_queue);
        }

                ~post_processing_filters() { stop(); }

                void update_texture(frame f) { pc->map_to(f); }

                /* Start the rendering thread in case its disabled */
                void start();

                /* Stop the rendering thread in case its enabled */
                void stop();

                bool is_rendering() const
                {
                    return render_thread_active.load();
                }

                rs2::frameset get_points()
                {
                    frame f;
                    if (resulting_queue.poll_for_frame(&f))
                    {
                        rs2::frameset frameset(f);
                        model = frameset;
                    }
                    return model;
                }

                void reset(void)
                {
                    rs2::frame f{};
                    model = f;
                    while (resulting_queue.poll_for_frame(&f));
                }

                rs2::frame_queue get_frame_queue(int id)
                {
                    // Return a shared queue handle so callers wait without holding the map lock.
                    std::lock_guard<std::mutex> lock(frames_queue_mutex);
                    return frames_queue.at(id);
                }

                std::atomic<bool> depth_stream_active;

                const size_t resulting_queue_max_size;
                // Capture callbacks must not wait for texture uploads under streams_mutex.
                std::mutex frames_queue_mutex;
                std::map<int, rs2::frame_queue> frames_queue;
                rs2::frame_queue resulting_queue;

                std::shared_ptr<pointcloud> get_pc() const { return pc; }
                std::shared_ptr<processing_block_model> get_pc_model() const { return pc_gen; }

    private:
        viewer_model& viewer;
        void process(rs2::frame f, const rs2::frame_source& source);
        std::vector<rs2::frame> handle_frame(rs2::frame f, const rs2::frame_source& source);

        void map_id(rs2::frame new_frame, rs2::frame old_frame);
        void map_id_frameset_to_frame(rs2::frameset first, rs2::frame second);
        void map_id_frameset_to_frameset(rs2::frameset first, rs2::frameset second);
        void map_id_frame_to_frame(rs2::frame first, rs2::frame second);

        rs2::frame apply_filters(rs2::frame f, const rs2::frame_source& source);
        std::shared_ptr<subdevice_model> get_frame_origin(const rs2::frame& f);

        void zero_first_pixel(const rs2::frame& f);
        rs2::frame last_tex_frame;
        rs2::processing_block processing_block;
        std::shared_ptr<pointcloud> pc;
        rs2::frameset model;
        std::shared_ptr<processing_block_model> pc_gen;
        rs2::disparity_transform disp_to_depth;

        /* Post processing filter rendering */
        std::atomic<bool> render_thread_active; // True when render post processing filter rendering thread is active, False otherwise
        std::shared_ptr<std::thread> render_thread;              // Post processing filter rendering Thread running render_loop()
        void render_loop();                     // Post processing filter rendering function

        std::shared_ptr<gl::uploader> uploader; // GL element that helps pre-emptively copy frames to the GPU
    };
}
