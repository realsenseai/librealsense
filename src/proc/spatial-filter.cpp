// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include "option.h"
#include "environment.h"
#include "software-sensor.h"
#include <src/depth-sensor.h>
#include "proc/synthetic-stream.h"
#include "proc/hole-filling-filter.h"
#include "proc/spatial-filter.h"

#include <librealsense2/hpp/rs_sensor.hpp>
#include <librealsense2/hpp/rs_processing.hpp>

#include <rsutils/string/from.h>


namespace librealsense
{
    enum spatial_holes_filling_types : uint8_t
    {
        sp_hf_disabled,
        sp_hf_2_pixel_radius,
        sp_hf_4_pixel_radius,
        sp_hf_8_pixel_radius,
        sp_hf_16_pixel_radius,
        sp_hf_unlimited_radius,
        sp_hf_max_value
    };

    // The weight of the current pixel for smoothing is bounded within [25..100]%
    const float alpha_min_val = 0.25f;
    const float alpha_max_val = 1.f;
    const float alpha_default_val = 0.5f;
    const float alpha_step = 0.01f;

    // The depth gradient below which the smoothing will occur as number of depth levels
    const uint8_t delta_min_val = 1;
    const uint8_t delta_max_val = 50;
    const uint8_t delta_default_val = 20;
    const uint8_t delta_step = 1;

    // the number of passes used in the iterative smoothing approach
    const uint8_t filter_iter_min = 1;
    const uint8_t filter_iter_max = 5;
    const uint8_t filter_iter_def = 2;
    const uint8_t filter_iter_step = 1;

    // The holes filling mode
    const uint8_t holes_fill_min = sp_hf_disabled;
    const uint8_t holes_fill_max = sp_hf_max_value - 1;
    const uint8_t holes_fill_step = 1;
    const uint8_t holes_fill_def = sp_hf_disabled;

    spatial_filter::spatial_filter() :
        depth_processing_block("Spatial Filter"),
        _spatial_alpha_param(alpha_default_val),
        _spatial_delta_param(delta_default_val),
        _spatial_iterations(filter_iter_def),
        _width(0), _height(0), _stride(0), _bpp(0),
        _extension_type(RS2_EXTENSION_DEPTH_FRAME),
        _current_frm_size_pixels(0),
        _stereoscopic_depth(false),
        _focal_lenght_mm(0.f),
        _stereo_baseline_mm(0.f),
        _holes_filling_mode(holes_fill_def),
        _holes_filling_radius(0)
    {
        _stream_filter.stream = RS2_STREAM_DEPTH;
        _stream_filter.format = RS2_FORMAT_Z16;

        auto spatial_filter_alpha = std::make_shared<ptr_option<float>>(
            alpha_min_val,
            alpha_max_val,
            alpha_step,
            alpha_default_val,
            &_spatial_alpha_param, "Alpha factor of Exp.moving average, 1 = no filter, 0 = infinite filter");

        auto spatial_filter_delta = std::make_shared<ptr_option<uint8_t>>(
            delta_min_val,
            delta_max_val,
            delta_step,
            delta_default_val,
            &_spatial_delta_param, "Edge-preserving Threshold");

        auto weak_spatial_filter_delta = std::weak_ptr<ptr_option<uint8_t>>();
        spatial_filter_delta->on_set([this, weak_spatial_filter_delta](float val)
        {
            auto strong_spatial_filter_delta = weak_spatial_filter_delta.lock();
            if(!strong_spatial_filter_delta) return;

            if (!strong_spatial_filter_delta->is_valid(val))
                throw invalid_value_exception( rsutils::string::from()
                                               << "Unsupported spatial delta: " << val << " is out of range." );

            std::lock_guard<std::mutex> lock(_mutex);
            _spatial_delta_param = static_cast<uint8_t>(val);
            _spatial_edge_threshold = float(_spatial_delta_param);
        });

        auto spatial_filter_iterations = std::make_shared<ptr_option<uint8_t>>(
            filter_iter_min,
            filter_iter_max,
            filter_iter_step,
            filter_iter_def,
            &_spatial_iterations, "Filtering iterations");

        auto holes_filling_mode = std::make_shared<ptr_option<uint8_t>>(
            holes_fill_min,
            holes_fill_max,
            holes_fill_step,
            holes_fill_def,
            &_holes_filling_mode, "Holes filling mode");

        holes_filling_mode->set_description(sp_hf_disabled, "Disabled");
        holes_filling_mode->set_description(sp_hf_2_pixel_radius, "2-pixel radius");
        holes_filling_mode->set_description(sp_hf_4_pixel_radius, "4-pixel radius");
        holes_filling_mode->set_description(sp_hf_8_pixel_radius, "8-pixel radius");
        holes_filling_mode->set_description(sp_hf_16_pixel_radius, "16-pixel radius");
        holes_filling_mode->set_description(sp_hf_unlimited_radius, "Unlimited");

        auto weak_holes_filling_mode = std::weak_ptr<ptr_option<uint8_t>>(holes_filling_mode);
        holes_filling_mode->on_set([this, weak_holes_filling_mode](float val)
        {
            auto strong_holes_filling_mode = weak_holes_filling_mode.lock();
            if(!strong_holes_filling_mode) return;

            if (!strong_holes_filling_mode->is_valid(val))
                throw invalid_value_exception( rsutils::string::from()
                                               << "Unsupported mode for spatial holes filling selected: value " << val
                                               << " is out of range." );

            std::lock_guard<std::mutex> lock(_mutex);
            _holes_filling_mode = static_cast<uint8_t>(val);
            switch (_holes_filling_mode)
            {
            case sp_hf_disabled:
                _holes_filling_radius = 0;      // disabled
                break;
            case sp_hf_unlimited_radius:
                _holes_filling_radius = 0xff;   // Unrealistic smearing; not particulary useful
                break;
            case sp_hf_2_pixel_radius:
            case sp_hf_4_pixel_radius:
            case sp_hf_8_pixel_radius:
            case sp_hf_16_pixel_radius:
                _holes_filling_radius = 0x1 << _holes_filling_mode; // 2's exponential radius
                break;
            default:
                throw invalid_value_exception( rsutils::string::from()
                                               << "Unsupported spatial hole-filling requested: value "
                                               << _holes_filling_mode << " is out of range." );
            }
        });

        register_option(RS2_OPTION_FILTER_SMOOTH_ALPHA, spatial_filter_alpha);
        register_option(RS2_OPTION_FILTER_SMOOTH_DELTA, spatial_filter_delta);
        register_option(RS2_OPTION_FILTER_MAGNITUDE, spatial_filter_iterations);
        register_option(RS2_OPTION_HOLES_FILL, holes_filling_mode);
    }

