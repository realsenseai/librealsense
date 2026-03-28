// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "algo.h"
#include "option.h"
#include "core/video-frame.h"
#include <algorithm>

using namespace librealsense;

bool auto_exposure_state::get_enable_auto_exposure() const
{
    return is_auto_exposure;
}

auto_exposure_modes auto_exposure_state::get_auto_exposure_mode() const
{
    return mode;
}

unsigned auto_exposure_state::get_auto_exposure_antiflicker_rate() const
{
    return rate;
}

float auto_exposure_state::get_auto_exposure_step() const
{
    return step;
}

void auto_exposure_state::set_enable_auto_exposure(bool value)
{
    is_auto_exposure = value;
}

void auto_exposure_state::set_auto_exposure_mode(auto_exposure_modes value)
{
    mode = value;
}

void auto_exposure_state::set_auto_exposure_antiflicker_rate(unsigned value)
{
    rate = value;
}

void auto_exposure_state::set_auto_exposure_step(float value)
{
    step = value;
}

auto_exposure_mechanism::auto_exposure_mechanism(option& gain_option, option& exposure_option, const auto_exposure_state& auto_exposure_state)
    : _gain_option(gain_option), _exposure_option(exposure_option),
      _auto_exposure_algo(auto_exposure_state),
      _keep_alive(true), _data_queue(queue_size), _frames_counter(0),
      _skip_frames(auto_exposure_state.skip_frames)
{
    _exposure_thread = std::make_shared<std::thread>(
        rsutils::concurrency::create_thread( rsutils::concurrency::thread_category_frame_processing, "auto-expos",
                [this]()
    {
        while (_keep_alive)
        {
            std::unique_lock<std::mutex> lk(_queue_mtx);
            _cv.wait(lk, [&] {return (_data_queue.size() || !_keep_alive); });

            if (!_keep_alive)
                return;

            frame_holder f_holder;
            auto frame_sts = _data_queue.dequeue(&f_holder, RS2_DEFAULT_TIMEOUT);

            lk.unlock();

            if (!frame_sts)
            {
                LOG_ERROR("After waiting on data_queue signalled there are no frames on queue");
                continue;
            }
            try
            {
                auto frame = std::move(f_holder);

                double values[2] = {};

                rs2_metadata_type actual_exposure_md;
                values[0] = frame->find_metadata( RS2_FRAME_METADATA_ACTUAL_EXPOSURE, &actual_exposure_md )
                              ? static_cast< double >( actual_exposure_md )
                              : _exposure_option.query();
                rs2_metadata_type gain_level_md;
                values[1] = frame->find_metadata( RS2_FRAME_METADATA_GAIN_LEVEL, &gain_level_md )
                              ? static_cast< double >( gain_level_md )
                              : _gain_option.query();

                values[0] /= 1000.; // Fisheye exposure value by extension control-
                                    // is in units of MicroSeconds, from FW version 5.6.3.0

                auto exposure_value = static_cast<float>(values[0]);
                auto gain_value = static_cast<float>(2. + (values[1] - 15.) / 8.);

                bool sts = _auto_exposure_algo.analyze_image(frame);
                if (sts)
                {
                    bool modify_exposure, modify_gain;
                    _auto_exposure_algo.modify_exposure(exposure_value, modify_exposure, gain_value, modify_gain);

                    if (modify_exposure)
                    {
                        auto value = exposure_value * 1000.f;
                        if (value < 1)
                            value = 1;

                        _exposure_option.set(value);
                    }

                    if (modify_gain)
                    {
                        auto value =  (gain_value - 2.f) * 8.f + 15.f;
                        _gain_option.set(value);
                    }
                }
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR("Error during Auto-Exposure loop: " << ex.what());
            }
            catch (...)
            {
                LOG_ERROR("Unknown error during Auto-Exposure loop!");
            }
        }
    }));
}

auto_exposure_mechanism::~auto_exposure_mechanism()
{
    {
        std::lock_guard<std::mutex> lk(_queue_mtx);
        _keep_alive = false;
    }
    _cv.notify_one();
    _exposure_thread->join();
}

void auto_exposure_mechanism::update_auto_exposure_state(const auto_exposure_state& auto_exposure_state)
{
    std::lock_guard<std::mutex> lk(_queue_mtx);
    _skip_frames = auto_exposure_state.skip_frames;
    _auto_exposure_algo.update_options(auto_exposure_state);
}

