// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include <librealsense2/hpp/rs_sensor.hpp>
#include <librealsense2/hpp/rs_processing.hpp>

#include <numeric>
#include <cmath>
#include "environment.h"
#include "option.h"
#include "stream.h"
#include "core/video.h"
#include "proc/synthetic-stream.h"
#include "proc/decimation-filter.h"

#include <rsutils/string/from.h>


#define PIX_SORT(a,b) { if ((a)>(b)) PIX_SWAP((a),(b)); }
#define PIX_SWAP(a,b) { pixelvalue temp=(a);(a)=(b);(b)=temp; }
#define PIX_MIN(a,b) ((a)>(b)) ? (b) : (a)
#define PIX_MAX(a,b) ((a)>(b)) ? (a) : (b)

namespace librealsense
{
    /*----------------------------------------------------------------------------
    Function :   opt_med3()
    In       :   pointer to array of 3 pixel values
    Out      :   a pixelvalue
    Job      :   optimized search of the median of 3 pixel values
    Notice   :   found on sci.image.processing
    cannot go faster unless assumptions are made
    on the nature of the input signal.
    ---------------------------------------------------------------------------*/
    template <class pixelvalue>
    inline pixelvalue opt_med3(pixelvalue * p)
    {
        PIX_SORT(p[0], p[1]);
        PIX_SORT(p[1], p[2]);
        PIX_SORT(p[0], p[1]);
        return p[1];
    }

    /*
    ** opt_med4()
    hacked version
    **/
    template <class pixelvalue>
    inline pixelvalue opt_med4(pixelvalue * p)
    {
        PIX_SORT(p[0], p[1]);
        PIX_SORT(p[2], p[3]);
        PIX_SORT(p[0], p[2]);
        PIX_SORT(p[1], p[3]);
        return PIX_MIN(p[1], p[2]);
    }

    /*----------------------------------------------------------------------------
    Function :   opt_med5()
    In       :   pointer to array of 5 pixel values
    Out      :   a pixelvalue
    Job      :   optimized search of the median of 5 pixel values
    Notice   :   found on sci.image.processing
    cannot go faster unless assumptions are made
    on the nature of the input signal.
    ---------------------------------------------------------------------------*/
    template <class pixelvalue>
    inline pixelvalue opt_med5(pixelvalue * p)
    {
        PIX_SORT(p[0], p[1]);
        PIX_SORT(p[3], p[4]);
        p[3] = PIX_MAX(p[0], p[3]);
        p[1] = PIX_MIN(p[1], p[4]);
        PIX_SORT(p[1], p[2]);
        p[2] = PIX_MIN(p[2], p[3]);
        return PIX_MAX(p[1], p[2]);
    }

    /*----------------------------------------------------------------------------
    Function :   opt_med6()
    In       :   pointer to array of 6 pixel values
    Out      :   a pixelvalue
    Job      :   optimized search of the median of 6 pixel values
    Notice   :   from Christoph_John@gmx.de
    based on a selection network which was proposed in
    "FAST, EFFICIENT MEDIAN FILTERS WITH EVEN LENGTH WINDOWS"
    J.P. HAVLICEK, K.A. SAKADY, G.R.KATZ
    If you need larger even length kernels check the paper
    ---------------------------------------------------------------------------*/
    template <class pixelvalue>
    inline pixelvalue opt_med6(pixelvalue * p)
    {
        PIX_SORT(p[1], p[2]);
        PIX_SORT(p[3], p[4]);
        PIX_SORT(p[0], p[1]);
        PIX_SORT(p[2], p[3]);
        PIX_SORT(p[4], p[5]);
        PIX_SORT(p[1], p[2]);
        PIX_SORT(p[3], p[4]);
        PIX_SORT(p[0], p[1]);
        PIX_SORT(p[2], p[3]);
        p[4] = PIX_MIN(p[4], p[5]);
        p[2] = PIX_MAX(p[1], p[2]);
        p[3] = PIX_MIN(p[3], p[4]);
        return PIX_MIN(p[2], p[3]);
    }

