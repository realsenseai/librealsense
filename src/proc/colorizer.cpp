// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include "../include/librealsense2/hpp/rs_sensor.hpp"
#include "../include/librealsense2/hpp/rs_processing.hpp"

#include "proc/synthetic-stream.h"
#include "context.h"
#include "environment.h"
#include "option.h"
#include "colorizer.h"
#include "disparity-transform.h"


#ifdef _WIN32
#include <intrin.h>
#define cpuid(info, x) __cpuidex(info, x, 0)
#else
#include <cpuid.h>

extern cpuid(int info[4], int info_type); // in colorizer.cpp
#endif

bool has_avx2()
{
	int info[4];
	cpuid(info, 7);
	return (info[1] & ((int)1 << 5)) != 0;
}

namespace librealsense
{
    static color_map hue{ {
        { 255, 0, 0 },
        { 255, 255, 0 },
        { 0, 255, 0 },
        { 0, 255, 255 },
        { 0, 0, 255 },
        { 255, 0, 255 },
        { 255, 0, 0 },
        } };

    static color_map jet{ {
        { 0, 0, 255 },
        { 0, 255, 255 },
        { 255, 255, 0 },
        { 255, 0, 0 },
        { 50, 0, 0 },
        } };

    static color_map classic{ {
        { 30, 77, 203 },
        { 25, 60, 192 },
        { 45, 117, 220 },
        { 204, 108, 191 },
        { 196, 57, 178 },
        { 198, 33, 24 },
        } };

    static color_map grayscale{ {
        { 255, 255, 255 },
        { 0, 0, 0 },
        } };

    static color_map inv_grayscale{ {
        { 0, 0, 0 },
        { 255, 255, 255 },
        } };

    static color_map biomes{ {
        { 0, 0, 204 },
        { 204, 230, 255 },
        { 255, 255, 153 },
        { 170, 255, 128 },
        { 0, 153, 0 },
        { 230, 242, 255 },
        } };

    static color_map cold{ {
        { 230, 247, 255 },
        { 0, 92, 230 },
        { 0, 179, 179 },
        { 0, 51, 153 },
        { 0, 5, 15 }
        } };

    static color_map warm{ {
        { 255, 255, 230 },
        { 255, 204, 0 },
        { 255, 136, 77 },
        { 255, 51, 0 },
        { 128, 0, 0 },
        { 10, 0, 0 }
        } };

    static color_map quantized{ {
        { 255, 255, 255 },
        { 0, 0, 0 },
        }, 6 };

    static color_map pattern{ {
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        { 255, 255, 255 },
        { 0, 0, 0 },
        } };

    colorizer::colorizer()
        : colorizer("Depth Visualization")
    {}

