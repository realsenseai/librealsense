// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include <librealsense2/hpp/rs_sensor.hpp>
#include <librealsense2/hpp/rs_processing.hpp>

#include "core/video.h"
#include "core/depth-frame.h"
#include "proc/synthetic-stream.h"
#include "environment.h"
#include "align.h"
#include "stream.h"
#include <rsutils/easylogging/easyloggingpp.h>

#if defined(RS2_USE_CUDA)
#include "proc/cuda/cuda-align.h"
#include "rsutils/accelerators/gpu.h"
#endif
#if defined(__SSSE3__)
#include "proc/sse/sse-align.h"
#endif
#include "proc/neon/neon-align.h"

namespace librealsense
{
    template<int N> struct bytes { uint8_t b[N]; };

    std::shared_ptr<align> align::create_align(rs2_stream align_to)
    {
        #if defined(RS2_USE_CUDA)
        if (rsutils::rs2_is_cuda_available())
        {
            LOG_INFO("Using CUDA-optimized align implementation");
            return std::make_shared<librealsense::align_cuda>(align_to);
        }
        #endif
        #if defined(__SSSE3__)
            LOG_INFO("Using SSE-optimized align implementation");
            return std::make_shared<librealsense::align_sse>(align_to);
        #elif defined(__ARM_NEON) && defined(BUILD_WITH_NEON) && !defined(ANDROID)
            LOG_INFO("Using NEON-optimized align implementation");
            return std::make_shared<librealsense::align_neon>(align_to);
        #else
            LOG_INFO("Using generic (non-SIMD) align implementation");
            return std::make_shared<librealsense::align>(align_to);
        #endif
    }

    template<class GET_DEPTH, class TRANSFER_PIXEL>
    void align_images(const rs2_intrinsics& depth_intrin, const rs2_extrinsics& depth_to_other,
        const rs2_intrinsics& other_intrin, GET_DEPTH get_depth, TRANSFER_PIXEL transfer_pixel)
    {
        // Iterate over the pixels of the depth image
#pragma omp parallel for schedule(dynamic)
        for (int depth_y = 0; depth_y < depth_intrin.height; ++depth_y)
        {
            int depth_pixel_index = depth_y * depth_intrin.width;
            for (int depth_x = 0; depth_x < depth_intrin.width; ++depth_x, ++depth_pixel_index)
            {
                // Skip over depth pixels with the value of zero, we have no depth data so we will not write anything into our aligned images
                if (float depth = get_depth(depth_pixel_index))
                {
                    // Map the top-left corner of the depth pixel onto the other image
                    float depth_pixel[2] = { depth_x - 0.5f, depth_y - 0.5f }, depth_point[3], other_point[3], other_pixel[2];
                    rs2_deproject_pixel_to_point(depth_point, &depth_intrin, depth_pixel, depth);
                    rs2_transform_point_to_point(other_point, &depth_to_other, depth_point);
                    rs2_project_point_to_pixel(other_pixel, &other_intrin, other_point);
                    const int other_x0 = static_cast<int>(other_pixel[0] + 0.5f);
                    const int other_y0 = static_cast<int>(other_pixel[1] + 0.5f);

                    // Map the bottom-right corner of the depth pixel onto the other image
                    depth_pixel[0] = depth_x + 0.5f; depth_pixel[1] = depth_y + 0.5f;
                    rs2_deproject_pixel_to_point(depth_point, &depth_intrin, depth_pixel, depth);
                    rs2_transform_point_to_point(other_point, &depth_to_other, depth_point);
                    rs2_project_point_to_pixel(other_pixel, &other_intrin, other_point);
                    const int other_x1 = static_cast<int>(other_pixel[0] + 0.5f);
                    const int other_y1 = static_cast<int>(other_pixel[1] + 0.5f);

                    if (other_x0 < 0 || other_y0 < 0 || other_x1 >= other_intrin.width || other_y1 >= other_intrin.height)
                        continue;

                    // Transfer between the depth pixels and the pixels inside the rectangle on the other image
                    for (int y = other_y0; y <= other_y1; ++y)
                    {
                        for (int x = other_x0; x <= other_x1; ++x)
                        {
                            transfer_pixel(depth_pixel_index, y * other_intrin.width + x);
                        }
                    }
                }
            }
        }
    }

    align::align(rs2_stream to_stream) : align(to_stream, "Align")
    {}

