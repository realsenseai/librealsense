// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include <librealsense2/hpp/rs_sensor.hpp>
#include <librealsense2/hpp/rs_processing.hpp>

#include "option.h"
#include "stream.h"
#include "ds/d400/d400-private.h"
#include "core/video.h"
#include "proc/synthetic-stream.h"
#include "proc/disparity-transform.h"
#include "software-device.h"
#include "environment.h"

namespace librealsense
{
    disparity_transform::disparity_transform(bool transform_to_disparity):
        generic_processing_block(transform_to_disparity ? "Depth to Disparity" : "Disparity to Depth"),
        _transform_to_disparity(transform_to_disparity),
        _update_target(false),
        _width(0), _height(0), _bpp(0)
    {
        unregister_option(RS2_OPTION_FRAMES_QUEUE_SIZE);

        on_set_mode(_transform_to_disparity);
    }

    bool disparity_transform::should_process(const rs2::frame& frame)
    {
        if (!frame)
            return false;

        if (frame.is<rs2::frameset>())
            return false;

        if (_transform_to_disparity && (frame.get_profile().stream_type() != RS2_STREAM_DEPTH || frame.get_profile().format() != RS2_FORMAT_Z16))
            return false;

        if (!_transform_to_disparity && (frame.get_profile().stream_type() != RS2_STREAM_DEPTH ||
            (frame.get_profile().format() != RS2_FORMAT_DISPARITY16 && frame.get_profile().format() != RS2_FORMAT_DISPARITY32)))
            return false;

        if (frame.is<rs2::disparity_frame>() == _transform_to_disparity)
            return false;

        return true;
    }

    rs2::frame disparity_transform::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        rs2::frame tgt;

        update_transformation_profile(f);

        if (_stereoscopic_depth && (tgt = prepare_target_frame(f, source)))
        {
            auto src = f.as<rs2::video_frame>();

            if (_transform_to_disparity)
                convert<uint16_t, float>(src.get_data(), const_cast<void*>(tgt.get_data()));
            else
                convert<float, uint16_t>(src.get_data(), const_cast<void*>(tgt.get_data()));
        }

        return tgt;
    }

    void disparity_transform::on_set_mode(bool to_disparity)
    {
        _transform_to_disparity = to_disparity;
        _bpp = _transform_to_disparity ? sizeof(float) : sizeof(uint16_t);
        _update_target = true;
    }

    void disparity_transform::update_transformation_profile(const rs2::frame& f)
    {
        if(f.get_profile().get() != _source_stream_profile.get())
        {
            _source_stream_profile = f.get_profile();

            auto info = disparity_info::update_info_from_frame(f);
            _stereoscopic_depth = info.stereoscopic_depth;
            _d2d_convert_factor = info.d2d_convert_factor;

            auto vp = _source_stream_profile.as<rs2::video_stream_profile>();
            _width = vp.width();
            _height = vp.height();
            _update_target = true;
        }

        // Adjust the target profile
        if (_update_target)
        {
            auto tgt_format = _transform_to_disparity ? RS2_FORMAT_DISPARITY32 : RS2_FORMAT_Z16;
            _target_stream_profile = _source_stream_profile.clone(RS2_STREAM_DEPTH, _source_stream_profile.stream_index(), tgt_format);

            auto src_vspi = dynamic_cast<video_stream_profile_interface*>(_source_stream_profile.get()->profile);
            if (!src_vspi)
                throw std::runtime_error("Stream profile is not video stream profile");

            auto tgt_vspi = dynamic_cast<video_stream_profile_interface*>(_target_stream_profile.get()->profile);
            if (!tgt_vspi)
                throw std::runtime_error("Stream profile is not video stream profile");

            rs2_intrinsics src_intrin   = src_vspi->get_intrinsics();

            tgt_vspi->set_intrinsics([src_intrin]() { return src_intrin; });
            tgt_vspi->set_dims(src_intrin.width, src_intrin.height);

            _update_target = false;
        }
    }

    rs2::frame disparity_transform::prepare_target_frame(const rs2::frame& f, const rs2::frame_source& source)
    {
        return source.allocate_video_frame(_target_stream_profile, f, int(_bpp), int(_width), int(_height), int(_width*_bpp),
            _transform_to_disparity ? RS2_EXTENSION_DISPARITY_FRAME :RS2_EXTENSION_DEPTH_FRAME);
    }
}