void auto_exposure_mechanism::update_auto_exposure_roi(const region_of_interest& roi)
{
    std::lock_guard<std::mutex> lk(_queue_mtx);
    _auto_exposure_algo.update_roi(roi);
}

void auto_exposure_mechanism::add_frame(frame_holder frame)
{

    if (!_keep_alive || (_skip_frames && (_frames_counter++) != _skip_frames))
    {
        return;
    }

    _frames_counter = 0;

    {
        std::lock_guard<std::mutex> lk(_queue_mtx);
        _data_queue.enqueue(std::move(frame));
    }
    _cv.notify_one();
}

auto_exposure_algorithm::auto_exposure_algorithm(const auto_exposure_state& auto_exposure_state)
    : exposure_step(ae_step_default_value)
{
    update_options(auto_exposure_state);
}

void auto_exposure_algorithm::modify_exposure(float& exposure_value, bool& exp_modified, float& gain_value, bool& gain_modified)
{
    float total_exposure = exposure * gain;
    LOG_DEBUG("TotalExposure " << total_exposure << ", target_exposure " << target_exposure);
    if (fabs(target_exposure - total_exposure) > eps)
    {
        rounding_mode_type RoundingMode;

        if (target_exposure > total_exposure)
        {
            float target_exposure0 = total_exposure * (1.0f + exposure_step);

            target_exposure0 = std::min(target_exposure0, target_exposure);
            increase_exposure_gain(target_exposure0, target_exposure0, exposure, gain);
            RoundingMode = rounding_mode_type::ceil;
            LOG_DEBUG(" ModifyExposure: IncreaseExposureGain: ");
            LOG_DEBUG(" target_exposure0 " << target_exposure0);
        }
        else
        {
            float target_exposure0 = total_exposure / (1.0f + exposure_step);

            target_exposure0 = std::max(target_exposure0, target_exposure);
            decrease_exposure_gain(target_exposure0, target_exposure0, exposure, gain);
            RoundingMode = rounding_mode_type::floor;
            LOG_DEBUG(" ModifyExposure: DecreaseExposureGain: ");
            LOG_DEBUG(" target_exposure0 " << target_exposure0);
        }
        LOG_DEBUG(" exposure " << exposure << ", gain " << gain);
        if (exposure_value != exposure)
        {
            exp_modified = true;
            exposure_value = exposure;
            exposure_value = exposure_to_value(exposure_value, RoundingMode);
            LOG_DEBUG("output exposure by algo = " << exposure_value);
        }
        if (gain_value != gain)
        {
            gain_modified = true;
            gain_value = gain;
            LOG_DEBUG("GainModified: gain = " << gain);
            gain_value = gain_to_value(gain_value, RoundingMode);
            LOG_DEBUG(" rounded to: " << gain);
        }
    }
}

bool auto_exposure_algorithm::analyze_image(const frame_interface* image)
{
    bool roi_initialized;
    region_of_interest image_roi;
    {
        std::lock_guard< std::recursive_mutex > lock( state_mutex );
        roi_initialized = is_roi_initialized;
        image_roi = roi;
    }
    auto number_of_pixels = (image_roi.max_x - image_roi.min_x + 1)*(image_roi.max_y - image_roi.min_y + 1);
    if (number_of_pixels == 0)
        return false;   // empty image

    auto frame = ((video_frame*)image);

    if (!roi_initialized)
    {
        auto width = frame->get_width();
        auto height = frame->get_height();
        image_roi.min_x = 0;
        image_roi.min_y = 0;
        image_roi.max_x = width - 1;
        image_roi.max_y = height - 1;
        number_of_pixels = width * height;
    }

    std::vector<int> H(256);
    auto total_weight = number_of_pixels;

    auto cols = frame->get_width();
    im_hist((uint8_t*)frame->get_frame_data(), image_roi, frame->get_bpp() / 8 * cols, &H[0]);

    histogram_metric score = {};
    histogram_score(H, total_weight, score);
    // int EffectiveDynamicRange = (score.highlight_limit - score.shadow_limit);
    ///
    float s1 = (score.main_mean - 128.0f) / 255.0f;
    float s2 = 0;

    s2 = (score.over_exposure_count - score.under_exposure_count) / (float)total_weight;

    float s = -0.3f * (s1 + 5.0f * s2);
    LOG_DEBUG(" AnalyzeImage Score: " << s);

    if (s > 0)
    {
        direction = +1;
        increase_exposure_target(s, target_exposure);
    }
    else
    {
        LOG_DEBUG(" AnalyzeImage: DecreaseExposure");
        direction = -1;
        decrease_exposure_target(s, target_exposure);
    }

    if (fabs(1.0f - (exposure * gain) / target_exposure) < hysteresis)
    {
        LOG_DEBUG(" AnalyzeImage: Don't Modify (Hysteresis): " << target_exposure << " " << exposure * gain);
        return false;
    }

    prev_direction = direction;
    LOG_DEBUG(" AnalyzeImage: Modify");
    return true;
}