    void align::align_z_to_other(rs2::video_frame& aligned, 
        const rs2::video_frame& depth, const rs2::video_stream_profile& other_profile, float z_scale)
    {
        uint8_t * aligned_data = reinterpret_cast<uint8_t *>(const_cast<void*>(aligned.get_data()));
        auto aligned_profile = aligned.get_profile().as<rs2::video_stream_profile>();
        memset(aligned_data, 0, aligned_profile.height() * aligned_profile.width() * aligned.get_bytes_per_pixel());

        auto depth_profile = depth.get_profile().as<rs2::video_stream_profile>();

        auto z_intrin = depth_profile.get_intrinsics();
        auto other_intrin = other_profile.get_intrinsics();
        auto z_to_other = depth_profile.get_extrinsics_to(other_profile);

        auto z_pixels = reinterpret_cast<const uint16_t*>(depth.get_data());
        auto out_z = (uint16_t *)(aligned_data);

        align_images(z_intrin, z_to_other, other_intrin,
            [z_pixels, z_scale](int z_pixel_index) { return z_scale * z_pixels[z_pixel_index]; },
            [out_z, z_pixels](int z_pixel_index, int other_pixel_index)
        {
            out_z[other_pixel_index] = out_z[other_pixel_index] ?
                std::min((int)out_z[other_pixel_index], (int)z_pixels[z_pixel_index]) :
                z_pixels[z_pixel_index];
        });
    }

    template<int N, class GET_DEPTH>
    void align_other_to_depth_bytes( uint8_t * other_aligned_to_depth, GET_DEPTH get_depth, const rs2_intrinsics& depth_intrin, const rs2_extrinsics& depth_to_other, const rs2_intrinsics& other_intrin, const uint8_t * other_pixels)
    {
        auto in_other = (const bytes<N> *)(other_pixels);
        auto out_other = (bytes<N> *)(other_aligned_to_depth);
        align_images(depth_intrin, depth_to_other, other_intrin, get_depth,
            [out_other, in_other](int depth_pixel_index, int other_pixel_index) { out_other[depth_pixel_index] = in_other[other_pixel_index]; });
    }

    template<class GET_DEPTH>
    void align_other_to_depth( uint8_t * other_aligned_to_depth, GET_DEPTH get_depth, const rs2_intrinsics& depth_intrin, const rs2_extrinsics & depth_to_other, const rs2_intrinsics& other_intrin, const uint8_t * other_pixels, rs2_format other_format)
    {
        switch (other_format)
        {
        case RS2_FORMAT_Y8:
            align_other_to_depth_bytes<1>(other_aligned_to_depth, get_depth, depth_intrin, depth_to_other, other_intrin, other_pixels);
            break;
        case RS2_FORMAT_Y16:
        case RS2_FORMAT_Z16:
            align_other_to_depth_bytes<2>(other_aligned_to_depth, get_depth, depth_intrin, depth_to_other, other_intrin, other_pixels);
            break;
        case RS2_FORMAT_RGB8:
        case RS2_FORMAT_BGR8:
            align_other_to_depth_bytes<3>(other_aligned_to_depth, get_depth, depth_intrin, depth_to_other, other_intrin, other_pixels);
            break;
        case RS2_FORMAT_RGBA8:
        case RS2_FORMAT_BGRA8:
            align_other_to_depth_bytes<4>(other_aligned_to_depth, get_depth, depth_intrin, depth_to_other, other_intrin, other_pixels);
            break;
        default:
            assert(false); // NOTE: rs2_align_other_to_depth_bytes<2>(...) is not appropriate for RS2_FORMAT_YUYV/RS2_FORMAT_RAW10 images, no logic prevents U/V channels from being written to one another
        }
    }

    void align::align_other_to_z(rs2::video_frame& aligned, const rs2::video_frame& depth, const rs2::video_frame& other, float z_scale)
    {
        uint8_t * aligned_data = reinterpret_cast<uint8_t *>(const_cast<void*>(aligned.get_data()));
        auto aligned_profile = aligned.get_profile().as<rs2::video_stream_profile>();
        memset(aligned_data, 0, aligned_profile.height() * aligned_profile.width() * aligned.get_bytes_per_pixel());

        auto depth_profile = depth.get_profile().as<rs2::video_stream_profile>();
        auto other_profile = other.get_profile().as<rs2::video_stream_profile>();

        auto z_intrin = depth_profile.get_intrinsics();
        auto other_intrin = other_profile.get_intrinsics();
        auto z_to_other = depth_profile.get_extrinsics_to(other_profile);

        auto z_pixels = reinterpret_cast<const uint16_t*>(depth.get_data());
        auto other_pixels = reinterpret_cast<const uint8_t *>(other.get_data());

        align_other_to_depth(aligned_data, [z_pixels, z_scale](int z_pixel_index) { return z_scale * z_pixels[z_pixel_index]; },
            z_intrin, z_to_other, other_intrin, other_pixels, other_profile.format());
    }