    rs2::frame spatial_filter::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        rs2::frame tgt;

        update_configuration(f);
        tgt = prepare_target_frame(f, source);

        // Spatial domain transform edge-preserving filter
        if (_extension_type == RS2_EXTENSION_DISPARITY_FRAME)
            dxf_smooth<float>(const_cast<void*>(tgt.get_data()), _spatial_alpha_param, _spatial_edge_threshold, _spatial_iterations);
        else
            dxf_smooth<uint16_t>(const_cast<void*>(tgt.get_data()), _spatial_alpha_param, _spatial_edge_threshold, _spatial_iterations);

        return tgt;
    }

    void  spatial_filter::update_configuration(const rs2::frame& f)
    {
        if (f.get_profile().get() != _source_stream_profile.get())
        {
            _source_stream_profile = f.get_profile();
            _target_stream_profile = _source_stream_profile.clone(RS2_STREAM_DEPTH, _source_stream_profile.stream_index(), _source_stream_profile.format());

            _extension_type = f.is<rs2::disparity_frame>() ? RS2_EXTENSION_DISPARITY_FRAME : RS2_EXTENSION_DEPTH_FRAME;
            _bpp = (_extension_type == RS2_EXTENSION_DISPARITY_FRAME) ? sizeof(float) : sizeof(uint16_t);
            auto vp = _target_stream_profile.as<rs2::video_stream_profile>();
            _focal_lenght_mm = vp.get_intrinsics().fx;
            _width = vp.width();
            _height = vp.height();
            _stride = _width * _bpp;
            _current_frm_size_pixels = _width * _height;

            // Check if the new frame originated from stereo-based depth sensor
            // retrieve the stereo baseline parameter
            // TODO refactor disparity parameters into the frame's metadata
            auto snr = ((frame_interface*)f.get())->get_sensor().get();
            librealsense::depth_stereo_sensor* dss;

            // Playback sensor
            if (auto a = As<librealsense::extendable_interface>(snr))
            {
                librealsense::depth_stereo_sensor* ptr;
                if ((_stereoscopic_depth = a->extend_to(TypeToExtension<librealsense::depth_stereo_sensor>::value, (void**)&ptr)))
                {
                    dss = ptr;
                    _stereo_baseline_mm = dss->get_stereo_baseline_mm();
                }
            }
            else if (auto depth_emul = As<librealsense::software_sensor>(snr))
            {
                // Software device can obtain these options via Options interface
                if (depth_emul->supports_option(RS2_OPTION_STEREO_BASELINE))
                {
                    _stereo_baseline_mm = depth_emul->get_option(RS2_OPTION_STEREO_BASELINE).query()*1000.f;
                    _stereoscopic_depth = true;
                }
            }
            else // Live sensor
            {
                _stereoscopic_depth = Is<librealsense::depth_stereo_sensor>(snr);
                dss = As<librealsense::depth_stereo_sensor>(snr);
                if (_stereoscopic_depth)
                    _stereo_baseline_mm = dss->get_stereo_baseline_mm();
            }

            _spatial_edge_threshold = _spatial_delta_param;// (_extension_type == RS2_EXTENSION_DISPARITY_FRAME) ?
                                                           // (_focal_lenght_mm * _stereo_baseline_mm) / float(_spatial_delta_param) : _spatial_delta_param;
        }
    }

    rs2::frame spatial_filter::prepare_target_frame(const rs2::frame& f, const rs2::frame_source& source)
    {
        // Allocate and copy the content of the original Depth data to the target
        rs2::frame tgt = source.allocate_video_frame(_target_stream_profile, f, int(_bpp), int(_width), int(_height), int(_stride), _extension_type);

        memmove(const_cast<void*>(tgt.get_data()), f.get_data(), _current_frm_size_pixels * _bpp);
        return tgt;
    }

    void spatial_filter::recursive_filter_horizontal_fp(void * image_data, float alpha, float deltaZ)
    {
        float *image = reinterpret_cast<float*>(image_data);

        int v, u;

        for (v = 0; v < _height;) {
            // left to right
            float *im = image + v * _width;
            float state = *im;
            float previousInnovation = state;

            im++;
            float innovation = *im;
            u = int(_width) - 1;
            if (!(*(int*)&previousInnovation > 0))
                goto CurrentlyInvalidLR;
            // else fall through

        CurrentlyValidLR:
            for (;;) {
                if (*(int*)&innovation > 0) {
                    float delta = previousInnovation - innovation;
                    bool smallDifference = delta < deltaZ && delta > -deltaZ;

                    if (smallDifference) {
                        float filtered = innovation * alpha + state * (1.0f - alpha);
                        *im = state = filtered;
                    }
                    else {
                        state = innovation;
                    }
                    u--;
                    if (u <= 0)
                        goto DoneLR;
                    previousInnovation = innovation;
                    im += 1;
                    innovation = *im;
                }
                else {  // switch to CurrentlyInvalid state
                    u--;
                    if (u <= 0)
                        goto DoneLR;
                    previousInnovation = innovation;
                    im += 1;
                    innovation = *im;
                    goto CurrentlyInvalidLR;
                }
            }

        CurrentlyInvalidLR:
            for (;;) {
                u--;
                if (u <= 0)
                    goto DoneLR;
                if (*(int*)&innovation > 0) { // switch to CurrentlyValid state
                    previousInnovation = state = innovation;
                    im += 1;
                    innovation = *im;
                    goto CurrentlyValidLR;
                }
                else {
                    im += 1;
                    innovation = *im;
                }
            }
        DoneLR:

            // right to left
            im = image + (v + 1) * _width - 2;  // end of row - two pixels
            previousInnovation = state = im[1];
            u = int(_width) - 1;
            innovation = *im;
            if (!(*(int*)&previousInnovation > 0))
                goto CurrentlyInvalidRL;
            // else fall through
        CurrentlyValidRL:
            for (;;) {
                if (*(int*)&innovation > 0) {
                    float delta = previousInnovation - innovation;
                    bool smallDifference = delta < deltaZ && delta > -deltaZ;

                    if (smallDifference) {
                        float filtered = innovation * alpha + state * (1.0f - alpha);
                        *im = state = filtered;
                    }
                    else {
                        state = innovation;
                    }
                    u--;
                    if (u <= 0)
                        goto DoneRL;
                    previousInnovation = innovation;
                    im -= 1;
                    innovation = *im;
                }
                else {  // switch to CurrentlyInvalid state
                    u--;
                    if (u <= 0)
                        goto DoneRL;
                    previousInnovation = innovation;
                    im -= 1;
                    innovation = *im;
                    goto CurrentlyInvalidRL;
                }
            }

        CurrentlyInvalidRL:
            for (;;) {
                u--;
                if (u <= 0)
                    goto DoneRL;
                if (*(int*)&innovation > 0) { // switch to CurrentlyValid state
                    previousInnovation = state = innovation;
                    im -= 1;
                    innovation = *im;
                    goto CurrentlyValidRL;
                }
                else {
                    im -= 1;
                    innovation = *im;
                }
            }
        DoneRL:
            v++;
        }
    }

    void spatial_filter::recursive_filter_vertical_fp(void * image_data, float alpha, float deltaZ)
    {
        float *image = reinterpret_cast<float*>(image_data);

        int v, u;

        // we'll do one column at a time, top to bottom, bottom to top, left to right,

        for (u = 0; u < _width;) {

            float *im = image + u;
            float state = im[0];
            float previousInnovation = state;

            v = int(_height) - 1;
            im += _width;
            float innovation = *im;

            if (!(*(int*)&previousInnovation > 0))
                goto CurrentlyInvalidTB;
            // else fall through

        CurrentlyValidTB:
            for (;;) {
                if (*(int*)&innovation > 0) {
                    float delta = previousInnovation - innovation;
                    bool smallDifference = delta < deltaZ && delta > -deltaZ;

                    if (smallDifference) {
                        float filtered = innovation * alpha + state * (1.0f - alpha);
                        *im = state = filtered;
                    }
                    else {
                        state = innovation;
                    }
                    v--;
                    if (v <= 0)
                        goto DoneTB;
                    previousInnovation = innovation;
                    im += _width;
                    innovation = *im;
                }
                else {  // switch to CurrentlyInvalid state
                    v--;
                    if (v <= 0)
                        goto DoneTB;
                    previousInnovation = innovation;
                    im += _width;
                    innovation = *im;
                    goto CurrentlyInvalidTB;
                }
            }

        CurrentlyInvalidTB:
            for (;;) {
                v--;
                if (v <= 0)
                    goto DoneTB;
                if (*(int*)&innovation > 0) { // switch to CurrentlyValid state
                    previousInnovation = state = innovation;
                    im += _width;
                    innovation = *im;
                    goto CurrentlyValidTB;
                }
                else {
                    im += _width;
                    innovation = *im;
                }
            }
        DoneTB:

            im = image + u + (_height - 2) * _width;
            state = im[_width];
            previousInnovation = state;
            innovation = *im;
            v = int(_height) - 1;
            if (!(*(int*)&previousInnovation > 0))
                goto CurrentlyInvalidBT;
            // else fall through
        CurrentlyValidBT:
            for (;;) {
                if (*(int*)&innovation > 0) {
                    float delta = previousInnovation - innovation;
                    bool smallDifference = delta < deltaZ && delta > -deltaZ;

                    if (smallDifference) {
                        float filtered = innovation * alpha + state * (1.0f - alpha);
                        *im = state = filtered;
                    }
                    else {
                        state = innovation;
                    }
                    v--;
                    if (v <= 0)
                        goto DoneBT;
                    previousInnovation = innovation;
                    im -= _width;
                    innovation = *im;
                }
                else {  // switch to CurrentlyInvalid state
                    v--;
                    if (v <= 0)
                        goto DoneBT;
                    previousInnovation = innovation;
                    im -= _width;
                    innovation = *im;
                    goto CurrentlyInvalidBT;
                }
            }

        CurrentlyInvalidBT:
            for (;;) {
                v--;
                if (v <= 0)
                    goto DoneBT;
                if (*(int*)&innovation > 0) { // switch to CurrentlyValid state
                    previousInnovation = state = innovation;
                    im -= _width;
                    innovation = *im;
                    goto CurrentlyValidBT;
                }
                else {
                    im -= _width;
                    innovation = *im;
                }
            }
        DoneBT:
            u++;
        }
    }
}