    /*----------------------------------------------------------------------------
    Function :   opt_med7()
    In       :   pointer to array of 7 pixel values
    Out      :   a pixelvalue
    Job      :   optimized search of the median of 7 pixel values
    Notice   :   found on sci.image.processing
    cannot go faster unless assumptions are made
    on the nature of the input signal.
    ---------------------------------------------------------------------------*/
    template <class pixelvalue>
    inline pixelvalue opt_med7(pixelvalue * p)
    {
        PIX_SORT(p[0], p[5]);
        PIX_SORT(p[0], p[3]);
        PIX_SORT(p[1], p[6]);
        PIX_SORT(p[2], p[4]);
        PIX_SORT(p[0], p[1]);
        PIX_SORT(p[3], p[5]);
        PIX_SORT(p[2], p[6]);
        p[3] = PIX_MAX(p[2], p[3]);
        p[3] = PIX_MIN(p[3], p[6]);
        p[4] = PIX_MIN(p[4], p[5]);
        PIX_SORT(p[1], p[4]);
        p[3] = PIX_MAX(p[1], p[3]);
        return PIX_MIN(p[3], p[4]);
    }

    /*----------------------------------------------------------------------------
    Function :   opt_med9()
    Hacked version of opt_med9()
    */
    template <class pixelvalue>
    inline pixelvalue opt_med8(pixelvalue * p)
    {
        PIX_SORT(p[0], p[1]);
        PIX_SORT(p[3], p[4]);
        PIX_SORT(p[6], p[7]);
        PIX_SORT(p[2], p[3]);
        PIX_SORT(p[5], p[6]);
        PIX_SORT(p[3], p[4]);
        PIX_SORT(p[6], p[7]);
        p[4] = PIX_MIN(p[4], p[7]);
        PIX_SORT(p[3], p[6]);
        p[5] = PIX_MAX(p[2], p[5]);
        p[3] = PIX_MAX(p[0], p[3]);
        p[1] = PIX_MIN(p[1], p[4]);
        p[3] = PIX_MIN(p[3], p[6]);
        PIX_SORT(p[3], p[1]);
        p[3] = PIX_MAX(p[5], p[3]);
        return PIX_MIN(p[3], p[1]);
    }

    /*----------------------------------------------------------------------------
    Function :   opt_med9()
    In       :   pointer to an array of 9 pixelvalues
    Out      :   a pixelvalue
    Job      :   optimized search of the median of 9 pixelvalues
    Notice   :   in theory, cannot go faster without assumptions on the
    signal.
    Formula from:
    XILINX XCELL magazine, vol. 23 by John L. Smith

    The input array is modified in the process
    The result array is guaranteed to contain the median
    value
    in middle position, but other elements are NOT sorted.
    ---------------------------------------------------------------------------*/
    template <class pixelvalue>
    inline pixelvalue opt_med9(pixelvalue * p)
    {
        PIX_SORT(p[1], p[2]);
        PIX_SORT(p[4], p[5]);
        PIX_SORT(p[7], p[8]);
        PIX_SORT(p[0], p[1]);
        PIX_SORT(p[3], p[4]);
        PIX_SORT(p[6], p[7]);
        PIX_SORT(p[1], p[2]);
        PIX_SORT(p[4], p[5]);
        PIX_SORT(p[7], p[8]);
        p[3] = PIX_MAX(p[0], p[3]);
        p[5] = PIX_MIN(p[5], p[8]);
        PIX_SORT(p[4], p[7]);
        p[6] = PIX_MAX(p[3], p[6]);
        p[4] = PIX_MAX(p[1], p[4]);
        p[2] = PIX_MIN(p[2], p[5]);
        p[4] = PIX_MIN(p[4], p[7]);
        PIX_SORT(p[4], p[2]);
        p[4] = PIX_MAX(p[6], p[4]);
        return PIX_MIN(p[4], p[2]);
    }

    const uint8_t decimation_min_val = 1;
    const uint8_t decimation_max_val = 8;    // Decimation levels according to the reference design
    const uint8_t decimation_default_val = 2;
    const uint8_t decimation_step = 1;    // Linear decimation