    std::shared_ptr<rs2::video_stream_profile> align::create_aligned_profile(
        rs2::video_stream_profile& original_profile,
        rs2::video_stream_profile& to_profile)
    {
        auto from_to = std::make_pair(original_profile.get()->profile, to_profile.get()->profile);
        auto it = _align_stream_unique_ids.find(from_to);
        if (it != _align_stream_unique_ids.end())
        {
            return it->second;
        }
        auto aligned_profile = std::make_shared<rs2::video_stream_profile>(original_profile.clone(original_profile.stream_type(), original_profile.stream_index(), original_profile.format()));
        aligned_profile->get()->profile->set_framerate(original_profile.fps());
        if (auto original_video_profile = As<video_stream_profile_interface>(original_profile.get()->profile))
        {
            if (auto to_video_profile = As<video_stream_profile_interface>(to_profile.get()->profile))
            {
                if (auto aligned_video_profile = As<video_stream_profile_interface>(aligned_profile->get()->profile))
                {
                    aligned_video_profile->set_dims(to_video_profile->get_width(), to_video_profile->get_height());
                    auto aligned_intrinsics = to_video_profile->get_intrinsics();
                    aligned_intrinsics.width = to_video_profile->get_width();
                    aligned_intrinsics.height = to_video_profile->get_height();
                    aligned_video_profile->set_intrinsics([aligned_intrinsics]() { return aligned_intrinsics; });
                    aligned_profile->register_extrinsics_to(to_profile, { { 1,0,0,0,1,0,0,0,1 },{ 0,0,0 } });
                }
            }
        }
        _align_stream_unique_ids[from_to] = aligned_profile;
        reset_cache(original_profile.stream_type(), to_profile.stream_type());
        return aligned_profile;
    }

    bool align::should_process(const rs2::frame& frame)
    {
        if (!frame)
            return false;

        auto set = frame.as<rs2::frameset>();
        if (!set)
            return false;

        auto profile = frame.get_profile();
        rs2_stream stream = profile.stream_type();
        rs2_format format = profile.format();
        int index = profile.stream_index();

        //process composite frame only if it contains both a depth frame and the requested texture frame
        bool has_tex = false, has_depth = false;
        set.foreach_rs([this, &has_tex](const rs2::frame& frame) { if (frame.get_profile().stream_type() == _to_stream_type) has_tex = true; });
        set.foreach_rs([&has_depth](const rs2::frame& frame)
            { if (frame.get_profile().stream_type() == RS2_STREAM_DEPTH && frame.get_profile().format() == RS2_FORMAT_Z16) has_depth = true; });
        if (!has_tex || !has_depth)
            return false;

        return true;
    }

    rs2_extension align::select_extension(const rs2::frame& input)
    {
        auto ext = input.is<rs2::depth_frame>() ? RS2_EXTENSION_DEPTH_FRAME : RS2_EXTENSION_VIDEO_FRAME;
        return ext;
    }

    rs2::video_frame align::allocate_aligned_frame(const rs2::frame_source& source, const rs2::video_frame& from, const rs2::video_frame& to)
    {
        rs2::frame rv;
        auto from_bytes_per_pixel = from.get_bytes_per_pixel();

        auto from_profile = from.get_profile().as<rs2::video_stream_profile>();
        auto to_profile = to.get_profile().as<rs2::video_stream_profile>();

        auto aligned_profile = create_aligned_profile(from_profile, to_profile);

        auto ext = select_extension(from);

        rv = source.allocate_video_frame(
            *aligned_profile,
            from,
            from_bytes_per_pixel,
            to.get_width(),
            to.get_height(),
            to.get_width() * from_bytes_per_pixel,
            ext);
        return rv;
    }

    void align::align_frames(rs2::video_frame& aligned, const rs2::video_frame& from, const rs2::video_frame& to)
    {
        auto from_profile = from.get_profile().as<rs2::video_stream_profile>();
        auto to_profile = to.get_profile().as<rs2::video_stream_profile>();

        auto aligned_profile = aligned.get_profile().as<rs2::video_stream_profile>();

        if (to_profile.stream_type() == RS2_STREAM_DEPTH)
        {
            align_other_to_z(aligned, to, from, _depth_scale);
        }
        else
        {
            align_z_to_other(aligned, from, to_profile, _depth_scale);
        }
    }

    rs2::frame align::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        rs2::frame rv;
        std::vector<rs2::frame> output_frames;
        std::vector<rs2::frame> other_frames;

        auto frames = f.as<rs2::frameset>();
        auto depth = frames.first_or_default(RS2_STREAM_DEPTH, RS2_FORMAT_Z16).as<rs2::depth_frame>();

        _depth_scale = ((librealsense::depth_frame*)depth.get())->get_units();

        if (_to_stream_type == RS2_STREAM_DEPTH)
            frames.foreach_rs([&other_frames](const rs2::frame& f) {if ((f.get_profile().stream_type() != RS2_STREAM_DEPTH) && f.is<rs2::video_frame>()) other_frames.push_back(f); });
        else
            frames.foreach_rs([this, &other_frames](const rs2::frame& f) {if (f.get_profile().stream_type() == _to_stream_type) other_frames.push_back(f); });

        if (_to_stream_type == RS2_STREAM_DEPTH)
        {
            for (auto from : other_frames)
            {
                auto aligned_frame = allocate_aligned_frame(source, from, depth);
                align_frames(aligned_frame, from, depth);
                output_frames.push_back(aligned_frame);
            }
        }
        else
        {
            auto to = other_frames.front();
            auto aligned_frame = allocate_aligned_frame(source, depth, to);
            align_frames(aligned_frame, depth, to);
            output_frames.push_back(aligned_frame);
        }

        auto new_composite = source.allocate_composite_frame(std::move(output_frames));
        return new_composite;
    }
}