void auto_exposure_algorithm::update_options(const auto_exposure_state& options)
{
    std::lock_guard<std::recursive_mutex> lock(state_mutex);

    state = options;
    flicker_cycle = 1000.0f / (state.get_auto_exposure_antiflicker_rate() * 2.0f);
    exposure_step = state.get_auto_exposure_step();
}

void auto_exposure_algorithm::update_roi(const region_of_interest& ae_roi)
{
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    roi = ae_roi;
    is_roi_initialized = true;
}

void auto_exposure_algorithm::im_hist(const uint8_t* data, const region_of_interest& image_roi, const int rowStep, int h[])
{
    std::lock_guard<std::recursive_mutex> lock(state_mutex);

    for (int i = 0; i < 256; ++i)
        h[i] = 0;

    const uint8_t* rowData = data + (image_roi.min_y * rowStep);
    for (int i = image_roi.min_y; i < image_roi.max_y; ++i, rowData += rowStep)
        for (int j = image_roi.min_x; j < image_roi.max_x; j+=state.sample_rate)
            ++h[rowData[j]];
}

void auto_exposure_algorithm::increase_exposure_target(float mult, float& target_exposure)
{
    target_exposure = std::min((exposure * gain) * (1.0f + mult), maximal_exposure * gain_limit);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void auto_exposure_algorithm::decrease_exposure_target(float mult, float& target_exposure)
{
    target_exposure = std::max((exposure * gain) * (1.0f + mult), minimal_exposure * base_gain);
}

void auto_exposure_algorithm::increase_exposure_gain(const float& target_exposure, const float& target_exposure0, float& exposure, float& gain)
{
    std::lock_guard<std::recursive_mutex> lock(state_mutex);

    switch ((int)(state.get_auto_exposure_mode()))
    {
    case int(auto_exposure_modes::static_auto_exposure):          static_increase_exposure_gain(target_exposure, target_exposure0, exposure, gain); break;
    case int(auto_exposure_modes::auto_exposure_anti_flicker):    anti_flicker_increase_exposure_gain(target_exposure, target_exposure0, exposure, gain); break;
    case int(auto_exposure_modes::auto_exposure_hybrid):          hybrid_increase_exposure_gain(target_exposure, target_exposure0, exposure, gain); break;
    }
}
void auto_exposure_algorithm::decrease_exposure_gain(const float& target_exposure, const float& target_exposure0, float& exposure, float& gain)
{
    std::lock_guard<std::recursive_mutex> lock(state_mutex);

    switch ((int)(state.get_auto_exposure_mode()))
    {
    case int(auto_exposure_modes::static_auto_exposure):          static_decrease_exposure_gain(target_exposure, target_exposure0, exposure, gain); break;
    case int(auto_exposure_modes::auto_exposure_anti_flicker):    anti_flicker_decrease_exposure_gain(target_exposure, target_exposure0, exposure, gain); break;
    case int(auto_exposure_modes::auto_exposure_hybrid):          hybrid_decrease_exposure_gain(target_exposure, target_exposure0, exposure, gain); break;
    }
}
void auto_exposure_algorithm::static_increase_exposure_gain(const float& /*target_exposure*/, const float& target_exposure0, float& exposure, float& gain)
{
    exposure = std::max(minimal_exposure, std::min(target_exposure0 / base_gain, maximal_exposure));
    gain = std::min(gain_limit, std::max(target_exposure0 / exposure, base_gain));
}
void auto_exposure_algorithm::static_decrease_exposure_gain(const float& /*target_exposure*/, const float& target_exposure0, float& exposure, float& gain)
{
    exposure = std::max(minimal_exposure, std::min(target_exposure0 / base_gain, maximal_exposure));
    gain = std::min(gain_limit, std::max(target_exposure0 / exposure, base_gain));
}
void auto_exposure_algorithm::anti_flicker_increase_exposure_gain(const float& target_exposure, const float& /*target_exposure0*/, float& exposure, float& gain)
{
    std::vector< std::tuple<float, float, float> > exposure_gain_score;

    for (int i = 1; i < 4; ++i)
    {
        if (i * flicker_cycle >= maximal_exposure)
        {
            continue;
        }
        float exposure1 = std::max(std::min(i * flicker_cycle, maximal_exposure), flicker_cycle);
        float gain1 = base_gain;

        if ((exposure1 * gain1) != target_exposure)
        {
            gain1 = std::min(std::max(target_exposure / exposure1, base_gain), gain_limit);
        }
        float score1 = fabs(target_exposure - exposure1 * gain1);
        exposure_gain_score.push_back(std::tuple<float, float, float>(score1, exposure1, gain1));
    }

    std::sort(exposure_gain_score.begin(), exposure_gain_score.end());

    exposure = std::get<1>(exposure_gain_score.front());
    gain = std::get<2>(exposure_gain_score.front());
}
void auto_exposure_algorithm::anti_flicker_decrease_exposure_gain(const float& target_exposure, const float& /*target_exposure0*/, float& exposure, float& gain)
{
    std::vector< std::tuple<float, float, float> > exposure_gain_score;

    for (int i = 1; i < 4; ++i)
    {
        if (i * flicker_cycle >= maximal_exposure)
        {
            continue;
        }
        float exposure1 = std::max(std::min(i * flicker_cycle, maximal_exposure), flicker_cycle);
        float gain1 = base_gain;
        if ((exposure1 * gain1) != target_exposure)
        {
            gain1 = std::min(std::max(target_exposure / exposure1, base_gain), gain_limit);
        }
        float score1 = fabs(target_exposure - exposure1 * gain1);
        exposure_gain_score.push_back(std::tuple<float, float, float>(score1, exposure1, gain1));
    }

    std::sort(exposure_gain_score.begin(), exposure_gain_score.end());

    exposure = std::get<1>(exposure_gain_score.front());
    gain = std::get<2>(exposure_gain_score.front());
}
void auto_exposure_algorithm::hybrid_increase_exposure_gain(const float& target_exposure, const float& target_exposure0, float& exposure, float& gain)
{
    if (anti_flicker_mode)
    {
        anti_flicker_increase_exposure_gain(target_exposure, target_exposure0, exposure, gain);
    }
    else
    {
        static_increase_exposure_gain(target_exposure, target_exposure0, exposure, gain);
        LOG_DEBUG("HybridAutoExposure::IncreaseExposureGain: " << exposure * gain << " " << flicker_cycle * base_gain << " " << base_gain);
        if (target_exposure > 0.99 * flicker_cycle * base_gain)
        {
            anti_flicker_mode = true;
            anti_flicker_increase_exposure_gain(target_exposure, target_exposure0, exposure, gain);
            LOG_DEBUG("anti_flicker_mode = true");
        }
    }
}
void auto_exposure_algorithm::hybrid_decrease_exposure_gain(const float& target_exposure, const float& target_exposure0, float& exposure, float& gain)
{
    if (anti_flicker_mode)
    {
        LOG_DEBUG("HybridAutoExposure::DecreaseExposureGain: " << exposure << " " << flicker_cycle << " " << gain << " " << base_gain);
        if ((target_exposure) <= 0.99 * (flicker_cycle * base_gain))
        {
            anti_flicker_mode = false;
            static_decrease_exposure_gain(target_exposure, target_exposure0, exposure, gain);
            LOG_DEBUG("anti_flicker_mode = false");
        }
        else
        {
            anti_flicker_decrease_exposure_gain(target_exposure, target_exposure0, exposure, gain);
        }
    }
    else
    {
        static_decrease_exposure_gain(target_exposure, target_exposure0, exposure, gain);
    }
}

float auto_exposure_algorithm::exposure_to_value(float exp_ms, rounding_mode_type rounding_mode)
{
    const float line_period_us = 19.33333333f;

    float ExposureTimeLine = (exp_ms * 1000.0f / line_period_us);
    if (rounding_mode == rounding_mode_type::ceil) ExposureTimeLine = std::ceil(ExposureTimeLine);
    else if (rounding_mode == rounding_mode_type::floor) ExposureTimeLine = std::floor(ExposureTimeLine);
    else ExposureTimeLine = round(ExposureTimeLine);
    return ((float)ExposureTimeLine * line_period_us) / 1000.0f;
}

float auto_exposure_algorithm::gain_to_value(float gain, rounding_mode_type rounding_mode)
{

    if (gain < base_gain) { return base_gain; }
    else if (gain > 16.0f) { return 16.0f; }
    else {
        if (rounding_mode == rounding_mode_type::ceil) return std::ceil(gain * 8.0f) / 8.0f;
        else if (rounding_mode == rounding_mode_type::floor) return std::floor(gain * 8.0f) / 8.0f;
        else return round(gain * 8.0f) / 8.0f;
    }
}

template <typename T> inline T sqr(const T& x) { return (x*x); }
void auto_exposure_algorithm::histogram_score(std::vector<int>& h, const int total_weight, histogram_metric& score)
{
    score.under_exposure_count = 0;
    score.over_exposure_count = 0;

    for (size_t i = 0; i <= under_exposure_limit; ++i)
    {
        score.under_exposure_count += h[i];
    }
    score.shadow_limit = 0;
    //if (Score.UnderExposureCount < UnderExposureNoiseLimit)
    {
        score.shadow_limit = under_exposure_limit;
        for (size_t i = under_exposure_limit + 1; i <= over_exposure_limit; ++i)
        {
            if (h[i] > under_exposure_noise_limit)
            {
                break;
            }
            score.shadow_limit++;
        }
        int lower_q = 0;
        score.lower_q = 0;
        for (size_t i = under_exposure_limit + 1; i <= over_exposure_limit; ++i)
        {
            lower_q += h[i];
            if (lower_q > total_weight / 4)
            {
                break;
            }
            score.lower_q++;
        }
    }

    for (size_t i = over_exposure_limit; i <= 255; ++i)
    {
        score.over_exposure_count += h[i];
    }

    score.highlight_limit = 255;
    //if (Score.OverExposureCount < OverExposureNoiseLimit)
    {
        score.highlight_limit = over_exposure_limit;
        for (size_t i = over_exposure_limit; i >= under_exposure_limit; --i)
        {
            if (h[i] > over_exposure_noise_limit)
            {
                break;
            }
            score.highlight_limit--;
        }
        int upper_q = 0;
        score.upper_q = over_exposure_limit;
        for (size_t i = over_exposure_limit; i >= under_exposure_limit; --i)
        {
            upper_q += h[i];
            if (upper_q > total_weight / 4)
            {
                break;
            }
            score.upper_q--;
        }

    }
    int32_t m1 = 0;
    int64_t m2 = 0;

    double nn = (double)total_weight - score.under_exposure_count - score.over_exposure_count;
    if (nn == 0)
    {
        nn = (double)total_weight;
        for (int i = 0; i <= 255; ++i)
        {
            m1 += h[i] * i;
            m2 += static_cast< int64_t >( h[i] ) * sqr( i );
        }
    }
    else
    {
        for (int i = under_exposure_limit + 1; i < over_exposure_limit; ++i)
        {
            m1 += h[i] * i;
            m2 += static_cast< int64_t >( h[i] ) * sqr( i );
        }
    }
    score.main_mean = (float)((double)m1 / nn);
    double Var = (double)m2 / nn - sqr((double)m1 / nn);
    if (Var > 0)
    {
        score.main_std = (float)sqrt(Var);
    }
    else
    {
        score.main_std = 0.0f;
    }
}

rect_gaussian_dots_target_calculator::rect_gaussian_dots_target_calculator(int width, int height, int roi_start_x, int roi_start_y, int roi_width, int roi_height)
    : _full_width(width), _full_height(height), _roi_start_x(roi_start_x), _roi_start_y(roi_start_y), _width(roi_width), _height(roi_height)
{
    _wt = _width - _tsize;
    _ht = _height - _tsize;
    _size = _width * _height;

    _hwidth = _width >> 1;
    _hheight = _height >> 1;

    _imgt.resize(_tsize2);
    _img.resize(_size);
    _ncc.resize(_size);
    memset(_ncc.data(), 0, _size * sizeof(double));

    _buf.resize(_patch_size);
}

rect_gaussian_dots_target_calculator::~rect_gaussian_dots_target_calculator()
{
}

bool rect_gaussian_dots_target_calculator::calculate(const uint8_t* img, float* target_dims, unsigned int target_dims_size)
{
    bool ret = false;
    if (target_dims_size < 4)
        return ret;

    normalize(img);
    calculate_ncc();

    if (find_corners())
        ret = validate_corners(img);

    if (ret)
    {
        if (target_dims_size == 4)
            calculate_rect_sides(target_dims);
        else if (target_dims_size == 8)
        {
            int j = 0;
            for (int i = 0; i < 4; ++i)
            {
                target_dims[j++] = static_cast<float>(_corners[i].x + _roi_start_x);
                target_dims[j++] = static_cast<float>(_corners[i].y + _roi_start_y);
            }
        }
    }

    return ret;
}

void rect_gaussian_dots_target_calculator::normalize(const uint8_t* img)
{
    uint8_t min_val = 255;
    uint8_t max_val = 0;

    int jumper = _full_width - _width;
    const uint8_t* p = img + _roi_start_y * _full_width + _roi_start_x;
    for (int j = 0; j < _height; ++j)
    {
        for (int i = 0; i < _width; ++i)
        {
            if (*p < min_val)
                min_val = *p;

            if (*p > max_val)
                max_val = *p;

            ++p;
        }

        p += jumper;
    }

    if (max_val > min_val)
    {
        double factor = 1.0 / (max_val - min_val);

        p = img;
        double* q = _img.data();
        p = img + _roi_start_y * _full_width + _roi_start_x;
        for (int j = 0; j < _height; ++j)
        {
            for (int i = 0; i < _width; ++i)
                *q++ = 1.0f - (*p++ - min_val) * factor;

            p += jumper;
        }
    }
}

void rect_gaussian_dots_target_calculator::calculate_ncc()
{
    double* pncc = _ncc.data() + (_htsize * _width + _htsize);
    double* pi = _img.data();
    double* pit = _imgt.data();

    const double* pt = nullptr;
    const double* qi = nullptr;

    double sum = 0.0;
    double mean = 0.0;
    double norm = 0.0;

    double min_val = 2.0;
    double max_val = -2.0;
    double tmp = 0.0;

    for (int j = 0; j < _ht; ++j)
    {
        for (int i = 0; i < _wt; ++i)
        {
            qi = pi;
            sum = 0.0f;
            for (int m = 0; m < _tsize; ++m)
            {
                for (int n = 0; n < _tsize; ++n)
                    sum += *qi++;

                qi += _wt;
            }

            mean = sum / _tsize2;

            qi = pi;
            sum = 0.0f;
            pit = _imgt.data();
            for (int m = 0; m < _tsize; ++m)
            {
                for (int n = 0; n < _tsize; ++n)
                {
                    *pit = *qi++ - mean;
                    sum += *pit * *pit;
                    ++pit;
                }
                qi += _wt;
            }

            norm = sqrt(sum);

            pt = _template.data();
            pit = _imgt.data();
            sum = 0.0;
            for (int k = 0; k < _tsize2; ++k)
                sum += *pit++ * *pt++;

            tmp = sum / norm;
            if (tmp < min_val)
                min_val = tmp;

            if (tmp > max_val)
                max_val = tmp;

            *pncc++ = tmp;
            ++pi;
        }

        pncc += _tsize;
        pi += _tsize;
    }

    if (max_val > min_val)
    {
        double factor = 1.0 / (max_val - min_val);
        double div = 1.0 - _thresh;
        pncc = _ncc.data();
        for (int i = 0; i < _size; ++i)
        {
            tmp = (*pncc - min_val) * factor;
            *pncc++ = (tmp < _thresh ? 0 : (tmp - _thresh) / div);
        }
    }
}

bool rect_gaussian_dots_target_calculator::find_corners()
{
    static const int edge = 20;

    // upper left
    _pts[0].x = 0;
    _pts[0].y = 0;
    double peak = 0.0f;
    double* p = _ncc.data() + _htsize * _width;
    for (int j = _htsize; j < _hheight; ++j)
    {
        p += _htsize;
        for (int i = _htsize; i < _hwidth; ++i)
        {
            if (*p > peak)
            {
                peak = *p;
                _pts[0].x = i;
                _pts[0].y = j;
            }
            ++p;
        }
        p += _hwidth;
    }

    if (peak < _thresh || _pts[0].x < edge || _pts[0].y < edge)
        return false;

    // upper right
    _pts[1].x = 0;
    _pts[1].y = 0;
    peak = 0.0f;
    p = _ncc.data() + (_htsize * _width);
    for (int j = _htsize; j < _hheight; ++j)
    {
        p += _hwidth;
        for (int i = _hwidth; i < _width - _htsize; ++i)
        {
            if (*p > peak)
            {
                peak = *p;
                _pts[1].x = i;
                _pts[1].y = j;
            }
            ++p;
        }
        p += _htsize;
    }

    if (peak < _thresh || _pts[1].x + edge > _width || _pts[1].y < edge || _pts[1].x - _pts[0].x < edge)
        return false;

    // lower left
    _pts[2].x = 0;
    _pts[2].y = 0;
    peak = 0.0f;
    p = _ncc.data() + (_hheight * _width);
    for (int j = _hheight; j < _height - _htsize; ++j)
    {
        p += _htsize;
        for (int i = _htsize; i < _hwidth; ++i)
        {
            if (*p > peak)
            {
                peak = *p;
                _pts[2].x = i;
                _pts[2].y = j;
            }
            ++p;
        }
        p += _hwidth;
    }

    if (peak < _thresh || _pts[2].x < edge || _pts[2].y + edge > _height || _pts[2].y - _pts[1].y < edge)
        return false;

    // lower right
    _pts[3].x = 0;
    _pts[3].y = 0;
    peak = 0.0f;
    p = _ncc.data() + (_hheight * _width);
    for (int j = _hheight; j < _height - _htsize; ++j)
    {
        p += _hwidth;
        for (int i = _hwidth; i < _width - _htsize; ++i)
        {
            if (*p > peak)
            {
                peak = *p;
                _pts[3].x = i;
                _pts[3].y = j;
            }
            ++p;
        }
        p += _htsize;
    }

    if (peak < _thresh || _pts[3].x + edge > _width || _pts[3].y + edge > _height || _pts[3].x - _pts[2].x < edge || _pts[3].y - _pts[1].y < edge)
        return false;
    else
        refine_corners();

    return true;
}

void rect_gaussian_dots_target_calculator::refine_corners()
{
    double* f = _buf.data();
    int hs = _patch_size >> 1;

    // upper left
    int pos = (_pts[0].y - hs) * _width + _pts[0].x - hs;

    _corners[0].x = static_cast<double>(_pts[0].x) - hs;
    minimize_x(_ncc.data() + pos, _patch_size, f, _corners[0].x);

    _corners[0].y = static_cast<double>(_pts[0].y) - hs;
    minimize_y(_ncc.data() + pos, _patch_size, f, _corners[0].y);

    // upper right
    pos = (_pts[1].y - hs) * _width + _pts[1].x - hs;

    _corners[1].x = static_cast<double>(_pts[1].x) - hs;
    minimize_x(_ncc.data() + pos, _patch_size, f, _corners[1].x);

    _corners[1].y = static_cast<double>(_pts[1].y) - hs;
    minimize_y(_ncc.data() + pos, _patch_size, f, _corners[1].y);

    // lower left
    pos = (_pts[2].y - hs) * _width + _pts[2].x - hs;

    _corners[2].x = static_cast<double>(_pts[2].x) - hs;
    minimize_x(_ncc.data() + pos, _patch_size, f, _corners[2].x);

    _corners[2].y = static_cast<double>(_pts[2].y) - hs;
    minimize_y(_ncc.data() + pos, _patch_size, f, _corners[2].y);

    // lower right
    pos = (_pts[3].y - hs) * _width + _pts[3].x - hs;

    _corners[3].x = static_cast<double>(_pts[3].x) - hs;
    minimize_x(_ncc.data() + pos, _patch_size, f, _corners[3].x);

    _corners[3].y = static_cast<double>(_pts[3].y) - hs;
    minimize_y(_ncc.data() + pos, _patch_size, f, _corners[3].y);
}

bool rect_gaussian_dots_target_calculator::validate_corners(const uint8_t* img)
{
    bool ok = true;

    static const int pos_diff_threshold = 4;
    if( std::abs( _corners[0].x - _corners[2].x ) > pos_diff_threshold
        || std::abs( _corners[1].x - _corners[3].x ) > pos_diff_threshold
        || std::abs( _corners[0].y - _corners[1].y ) > pos_diff_threshold
        || std::abs( _corners[2].y - _corners[3].y ) > pos_diff_threshold )
    {
        ok = false;
    }

    return ok;
}

void rect_gaussian_dots_target_calculator::calculate_rect_sides(float* rect_sides)
{
    double lx = _corners[1].x - _corners[0].x;
    double ly = _corners[1].y - _corners[0].y;
    rect_sides[0] = static_cast<float>(sqrt(lx * lx + ly * ly)); // uppper

    lx = _corners[3].x - _corners[2].x;
    ly = _corners[3].y - _corners[2].y;
    rect_sides[1] = static_cast<float>(sqrt(lx * lx + ly * ly)); // lower

    lx = _corners[2].x - _corners[0].x;
    ly = _corners[2].y - _corners[0].y;
    rect_sides[2] = static_cast<float>(sqrt(lx * lx + ly * ly)); // left

    lx = _corners[3].x - _corners[1].x;
    ly = _corners[3].y - _corners[1].y;
    rect_sides[3] = static_cast<float>(sqrt(lx * lx + ly * ly)); // right
}

void rect_gaussian_dots_target_calculator::minimize_x(const double* p, int s, double* f, double& x)
{
    int ws = _width - s;

    for (int i = 0; i < s; ++i)
        f[i] = 0;

    for (int j = 0; j < s; ++j)
    {
        for (int i = 0; i < s; ++i)
            f[i] += *p++;
        p += ws;
    }

    x += subpixel_agj(f, s);
}

void rect_gaussian_dots_target_calculator::minimize_y(const double* p, int s, double* f, double& y)
{
    int ws = _width - s;

    for (int i = 0; i < s; ++i)
        f[i] = 0;

    for (int j = 0; j < s; ++j)
    {
        for (int i = 0; i < s; ++i)
            f[j] += *p++;
        p += ws;
    }

    y += subpixel_agj(f, s);
}

double rect_gaussian_dots_target_calculator::subpixel_agj(double* f, int s)
{
    int mi = 0;
    double mv = f[mi];
    for (int i = 1; i < s; ++i)
    {
        if (f[i] > mv)
        {
            mi = i;
            mv = f[mi];
        }
    }

    double half_mv = 0.5f * mv;

    int x_0 = 0;
    int x_1 = 0;
    for (int i = 0; i < s; ++i)
    {
        if (f[i] > half_mv)
        {
            x_1 = i;
            break;
        }
    }

    double left_mv = 0.0f;
    if (x_1 > 0)
    {
        x_0 = x_1 - 1;
        left_mv = x_0 + (half_mv - f[x_0]) / (f[x_1] - f[x_0]);
    }
    else
        left_mv = static_cast<double>(0);

    x_0 = s - 1;
    for (int i = s - 1; i >= 0; --i)
    {
        if (f[i] > half_mv)
        {
            x_0 = i;
            break;
        }
    }

    double right_mv = 0.0f;
    if (x_0 == s - 1)
        right_mv = static_cast<double>(s) - 1;
    else
    {
        x_1 = x_0 + 1;
        right_mv = x_0 + (half_mv - f[x_0]) / (f[x_1] - f[x_0]);
    }

    return (left_mv + right_mv) / 2;
}

// return 1 if target pattern is found and its location is calculated, zero otherwise.
int rect_calculator::extract_target_dims(const rs2_frame* frame_ref, float4& rect_sides)
{
    int ret = 0;
    rs2_error* e = nullptr;

    //Target dimension array size is predefined. 4 for RS2_CALIB_TARGET_RECT_GAUSSIAN_DOT_VERTICES and 8 for RS2_CALIB_TARGET_POS_GAUSSIAN_DOT_VERTICES.
    int dim_size = _roi ? 4 : 8;
    rs2_calib_target_type tgt_type = _roi ? RS2_CALIB_TARGET_ROI_RECT_GAUSSIAN_DOT_VERTICES : RS2_CALIB_TARGET_RECT_GAUSSIAN_DOT_VERTICES;
    rs2_extract_target_dimensions(frame_ref, tgt_type, reinterpret_cast<float*>(&rect_sides), dim_size, &e);
    if (e == nullptr)
    {
        ret = 1;
    }

    return ret;
}