    decimation_filter::decimation_filter() :
        stream_filter_processing_block("Decimation Filter"),
        _decimation_factor(decimation_default_val),
        _control_val(decimation_default_val),
        _patch_size(decimation_default_val),
        _kernel_size(_patch_size*_patch_size),
        _real_width(),
        _real_height(0),
        _padded_width(0),
        _padded_height(0),
        _recalc_profile(false),
        _options_changed(false)
    {
        _stream_filter.stream = RS2_STREAM_DEPTH;
        _stream_filter.format = RS2_FORMAT_Z16;

        auto decimation_control = std::make_shared<ptr_option<uint8_t>>(
            decimation_min_val,
            decimation_max_val,
            decimation_step,
            decimation_default_val,
            &_control_val, "Resolution reduction. Higher = less noise and CPU, coarser detail");

        auto weak_decimation_control = std::weak_ptr<ptr_option<uint8_t>>(decimation_control);
        decimation_control->on_set([this, weak_decimation_control](float val)
        {
            auto strong_decimation_control = weak_decimation_control.lock();
            if(!strong_decimation_control) return;

            std::lock_guard<std::mutex> lock(_mutex);

            if (!strong_decimation_control->is_valid(val))
                throw invalid_value_exception( rsutils::string::from()
                                               << "Unsupported decimation scale " << val << " is out of range." );

            // Linear decimation factor
            if (_control_val != _decimation_factor)
            {
                _patch_size = _decimation_factor = _control_val;
                _kernel_size = _patch_size * _patch_size;
                _options_changed = true;
            }
        });

        register_option(RS2_OPTION_FILTER_MAGNITUDE, decimation_control);
    }

    rs2::frame decimation_filter::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        update_output_profile(f);

        auto src = f.as<rs2::video_frame>();
        rs2::stream_profile profile = f.get_profile();
        rs2_format format = profile.format();
        rs2_stream type = profile.stream_type();

        rs2_extension tgt_type;
        if (type == RS2_STREAM_COLOR || type == RS2_STREAM_INFRARED)
            tgt_type = RS2_EXTENSION_VIDEO_FRAME;
        else
            tgt_type = f.is<rs2::disparity_frame>() ? RS2_EXTENSION_DISPARITY_FRAME : RS2_EXTENSION_DEPTH_FRAME;