    colorizer::colorizer(const char* name)
        : stream_filter_processing_block(name),
         _min(0.f), _max(6.f), _equalize(true), 
         _target_stream_profile(), _histogram()
    {
        _histogram = std::vector<int>(MAX_DEPTH, 0);
        _hist_data = _histogram.data();
        _stream_filter.stream = RS2_STREAM_DEPTH;
        _stream_filter.format = RS2_FORMAT_Z16;

        _maps = { &jet, &classic, &grayscale, &inv_grayscale, &biomes, &cold, &warm, &quantized, &pattern, &hue };

        auto min_opt = std::make_shared< ptr_option< float > >( 0.f, 16.f, 0.1f, 0.f, &_min, "Min range in meters" );

        auto max_opt = std::make_shared< ptr_option< float > >( 0.f, 16.f, 0.1f, 6.f, &_max, "Max range in meters" );

        auto max_dist_opt = std::make_shared< max_distance_option >( max_opt, min_opt );

        auto min_dist_opt = std::make_shared< min_distance_option >( min_opt, max_opt );

        register_option( RS2_OPTION_MAX_DISTANCE, max_dist_opt );

        register_option( RS2_OPTION_MIN_DISTANCE, min_dist_opt );

        auto hist_opt = std::make_shared< ptr_option< bool > >( false,
                                                                true,
                                                                true,
                                                                true,
                                                                &_equalize,
                                                                "Perform histogram equalization" );
        
        auto weak_hist_opt = std::weak_ptr< ptr_option< bool > >( hist_opt );

        max_dist_opt->add_observer( [weak_hist_opt]( float val ) {
            auto strong_hist_opt = weak_hist_opt.lock();
            if( strong_hist_opt )
                strong_hist_opt->set( false );
        } );

        min_dist_opt->add_observer( [weak_hist_opt]( float val ) {
            auto strong_hist_opt = weak_hist_opt.lock();
            if( strong_hist_opt )
                strong_hist_opt->set( false );
        } );

        auto color_map = std::make_shared<ptr_option<int>>(0, (int)_maps.size() - 1, 1, 0, &_map_index, "Color map");
        color_map->set_description(0.f, "Jet");
        color_map->set_description(1.f, "Classic");
        color_map->set_description(2.f, "White to Black");
        color_map->set_description(3.f, "Black to White");
        color_map->set_description(4.f, "Bio");
        color_map->set_description(5.f, "Cold");
        color_map->set_description(6.f, "Warm");
        color_map->set_description(7.f, "Quantized");
        color_map->set_description(8.f, "Pattern");
        color_map->set_description(9.f, "Hue");
        register_option(RS2_OPTION_COLOR_SCHEME, color_map);

        auto preset_opt = std::make_shared<ptr_option<int>>(0, 3, 1, 0, &_preset, "Preset depth colorization");
        preset_opt->set_description(0.f, "Dynamic");
        preset_opt->set_description(1.f, "Fixed");
        preset_opt->set_description(2.f, "Near");
        preset_opt->set_description(3.f, "Far");

        preset_opt->on_set([this](float val)
        {
            if (fabs(val - 0.f) < 1e-6)
            {
                // Dynamic
                _equalize = true;
                _map_index = 0;
            }
            if (fabs(val - 1.f) < 1e-6)
            {
                // Fixed
                _equalize = false;
                _map_index = 0;
                _min = 0.f;
                _max = 6.f;
            }
            if (fabs(val - 2.f) < 1e-6)
            {
                // Near
                _equalize = false;
                _map_index = 1;
                _min = 0.3f;
                _max = 1.5f;
            }
            if (fabs(val - 3.f) < 1e-6)
            {
                // Far
                _equalize = false;
                _map_index = 0;
                _min = 1.f;
                _max = 16.f;
            }
        });
        register_option(RS2_OPTION_VISUAL_PRESET, preset_opt);

        register_option(RS2_OPTION_HISTOGRAM_EQUALIZATION_ENABLED, hist_opt);
    }

    bool colorizer::should_process(const rs2::frame& frame)
    {
        if (!frame || frame.is<rs2::frameset>())
            return false;

        if ((frame.get_profile().stream_type() == RS2_STREAM_DEPTH) && 
                    (_supported_formats.find(frame.get_profile().format()) != _supported_formats.end()))
            return true;

        return false;
    }

    rs2::frame colorizer::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        if (f.as<rs2::depth_frame>())
            _depth_units = ((depth_frame*)f.get())->get_units();
        if (f.get_profile().get() != _source_stream_profile.get())
        {
            _source_stream_profile = f.get_profile();
            _target_stream_profile = f.get_profile().clone(RS2_STREAM_DEPTH, f.get_profile().stream_index(), RS2_FORMAT_RGB8);

            // workaround for D457
            //auto info = disparity_info::update_info_from_frame(f);
            //_d2d_convert_factor = info.d2d_convert_factor;
            _d2d_convert_factor = 681678.625;
        }

