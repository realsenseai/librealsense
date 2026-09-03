// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#include <librealsense2/hpp/rs_sensor.hpp>
#include <librealsense2/hpp/rs_processing.hpp>

#include "core/depth-frame.h"
#include "core/sensor-interface.h"
#include "proc/synthetic-stream.h"
#include "environment.h"
#include "units-transform.h"

namespace librealsense
{
    units_transform::units_transform() : stream_filter_processing_block("Units Transform")
    {
        _stream_filter.format = RS2_FORMAT_DISTANCE;
        _stream_filter.stream = RS2_STREAM_DEPTH;
    }

    void units_transform::update_configuration(const rs2::frame& f)
    {
        if (f.get_profile().get() != _source_stream_profile.get())
        {
            _source_stream_profile = f.get_profile();
            _target_stream_profile = f.get_profile().clone(RS2_STREAM_DEPTH, f.get_profile().stream_index(), RS2_FORMAT_DISTANCE);

            if (!_depth_units)
            {
                try 
                {
                    auto sensor = ((frame_interface*)f.get())->get_sensor().get();
                    _depth_units = sensor->get_option(RS2_OPTION_DEPTH_UNITS).query();
                }
                catch (...)
                {
                    // reset stream profile in case of failure.
                    _source_stream_profile = rs2::stream_profile();
                    _depth_units = optional_value<float>();
                    LOG_ERROR("Failed obtaining depth units option");
                }
            }

            auto vf = f.as<rs2::depth_frame>();
            _width = vf.get_width();
            _height = vf.get_height();
            _stride = sizeof(float) * _width;
            _bpp = sizeof(float);
        }
    }

    rs2::frame units_transform::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        update_configuration(f);

        auto new_f = source.allocate_video_frame( _target_stream_profile,
                                                  f,
                                                  (int)_bpp,
                                                  (int)_width,
                                                  (int)_height,
                                                  (int)_stride,
                                                  RS2_EXTENSION_DEPTH_FRAME );

        if (new_f && _depth_units)
        {
            auto ptr = reinterpret_cast< librealsense::frame_interface * >( new_f.get() );
            auto orig = reinterpret_cast< librealsense::frame_interface * >( f.get() );

            auto depth_data = (uint16_t*)orig->get_frame_data();
            auto new_data = (float*)ptr->get_frame_data();

            ptr->set_sensor(orig->get_sensor());

            memset(new_data, 0, _width * _height * sizeof(float));
            for (int i = 0; i < _width * _height; i++)
            {
                float dist = *_depth_units * depth_data[i];
                new_data[i] = dist;
            }

            return new_f;
        }

        return f;
    }

    bool units_transform::should_process(const rs2::frame& frame)
    {
        if (!frame.is<rs2::depth_frame>()) return false;
        return true;
    }
}