        if (auto tgt = prepare_target_frame(f, source, tgt_type))
        {
            if (format == RS2_FORMAT_Z16)
            {
                decimate_depth(static_cast<const uint16_t*>(src.get_data()),
                    static_cast<uint16_t*>(const_cast<void*>(tgt.get_data())),
                    src.get_width(), src.get_height(), this->_patch_size);
            }
            else
            {
                decimate_others(format, src.get_data(),
                    const_cast<void*>(tgt.get_data()),
                    src.get_width(), src.get_height(), this->_patch_size);
            }
            return tgt;
        }
        return f;
    }

    void  decimation_filter::update_output_profile(const rs2::frame& f)
    {
        if (_options_changed || f.get_profile().get() != _source_stream_profile.get())
        {
            _options_changed = false;
            _source_stream_profile = f.get_profile();
            const auto pf = _registered_profiles.find(std::make_tuple(_source_stream_profile.get(), _decimation_factor));
            if (_registered_profiles.end() != pf)
            {
                _target_stream_profile = pf->second;
                auto tgt_vspi = dynamic_cast<video_stream_profile_interface*>(_target_stream_profile.get()->profile);
                if (!tgt_vspi)
                    throw std::runtime_error("Stream profile interface is not video stream profile interface");

                auto f_pf = dynamic_cast<video_stream_profile_interface*>(_source_stream_profile.get()->profile);
                if (!f_pf)
                    throw std::runtime_error("Stream profile interface is not video stream profile interface");

                rs2_intrinsics tgt_intrin = tgt_vspi->get_intrinsics();

                // Update real/padded output frame size based on retrieved input properties
                _real_width = f_pf->get_width() / _patch_size;
                _real_height = f_pf->get_height() / _patch_size;
                _padded_width = tgt_intrin.width;
                _padded_height = tgt_intrin.height;
            }
            else
            {
                _recalc_profile = true;
            }
        }

        // Buld a new target profile for every system/filter change
        if (_recalc_profile)
        {
            auto vp = _source_stream_profile.as<rs2::video_stream_profile>();

            auto tmp_profile = _source_stream_profile.clone(_source_stream_profile.stream_type(), _source_stream_profile.stream_index(), _source_stream_profile.format());
            auto src_vspi = dynamic_cast<video_stream_profile_interface*>(_source_stream_profile.get()->profile);
            if (!src_vspi)
                throw std::runtime_error("Stream profile interface is not video stream profile interface");

            auto tgt_vspi = dynamic_cast<video_stream_profile_interface*>(tmp_profile.get()->profile);
            if( ! tgt_vspi )
                throw std::runtime_error( "Profile is not video stream profile" );

            rs2_intrinsics src_intrin = src_vspi->get_intrinsics();
            rs2_intrinsics tgt_intrin = tgt_vspi->get_intrinsics();

            // recalculate real/padded output frame size based on new input porperties
            _real_width = src_vspi->get_width() / _patch_size;
            _real_height = src_vspi->get_height() / _patch_size;

            // The resulted frame dimension will be dividible by 4;
            _padded_width = _real_width + 3;
            _padded_width /= 4;
            _padded_width *= 4;

            _padded_height = _real_height + 3;
            _padded_height /= 4;
            _padded_height *= 4;

            tgt_intrin.width = _padded_width;
            tgt_intrin.height = _padded_height;
            tgt_intrin.fx = src_intrin.fx / _patch_size;
            tgt_intrin.fy = src_intrin.fy / _patch_size;
            tgt_intrin.ppx = src_intrin.ppx / _patch_size;
            tgt_intrin.ppy = src_intrin.ppy / _patch_size;

            tgt_vspi->set_intrinsics([tgt_intrin]() { return tgt_intrin; });
            tgt_vspi->set_dims(tgt_intrin.width, tgt_intrin.height);

            _registered_profiles[std::make_tuple(_source_stream_profile.get(), _decimation_factor)] = _target_stream_profile = tmp_profile;

            _recalc_profile = false;
        }
    }

    rs2::frame decimation_filter::prepare_target_frame(const rs2::frame& f, const rs2::frame_source& source, rs2_extension tgt_type)
    {
        auto vf = f.as<rs2::video_frame>();
        auto ret = source.allocate_video_frame(_target_stream_profile, f,
            vf.get_bytes_per_pixel(),
            _padded_width,
            _padded_height,
            _padded_width*vf.get_bytes_per_pixel(),
            tgt_type);

        return ret;
    }

    void decimation_filter::decimate_depth(const uint16_t * frame_data_in, uint16_t * frame_data_out,
        size_t width_in, size_t height_in, size_t scale)
    {
        // Use median filtering
        std::vector<uint16_t> working_kernel(_kernel_size);
        auto wk_begin = working_kernel.data();
        auto wk_itr = wk_begin;
        std::vector<uint16_t*> pixel_raws(scale);
        uint16_t* block_start = const_cast<uint16_t*>(frame_data_in);

        if (scale == 2 || scale == 3)
        {
            for (int j = 0; j < _real_height; j++)
            {
                uint16_t *p{};
                // Mark the beginning of each of the N lines that the filter will run upon
                for (size_t i = 0; i < pixel_raws.size(); i++)
                    pixel_raws[i] = block_start + (width_in*i);

                for (size_t i = 0, chunk_offset = 0; i < _real_width; i++)
                {
                    wk_itr = wk_begin;
                    // extract data the kernel to process
                    for (size_t n = 0; n < scale; ++n)
                    {
                        p = pixel_raws[n] + chunk_offset;
                        for (size_t m = 0; m < scale; ++m)
                        {
                            if (*(p + m))
                                *wk_itr++ = *(p + m);
                        }
                    }

                    // For even-size kernels pick the member one below the middle
                    auto ks = (int)(wk_itr - wk_begin);
                    if (ks == 0)
                        *frame_data_out++ = 0;
                    else
                    {
                        switch (ks)
                        {
                        case 1:
                            *frame_data_out++ = working_kernel[0];
                            break;
                        case 2:
                            *frame_data_out++ = PIX_MIN(working_kernel[0], working_kernel[1]);
                            break;
                        case 3:
                            *frame_data_out++ = opt_med3<uint16_t>(working_kernel.data());
                            break;
                        case 4:
                            *frame_data_out++ = opt_med4<uint16_t>(working_kernel.data());
                            break;
                        case 5:
                            *frame_data_out++ = opt_med5<uint16_t>(working_kernel.data());
                            break;
                        case 6:
                            *frame_data_out++ = opt_med6<uint16_t>(working_kernel.data());
                            break;
                        case 7:
                            *frame_data_out++ = opt_med7<uint16_t>(working_kernel.data());
                            break;
                        case 8:
                            *frame_data_out++ = opt_med8<uint16_t>(working_kernel.data());
                            break;
                        case 9:
                            *frame_data_out++ = opt_med9<uint16_t>(working_kernel.data());
                            break;
                        }
                    }

                    chunk_offset += scale;
                }

                // Fill-in the padded colums with zeros
                for (int j = _real_width; j < _padded_width; j++)
                    *frame_data_out++ = 0;

                // Skip N lines to the beginnig of the next processing segment
                block_start += width_in * scale;
            }
        }
        else
        {
            for (int j = 0; j < _real_height; j++)
            {
                uint16_t *p{};
                // Mark the beginning of each of the N lines that the filter will run upon
                for (size_t i = 0; i < pixel_raws.size(); i++)
                    pixel_raws[i] = block_start + (width_in*i);

                for (size_t i = 0, chunk_offset = 0; i < _real_width; i++)
                {
                    int sum = 0;
                    int counter = 0;

                    // extract data the kernel to process
                    for (size_t n = 0; n < scale; ++n)
                    {
                        p = pixel_raws[n] + chunk_offset;
                        for (size_t m = 0; m < scale; ++m)
                        {
                            if (*(p + m))
                            {
                                sum += p[m];
                                ++counter;
                            }
                        }
                    }

                    *frame_data_out++ = (counter == 0 ? 0 : sum / counter);
                    chunk_offset += scale;
                }

                // Fill-in the padded colums with zeros
                for (int j = _real_width; j < _padded_width; j++)
                    *frame_data_out++ = 0;

                // Skip N lines to the beginnig of the next processing segment
                block_start += width_in * scale;
            }
        }

        // Fill-in the padded rows with zeros
        for (auto v = _real_height; v < _padded_height; ++v)
        {
            for (auto u = 0; u < _padded_width; ++u)
                *frame_data_out++ = 0;
        }
    }

    void decimation_filter::decimate_others(rs2_format format, const void * frame_data_in, void * frame_data_out,
        size_t width_in, size_t height_in, size_t scale)
    {
        int sum = 0;
        auto patch_size = scale * scale;

        switch (format)
        {
        case RS2_FORMAT_YUYV:
        {
            uint8_t* from = (uint8_t*)frame_data_in;
            uint8_t* p = nullptr;
            uint8_t* q = (uint8_t*)frame_data_out;

            auto w_2 = width_in >> 1;
            auto rw_2 = _real_width >> 1;
            auto pw_2 = _padded_width >> 1;
            auto s2 = scale >> 1;
            bool odd = (scale & 1);
            for (int j = 0; j < _real_height; ++j)
            {
                for (int i = 0; i < rw_2; ++i)
                {
                    p = from + scale * (j * w_2 + i) * 4;
                    sum = 0;
                    for (size_t n = 0; n < scale; ++n)
                    {
                        for (size_t m = 0; m < scale; ++m)
                            sum += p[m * 2];

                        p += w_2 * 4;
                    }
                    *q++ = (uint8_t)(sum / patch_size);

                    p = from + scale * (j * w_2 + i) * 4 + 1;
                    sum = 0;
                    for (size_t n = 0; n < scale; ++n)
                    {
                        for (size_t m = 0; m < s2; ++m)
                            sum += 2 * p[m * 4];

                        if (odd)
                            sum += p[s2 * 4];

                        p += w_2 * 4;
                    }
                    *q++ = (uint8_t)(sum / patch_size);

                    p = from + scale * (j * w_2 + i) * 4 + s2 * 4 + (odd ? 2 : 0);
                    sum = 0;
                    for (size_t n = 0; n < scale; ++n)
                    {
                        for (size_t m = 0; m < scale; ++m)
                            sum += p[m * 2];

                        p += w_2 * 4;
                    }
                    *q++ = (uint8_t)(sum / patch_size);

                    p = from + scale * (j * w_2 + i) * 4 + 3;
                    sum = 0;
                    for (size_t n = 0; n < scale; ++n)
                    {
                        for (size_t m = 0; m < s2; ++m)
                            sum += 2 * p[m * 4];

                        if (odd)
                            sum += p[s2 * 4];

                        p += w_2 * 4;
                    }
                    *q++ = (uint8_t)(sum / patch_size);
                }

                for (int i = rw_2; i < pw_2; ++i)
                {
                    *q++ = 0;
                    *q++ = 0;
                    *q++ = 0;
                    *q++ = 0;
                }
            }

            for (int j = _real_height; j < _padded_height; ++j)
            {
                for (int i = 0; i < _padded_width; ++i)
                {
                    *q++ = 0;
                    *q++ = 0;
                }
            }
        }
        break;

        case RS2_FORMAT_UYVY:
        {
            uint8_t* from = (uint8_t*)frame_data_in;
            uint8_t* p = nullptr;
            uint8_t* q = (uint8_t*)frame_data_out;

            auto w_2 = width_in >> 1;
            auto rw_2 = _real_width >> 1;
            auto pw_2 = _padded_width >> 1;
            auto s2 = scale >> 1;
            bool odd = (scale & 1);
            for (int j = 0; j < _real_height; ++j)
            {
                for (int i = 0; i < rw_2; ++i)
                {
                    p = from + scale * (j * w_2 + i) * 4;
                    sum = 0;
                    for (size_t n = 0; n < scale; ++n)
                    {
                        for (size_t m = 0; m < s2; ++m)
                            sum += 2 * p[m * 4];

                        if (odd)
                            sum += p[s2 * 4];

                        p += w_2 * 4;
                    }
                    *q++ = (uint8_t)(sum / patch_size);

                    p = from + scale * (j * w_2 + i) * 4 + 1;
                    sum = 0;
                    for (size_t n = 0; n < scale; ++n)
                    {
                        for (size_t m = 0; m < scale; ++m)
                            sum += p[m * 2];

                        p += w_2 * 4;
                    }
                    *q++ = (uint8_t)(sum / patch_size);

                    p = from + scale * (j * w_2 + i) * 4 + 2;
                    sum = 0;
                    for (size_t n = 0; n < scale; ++n)
                    {
                        for (size_t m = 0; m < s2; ++m)
                            sum += 2 * p[m * 4];

                        if (odd)
                            sum += p[s2 * 4];

                        p += w_2 * 4;
                    }
                    *q++ = (uint8_t)(sum / patch_size);

                    p = from + scale * (j * w_2 + i) * 4 + s2 * 4 + (odd ? 3 : 1);
                    sum = 0;
                    for (size_t n = 0; n < scale; ++n)
                    {
                        for (size_t m = 0; m < scale; ++m)
                            sum += p[m * 2];

                        p += w_2 * 4;
                    }
                    *q++ = (uint8_t)(sum / patch_size);
                }

                for (int i = rw_2; i < pw_2; ++i)
                {
                    *q++ = 0;
                    *q++ = 0;
                    *q++ = 0;
                    *q++ = 0;
                }
            }

            for (int j = _real_height; j < _padded_height; ++j)
            {
                for (int i = 0; i < _padded_width; ++i)
                {
                    *q++ = 0;
                    *q++ = 0;
                }
            }
        }
        break;

        case RS2_FORMAT_RGB8:
        case RS2_FORMAT_BGR8:
        {
            uint8_t* from = (uint8_t*)frame_data_in;
            uint8_t* p = nullptr;
            uint8_t* q = (uint8_t*)frame_data_out;;

            for (int j = 0; j < _real_height; ++j)
            {
                for (int i = 0; i < _real_width; ++i)
                {
                    for (int k = 0; k < 3; ++k)
                    {
                        p = from + scale * (j * width_in + i) * 3 + k;
                        sum = 0;
                        for (size_t n = 0; n < scale; ++n)
                        {
                            for (size_t m = 0; m < scale; ++m)
                                sum += p[m * 3];

                            p += width_in * 3;
                        }

                        *q++ = (uint8_t)(sum / patch_size);
                    }
                }

                for (int i = _real_width; i < _padded_width; ++i)
                {
                    *q++ = 0;
                    *q++ = 0;
                    *q++ = 0;
                }
            }

            for (int j = _real_height; j < _padded_height; ++j)
            {
                for (int i = 0; i < _padded_width; ++i)
                {
                    *q++ = 0;
                    *q++ = 0;
                    *q++ = 0;
                }
            }
        }
        break;

        case RS2_FORMAT_RGBA8:
        case RS2_FORMAT_BGRA8:
        {
            uint8_t* from = (uint8_t*)frame_data_in;
            uint8_t* p = nullptr;
            uint8_t* q = (uint8_t*)frame_data_out;

            for (int j = 0; j < _real_height; ++j)
            {
                for (int i = 0; i < _real_width; ++i)
                {
                    for (int k = 0; k < 4; ++k)
                    {
                        p = from + scale * (j * width_in + i) * 4 + k;
                        sum = 0;
                        for (size_t n = 0; n < scale; ++n)
                        {
                            for (size_t m = 0; m < scale; ++m)
                                sum += p[m * 4];

                            p += width_in * 4;
                        }

                        *q++ = (uint8_t)(sum / patch_size);
                    }
                }

                for (int i = _real_width; i < _padded_width; ++i)
                {
                    *q++ = 0;
                    *q++ = 0;
                    *q++ = 0;
                    *q++ = 0;
                }
            }

            for (int j = _real_height; j < _padded_height; ++j)
            {
                for (int i = 0; i < _padded_width; ++i)
                {
                    *q++ = 0;
                    *q++ = 0;
                    *q++ = 0;
                    *q++ = 0;
                }
            }
        }
        break;

        case RS2_FORMAT_Y8:
        {
            uint8_t* from = (uint8_t*)frame_data_in;
            uint8_t* p = nullptr;
            uint8_t* q = (uint8_t*)frame_data_out;

            for (int j = 0; j < _real_height; ++j)
            {
                for (int i = 0; i < _real_width; ++i)
                {
                    p = from + scale * (j * width_in + i);
                    sum = 0;
                    for (size_t n = 0; n < scale; ++n)
                    {
                        for (size_t m = 0; m < scale; ++m)
                            sum += p[m];

                        p += width_in;
                    }

                    *q++ = (uint8_t)(sum / patch_size);
                }

                for (int i = _real_width; i < _padded_width; ++i)
                    *q++ = 0;
            }

            for (int j = _real_height; j < _padded_height; ++j)
            {
                for (int i = 0; i < _padded_width; ++i)
                    *q++ = 0;
            }
        }
        break;

        case RS2_FORMAT_Y16:
        {
            uint16_t* from = (uint16_t*)frame_data_in;
            uint16_t* p = nullptr;
            uint16_t* q = (uint16_t*)frame_data_out;

            for (int j = 0; j < _real_height; ++j)
            {
                for (int i = 0; i < _real_width; ++i)
                {
                    p = from + scale * (j * width_in + i);
                    sum = 0;
                    for (size_t n = 0; n < scale; ++n)
                    {
                        for (size_t m = 0; m < scale; ++m)
                            sum += p[m];

                        p += width_in;
                    }

                    *q++ = (uint16_t)(sum / patch_size);
                }

                for (int i = _real_width; i < _padded_width; ++i)
                    *q++ = 0;
            }

            for (int j = _real_height; j < _padded_height; ++j)
            {
                for (int i = 0; i < _padded_width; ++i)
                    *q++ = 0;
            }
        }
        break;

        default:
            break;
        }
    }
}

