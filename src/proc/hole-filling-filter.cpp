// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2018 RealSense, Inc. All Rights Reserved.
// Implementation details of hole-filling modes

#include <librealsense2/hpp/rs_sensor.hpp>
#include <librealsense2/hpp/rs_processing.hpp>
#include "option.h"
#include "environment.h"
#include "software-device.h"
#include "proc/synthetic-stream.h"
#include "proc/hole-filling-filter.h"

#include <rsutils/string/from.h>


namespace librealsense
{
    // The holes filling mode
    const uint8_t hole_fill_min = hf_fill_from_left;
    const uint8_t hole_fill_max = hf_max_value - 1;
    const uint8_t hole_fill_step = 1;
    const uint8_t hole_fill_def = hf_farest_from_around;

    hole_filling_filter::hole_filling_filter() :
        depth_processing_block("Hole Filling Filter"),
        _width(0), _height(0), _stride(0), _bpp(0),
        _extension_type(RS2_EXTENSION_DEPTH_FRAME),
        _current_frm_size_pixels(0),
        _hole_filling_mode(hole_fill_def)
    {
        _stream_filter.stream = RS2_STREAM_DEPTH;
        _stream_filter.format = RS2_FORMAT_Z16;

        auto hole_filling_mode = std::make_shared<ptr_option<uint8_t>>(
            hole_fill_min,
            hole_fill_max,
            hole_fill_step,
            hole_fill_def,
            &_hole_filling_mode, "Hole Filling mode");

        hole_filling_mode->set_description(hf_fill_from_left, "Fill from Left");
        hole_filling_mode->set_description(hf_farest_from_around, "Farest from around");
        hole_filling_mode->set_description(hf_nearest_from_around, "Nearest from around");

        auto weak_hole_filling_mode = std::weak_ptr<ptr_option<uint8_t>>(hole_filling_mode);
        hole_filling_mode->on_set([this, weak_hole_filling_mode](float val)
        {
            auto strong_hole_filling_mode = weak_hole_filling_mode.lock();
            if(!strong_hole_filling_mode) return;

            if (!strong_hole_filling_mode->is_valid(val))
                throw invalid_value_exception( rsutils::string::from()
                                               << "Unsupported mode for hole filling selected: value " << val
                                               << " is out of range." );

            _hole_filling_mode = static_cast<uint8_t>(val);
        });

        register_option(RS2_OPTION_HOLES_FILL, hole_filling_mode);
    }

    rs2::frame hole_filling_filter::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        update_configuration(f);
        auto tgt = prepare_target_frame(f, source);

        // Hole filling pass
        if (_extension_type == RS2_EXTENSION_DISPARITY_FRAME)
            apply_hole_filling<float>(const_cast<void*>(tgt.get_data()));
        else
            apply_hole_filling<uint16_t>(const_cast<void*>(tgt.get_data()));

        return tgt;
    }

    void  hole_filling_filter::update_configuration(const rs2::frame& f)
    {
        if (f.get_profile().get() != _source_stream_profile.get())
        {
            _source_stream_profile = f.get_profile();
            _target_stream_profile = _source_stream_profile.clone(RS2_STREAM_DEPTH, _source_stream_profile.stream_index(), _source_stream_profile.format());

            _extension_type = f.is<rs2::disparity_frame>() ? RS2_EXTENSION_DISPARITY_FRAME : RS2_EXTENSION_DEPTH_FRAME;
            _bpp = (_extension_type == RS2_EXTENSION_DISPARITY_FRAME) ? sizeof(float) : sizeof(uint16_t);
            auto vp = _target_stream_profile.as<rs2::video_stream_profile>();
            _width = vp.width();
            _height = vp.height();
            _stride = _width * _bpp;
            _current_frm_size_pixels = _width * _height;

        }
    }

    rs2::frame hole_filling_filter::prepare_target_frame(const rs2::frame& f, const rs2::frame_source& source)
    {
        // Allocate and copy the content of the input data to the target
        rs2::frame tgt = source.allocate_video_frame(_target_stream_profile, f, int(_bpp), int(_width), int(_height), int(_stride), _extension_type);

        memmove(const_cast<void*>(tgt.get_data()), f.get_data(), _current_frm_size_pixels * _bpp);
        return tgt;
    }

}
