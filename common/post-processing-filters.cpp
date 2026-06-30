// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "viewer.h"
#include "post-processing-filters.h"

using namespace rs2;

rs2::frame post_processing_filters::apply_filters(rs2::frame f, const rs2::frame_source& source)
{
    std::vector<rs2::frame> frames;
    if (auto composite = f.as<rs2::frameset>())
    {
        for (auto&& f : composite)
            frames.push_back(f);
    }
    else
        frames.push_back(f);

    auto res = f;

    //In order to know what are the processing blocks we need to apply
    //We should find all the sub devices releted to the frames
    std::set<std::shared_ptr<subdevice_model>> subdevices;
    for (auto f : frames)
    {
        auto sub = get_frame_origin(f);
        if (sub)
            subdevices.insert(sub);
    }

    for (auto sub : subdevices)
    {
        if (!sub->post_processing_enabled)
            continue;

        for (auto&& pp : sub->post_processing)
            if (pp->is_enabled())
                res = pp->invoke(res);
    }

    return res;
}

std::shared_ptr<subdevice_model> post_processing_filters::get_frame_origin(const rs2::frame& f)
{
    for (auto&& s : viewer.streams)
    {
        if (s.second.dev)
        {
            auto dev = s.second.dev;

            if (s.second.original_profile.unique_id() == f.get_profile().unique_id())
            {
                return dev;
            }
        }
    }
    return nullptr;
}


//Zero the first pixel on frame ,used to invalidate the occlusion pixels
void post_processing_filters::zero_first_pixel(const rs2::frame& f)
{
    auto stream_type = f.get_profile().stream_type();

    switch (stream_type)
    {
    case RS2_STREAM_COLOR:
    {
        auto rgb_stream = const_cast<uint8_t*>(static_cast<const uint8_t*>(f.get_data()));
        memset(rgb_stream, 0, 3);
        // Alternatively, enable the next two lines to render invalidation with magenta color for inspection
        //rgb_stream[0] = rgb_stream[2] = 0xff; // Use magenta to highlight the occlusion areas
        //rgb_stream[1] = 0;
    }
    break;
    case RS2_STREAM_INFRARED:
    {
        auto ir_stream = const_cast<uint8_t*>(static_cast<const uint8_t*>(f.get_data()));
        memset(ir_stream, 0, 2); // Override the first two bytes to cover Y8/Y16 formats
    }
    break;
    default:
        break;
    }
}

void post_processing_filters::map_id(rs2::frame new_frame, rs2::frame old_frame)
{
    if (auto new_set = new_frame.as<rs2::frameset>())
    {
        if (auto old_set = old_frame.as<rs2::frameset>())
        {
            map_id_frameset_to_frameset(new_set, old_set);
        }
        else
        {
            map_id_frameset_to_frame(new_set, old_frame);
        }
    }
    else if (auto old_set = old_frame.as<rs2::frameset>())
    {
        map_id_frameset_to_frame(old_set, new_frame);
    }
    else
        map_id_frame_to_frame(new_frame, old_frame);
}

void post_processing_filters::map_id_frameset_to_frame(rs2::frameset first, rs2::frame second)
{
    if (auto f = first.first_or_default(second.get_profile().stream_type()))
    {
        auto first_uid = f.get_profile().unique_id();
        auto second_uid = second.get_profile().unique_id();

        viewer.streams_origin[first_uid] = second_uid;
        viewer.streams_origin[second_uid] = first_uid;
    }
}

void post_processing_filters::map_id_frameset_to_frameset(rs2::frameset first, rs2::frameset second)
{
    for (auto&& f : first)
    {
        auto first_uid = f.get_profile().unique_id();

        frame second_f;
        if (f.get_profile().stream_type() == RS2_STREAM_INFRARED)
        {
            second_f = second.get_infrared_frame(f.get_profile().stream_index());
        }
        else if( f.get_profile().stream_type() == RS2_STREAM_COLOR )
        {
            second_f = second.get_color_frame( f.get_profile().stream_index() );
        }
        else
        {
            second_f = second.first_or_default(f.get_profile().stream_type());
        }
        if (second_f)
        {
            auto second_uid = second_f.get_profile().unique_id();

            viewer.streams_origin[first_uid] = second_uid;
            viewer.streams_origin[second_uid] = first_uid;
        }
    }
}

void rs2::post_processing_filters::map_id_frame_to_frame(rs2::frame first, rs2::frame second)
{
    if (first.get_profile().stream_type() == second.get_profile().stream_type())
    {
        auto first_uid = first.get_profile().unique_id();
        auto second_uid = second.get_profile().unique_id();

        viewer.streams_origin[first_uid] = second_uid;
        viewer.streams_origin[second_uid] = first_uid;
    }
}


std::vector<rs2::frame> post_processing_filters::handle_frame(rs2::frame f, const rs2::frame_source& source)
{
    std::vector<rs2::frame> res;

    if (uploader) f = uploader->process(f);

    auto filtered = apply_filters(f, source);

    map_id(filtered, f);

    if (auto composite = filtered.as<rs2::frameset>())
    {
        for (auto&& frame : composite)
        {
            res.push_back(frame);
        }
    }
    else
        res.push_back(filtered);

    if (viewer.is_3d_view)
    {
        if (auto depth = viewer.get_3d_depth_source(filtered))
        {
            switch (depth.get_profile().format())
            {
            case RS2_FORMAT_DISPARITY32: depth = disp_to_depth.process(depth); break;
            default: break;
            }

            pc->set_option(RS2_OPTION_FILTER_MAGNITUDE,
                viewer.occlusion_invalidation ? 2.f : 1.f);
            res.push_back(pc->calculate(depth));
        }
        if (auto texture = viewer.get_3d_texture_source(filtered))
        {
            update_texture(texture);
        }
    }

    return res;
}


void post_processing_filters::process(rs2::frame f, const rs2::frame_source& source)
{
    points p;
    std::vector<frame> results;

    auto res = handle_frame(f, source);
    auto frame = source.allocate_composite_frame(res);

    if (frame)
        source.frame_ready(std::move(frame));
}

void post_processing_filters::start()
{
    stop();
    if (render_thread_active.exchange(true) == false)
    {
        viewer.syncer->start();
        render_thread = std::make_shared<std::thread>([&]() {post_processing_filters::render_loop(); });
    }
}

void post_processing_filters::stop()
{
    if (render_thread_active.exchange(false) == true)
    {
        viewer.syncer->stop();
        render_thread->join();
        render_thread.reset();
    }
}
void post_processing_filters::render_loop()
{
    while (render_thread_active)
    {
        try
        {
            if (viewer.synchronization_enable)
            {
                auto frames = viewer.syncer->try_wait_for_frames();
                for (auto f : frames)
                {
                    processing_block.invoke(f);
                }
            }
            else
            {
                std::map<int, rs2::frame_queue> frames_queue_local;
                {
                    std::lock_guard<std::mutex> lock(viewer.streams_mutex);
                    frames_queue_local = frames_queue;
                }
                for (auto&& q : frames_queue_local)
                {
                    frame frm;
                    if (q.second.try_wait_for_frame(&frm, 30))
                    {
                        processing_block.invoke(frm);
                    }
                }
            }
        }
        catch (...) {}
    }
}

