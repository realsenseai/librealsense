// Disparity transformation block is responsible to convert stereoscopic depth to disparity data
// and vice versa
// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/hpp/rs_frame.hpp>
#include <librealsense2/hpp/rs_processing.hpp>
#include <src/core/depth-frame.h>
#include <src/core/sensor-interface.h>
#include <src/depth-sensor.h>
#include "synthetic-stream.h"
#include "calibration-change-tracker.h"

namespace librealsense
{
    class disparity_transform : public generic_processing_block
    {
    public:
        disparity_transform(bool transform_to_disparity);
        bool should_process(const rs2::frame& frame) override;
        rs2::frame process_frame(const rs2::frame_source& source, const rs2::frame& f) override;

    protected:
        rs2::frame prepare_target_frame(const rs2::frame& f, const rs2::frame_source& source);

        template<typename Tin, typename Tout>
        void convert(const void* in_data, void* out_data)
        {
            static_assert((std::is_arithmetic<Tin>::value), "disparity transform requires numeric type for input data");
            static_assert((std::is_arithmetic<Tout>::value), "disparity transform requires numeric type for output data");

            auto in = reinterpret_cast<const Tin*>(in_data);
            auto out = reinterpret_cast<Tout*>(out_data);

            const bool fp = (std::is_floating_point<Tin>::value);
            const float round = fp ? 0.5f : 0.f;

            float input{};
            //TODO SSE optimize
            for (size_t i = 0; i < _height; i++)
                for (size_t j = 0; j < _width; j++)
                {
                    input = *in;
                    if (std::isnormal(input))
                        *out++ = static_cast<Tout>((_d2d_convert_factor / input)+round);
                    else
                        *out++ = 0;
                    in++;
                }
        }

    private:
        void    update_transformation_profile(const rs2::frame& f);

        void    on_set_mode(bool to_disparity);

        bool                    _transform_to_disparity;
        rs2::stream_profile     _source_stream_profile;
        calibration_change_tracker _calibration;
        rs2::stream_profile     _target_stream_profile;
        bool                    _update_target;
        bool                    _stereoscopic_depth;
        float                   _stereo_baseline_meter; // in meters
        float                   _d2d_convert_factor;
        size_t                  _width, _height;
        size_t                  _bpp;
    };
    MAP_EXTENSION(RS2_EXTENSION_DISPARITY_FILTER, librealsense::disparity_transform);

    struct disparity_info
    {
    public:
        struct info {
            bool stereoscopic_depth = false;
            float d2d_convert_factor = 0;
        };

        static info update_info_from_frame(const rs2::frame& f)
        {
            // Check if the new frame originated from stereo-based depth sensor
            // and retrieve the stereo baseline parameter that will be used in transformations
            auto snr = ((frame_interface*)f.get())->get_sensor().get();
            librealsense::depth_stereo_sensor* dss;
            auto info = disparity_info::info();
            float stereo_baseline_meter;

            // Playback sensor
            if (auto a = As<librealsense::extendable_interface>(snr))
            {
                librealsense::depth_stereo_sensor* ptr;
                if ((info.stereoscopic_depth = a->extend_to(TypeToExtension<librealsense::depth_stereo_sensor>::value, (void**)&ptr)))
                {
                    dss = ptr;
                    stereo_baseline_meter = dss->get_stereo_baseline_mm()*0.001f;
                }
            }
            else // Live sensor
            {
                info.stereoscopic_depth = Is<librealsense::depth_stereo_sensor>(snr);
                if (info.stereoscopic_depth)
                {
                    dss = As<librealsense::depth_stereo_sensor>(snr);
                    stereo_baseline_meter = dss->get_stereo_baseline_mm()* 0.001f;
                }
            }

            if (info.stereoscopic_depth)
            {
                auto vp = f.get_profile().as<rs2::video_stream_profile>();
                auto focal_lenght_mm = vp.get_intrinsics().fx;
                const uint8_t fractional_bits = 5;
                const uint8_t fractions = 1 << fractional_bits;
                float depth_units = 0.001f;
                if (f.as<rs2::depth_frame>())
                    depth_units = ((depth_frame*)f.get())->get_units();
                info.d2d_convert_factor = (stereo_baseline_meter * focal_lenght_mm * fractions) / depth_units;
            }

            return info;
        }

    private:
        disparity_info() = delete;
        disparity_info(const disparity_info& other) = delete;
    };
}