        auto make_equalized_histogram = [this](const rs2::video_frame& depth, rs2::video_frame rgb)
        {
            auto depth_format = depth.get_profile().format();
            const auto w = depth.get_width(), h = depth.get_height();
            auto rgb_data = reinterpret_cast<uint8_t*>(const_cast<void *>(rgb.get_data()));
            auto coloring_function = [&, this](float data) {
                auto hist_data = _hist_data[(int)data];
                auto pixels = (float)_hist_data[MAX_DEPTH - 1];
                return (hist_data / pixels);
            };
#if defined(__AVX2__) and !defined(ANDROID)
           auto coloring_function_avx = [&, this] ( const short unsigned int* data) {
                __m256 hist_data = _mm256_set_ps( (float) _hist_data[(int)*(data+7)],
                                                  (float) _hist_data[(int)*(data+6)],
                                                  (float) _hist_data[(int)*(data+5)],
                                                  (float) _hist_data[(int)*(data+4)],
                                                  (float) _hist_data[(int)*(data+3)],
                                                  (float) _hist_data[(int)*(data+2)],
                                                  (float) _hist_data[(int)*(data+1)],
                                                  (float) _hist_data[(int)*(data+0)] );
                __m256 pixels = _mm256_set1_ps(   (float) _hist_data[MAX_DEPTH - 1 ] );
                __m256 f = _mm256_div_ps( hist_data, pixels );
                return f;
            };
#endif
            if (depth_format == RS2_FORMAT_DISPARITY32)
            {
                auto depth_data = reinterpret_cast<const float*>(depth.get_data());
                update_histogram(_hist_data, depth_data, w, h);
                make_rgb_data<float>(depth_data, rgb_data, w, h, coloring_function);
            }
            else if (depth_format == RS2_FORMAT_Z16)
            {
                auto depth_data = reinterpret_cast<const uint16_t*>(depth.get_data());
                update_histogram(_hist_data, depth_data, w, h);
#if defined(__AVX2__) and !defined(ANDROID)
		if (has_avx2()) 
			make_rgb_data_avx2<uint16_t>(depth_data, rgb_data, w, h, coloring_function_avx);
		else
                	make_rgb_data<uint16_t>(depth_data, rgb_data, w, h, coloring_function);
#else
                make_rgb_data<uint16_t>(depth_data, rgb_data, w, h, coloring_function);
#endif
            }
        };

        auto make_value_cropped_frame = [this](const rs2::video_frame& depth, rs2::video_frame rgb)
        {
            auto depth_format = depth.get_profile().format();
            const auto w = depth.get_width(), h = depth.get_height();
            auto rgb_data = reinterpret_cast<uint8_t*>(const_cast<void *>(rgb.get_data()));

            if (depth_format == RS2_FORMAT_DISPARITY32)
            {
                auto depth_data = reinterpret_cast<const float*>(depth.get_data());
                // convert from depth min max to disparity min max
                // note: max min value is inverted in disparity domain
                auto __min = _min;
                if (__min < 1e-6f) { __min = 1e-6f; } // Min value set to prevent zero division. only when _min is zero. 
                auto max = (_d2d_convert_factor / (__min)) * _depth_units + .5f;
                auto min = (_d2d_convert_factor / (_max)) * _depth_units + .5f;
                auto coloring_function = [&, this](float data) {
                    return (data - min) / (max - min);
                };
                make_rgb_data<float>(depth_data, rgb_data, w, h, coloring_function);
            }
            else if (depth_format == RS2_FORMAT_Z16)
            {
                auto depth_data = reinterpret_cast<const uint16_t*>(depth.get_data());
                auto min = _min;
                auto max = _max;
                auto coloring_function = [&, this](float data) {
                    if (min >= max) return 0.f;
                    return (data * _depth_units - min) / (max - min);
                };
                make_rgb_data<uint16_t>(depth_data, rgb_data, w, h, coloring_function);
            }
        };

        rs2::frame ret;

        auto vf = f.as<rs2::video_frame>();
        ret = source.allocate_video_frame(_target_stream_profile, f, 3, vf.get_width(), vf.get_height(), vf.get_width() * 3, RS2_EXTENSION_VIDEO_FRAME);

        if (_equalize)
            make_equalized_histogram(f, ret);
        else
            make_value_cropped_frame(f, ret);

        return ret;
    }
}
