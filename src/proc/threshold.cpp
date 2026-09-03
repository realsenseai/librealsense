// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include <librealsense2/hpp/rs_sensor.hpp>
#include <librealsense2/hpp/rs_processing.hpp>

#include <src/core/depth-frame.h>
#include "proc/synthetic-stream.h"
#include "context.h"
#include "environment.h"
#include "option.h"
#include "threshold.h"
#include "image.h"

namespace librealsense
{
    threshold::threshold() : stream_filter_processing_block("Threshold Filter"),_min(0.1f), _max(4.f)
    {
        _stream_filter.format = RS2_FORMAT_Z16;
        _stream_filter.stream = RS2_STREAM_DEPTH;
        
        auto min_opt = std::make_shared<ptr_option<float>>(0.f, 16.f, 0.1f, 0.1f, &_min, "Min range in meters");

        auto max_opt = std::make_shared<ptr_option<float>>(0.f, 16.f, 0.1f, 4.f, &_max, "Max range in meters");

        register_option(RS2_OPTION_MAX_DISTANCE,
            std::make_shared<max_distance_option>(
                max_opt,
                min_opt));

        register_option(RS2_OPTION_MIN_DISTANCE,
            std::make_shared<min_distance_option>(
                min_opt,
                max_opt));
    }

    rs2::frame threshold::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        if (!f.is<rs2::depth_frame>()) return f;

        if (f.get_profile().get() != _source_stream_profile.get())
        {
            _source_stream_profile = f.get_profile();
            _target_stream_profile = f.get_profile().clone(RS2_STREAM_DEPTH, f.get_profile().stream_index(), RS2_FORMAT_Z16);
        }

        auto vf = f.as<rs2::depth_frame>();
        auto width = vf.get_width();
        auto height = vf.get_height();
        auto new_f = source.allocate_video_frame(_target_stream_profile, f,
            vf.get_bytes_per_pixel(), width, height, vf.get_stride_in_bytes(), RS2_EXTENSION_DEPTH_FRAME);

        if (new_f)
        {
            auto ptr = dynamic_cast<librealsense::depth_frame*>((librealsense::frame_interface*)new_f.get());
            if (!ptr)
                throw std::runtime_error("Frame is not depth frame");

            auto orig = dynamic_cast<librealsense::depth_frame*>((librealsense::frame_interface*)f.get());
            if (!orig)
                throw std::runtime_error("Frame is not depth frame");

            auto depth_data = (uint16_t*)orig->get_frame_data();
            auto new_data = (uint16_t*)ptr->get_frame_data();

            ptr->set_sensor(orig->get_sensor());
            auto du = orig->get_units();

            memset(new_data, 0, width * height * sizeof(uint16_t));
            for (int i = 0; i < width * height; i++)
            {
                auto dist = du * depth_data[i];
                if (dist >= _min && dist <= _max) new_data[i] = depth_data[i];
            }

            return new_f;
        }

        return f;
    }
}
