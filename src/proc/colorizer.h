// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.
#pragma once

#include <src/float3.h>

#include <map>
#include <vector>
#include <set>

#if defined(__AVX2__) and !defined(ANDROID)
#include <immintrin.h>
#endif    

namespace rs2
{
    class stream_profile;
}

namespace librealsense {

    class LRS_EXTENSION_API color_map
    {
    public:
        color_map(std::map<float, float3> map, int steps = 4000) : _map(map)
        {
            initialize(steps);
        }

        color_map(const std::vector<float3>& values, int steps = 4000)
        {
            for (size_t i = 0; i < values.size(); i++)
            {
                _map[(float)i / (values.size() - 1)] = values[i];
            }
            initialize(steps);
        }

        color_map() {}

        inline float3 get(float value) const
        {
            if (_max == _min) return *_data;
            auto t = (value - _min) / (_max - _min);
            t = std::max( 0.f, std::min( t, 1.f ) );
            return _data[(int)(t * (_size - 1))];
        }

#if defined (__AVX2__) and !defined(ANDROID)
	inline void get_avx2(__m256* c, __m256 value) const
        {
           __m256 zero = _mm256_set1_ps(0.f);
            __m256 one = _mm256_set1_ps(1.f);
            __m256 sizeMinOne = _mm256_sub_ps(_mm256_set1_ps(_size), one);

            __m256 min = _mm256_set1_ps( _min );
            __m256 max = _mm256_set1_ps( _max );
            __m256 t = _mm256_div_ps( _mm256_sub_ps(value, min ), _mm256_sub_ps(max,min) );
            t = _mm256_min_ps(one, _mm256_max_ps( zero, t));

            __m256i index = _mm256_cvtps_epi32(_mm256_mul_ps(  _mm256_floor_ps( _mm256_mul_ps( t, sizeMinOne)), _mm256_set1_ps(3))); // multiply each element in index by 3 because _data are float3s not floats.  There are 3 times as many of them.

            float* dataR = (float*) _data;
            float* dataG = dataR + 1;
            float* dataB = dataR + 2;

            c[0] = _mm256_i32gather_ps( dataR, index, 4); // r[8] // requires AVX2
            c[1] = _mm256_i32gather_ps( dataG, index, 4); // g[8]
            c[2] = _mm256_i32gather_ps( dataB, index, 4); // b[8] 
        }
#endif 

        float min_key() const { return _min; }
        float max_key() const { return _max; }

        const std::vector<float3>& get_cache() const { return _cache; }

    private:
        inline float3 lerp(const float3& a, const float3& b, float t) const
        {
            return b * t + a * (1 - t);
        }

        float3 calc(float value) const
        {
            if (_map.size() == 0) return{ value, value, value };
            // if we have exactly this value in the map, just return it
            if (_map.find(value) != _map.end()) return _map.at(value);
            // if we are beyond the limits, return the first/last element
            if (value < _map.begin()->first)   return _map.begin()->second;
            if (value > _map.rbegin()->first)  return _map.rbegin()->second;

            auto lower = _map.lower_bound(value) == _map.begin() ? _map.begin() : --(_map.lower_bound(value));
            auto upper = _map.upper_bound(value);

            auto t = (value - lower->first) / (upper->first - lower->first);
            auto c1 = lower->second;
            auto c2 = upper->second;
            return lerp(c1, c2, t);
        }

        void initialize(int steps)
        {
            if (_map.size() == 0) return;

            _min = _map.begin()->first;
            _max = _map.rbegin()->first;

            _cache.resize(steps + 1);
            for (int i = 0; i <= steps; i++)
            {
                auto t = (float)i / steps;
                auto x = _min + t*(_max - _min);
                _cache[i] = calc(x);
            }

            // Save size and data to avoid STL checks penalties in DEBUG
            _size = _cache.size();
            _data = _cache.data();
        }

        std::map<float, float3> _map;
        std::vector<float3> _cache;
        float _min, _max;
        size_t _size; float3* _data;
    };

    class LRS_EXTENSION_API colorizer : public stream_filter_processing_block
    {
    public:
        colorizer();

        template<typename T>
        static void update_histogram(int* hist, const T* depth_data, int w, int h)
        {
            memset(hist, 0, MAX_DEPTH * sizeof(int));
            for (auto i = 0; i < w*h; ++i)
            {
                T depth_val = depth_data[i];
                int index = static_cast< int >( depth_val );
                hist[index] += 1;
            }

            for (auto i = 2; i < MAX_DEPTH; ++i) hist[i] += hist[i - 1]; // Build a cumulative histogram for the indices in [1,0xFFFF]
        }

        static const int MAX_DEPTH = 0x10000;
        static const int MAX_DISPARITY = 0x2710;

    protected:
        colorizer(const char* name);

        bool should_process(const rs2::frame& frame) override;
        rs2::frame process_frame(const rs2::frame_source& source, const rs2::frame& f) override;

        template<typename T, typename F>
        void make_rgb_data(const T* depth_data, uint8_t* rgb_data, int width, int height, F coloring_func)
        {
            auto cm = _maps[_map_index];
            for (auto i = 0; i < width*height; ++i)
            {
                auto d = depth_data[i];
                colorize_pixel(rgb_data, i, cm, d, coloring_func);
            }
        }

#if defined (__AVX2__) and !defined(ANDROID)
        template<typename T, typename F>
	void make_rgb_data_avx2(const T* depth_data, uint8_t* rgb_data, int width, int height, F coloring_func)
        {
            auto cm = _maps[_map_index];

            for ( auto i = 0; i < width*height; i+=8) {
                colorize_pixel_avx2(rgb_data, i, cm, depth_data, coloring_func);
            }
        }
#endif

        template<typename T, typename F>
        void colorize_pixel(uint8_t* rgb_data, int idx, color_map* cm, T data, F coloring_func)
        {
            if (data)
            {
                auto f = coloring_func(data); // 0-255 based on histogram locationcolorize_pixel
                auto c = cm->get(f);
                rgb_data[idx * 3 + 0] = (uint8_t)c.x;
                rgb_data[idx * 3 + 1] = (uint8_t)c.y;
                rgb_data[idx * 3 + 2] = (uint8_t)c.z;
            }
            else
            {
                rgb_data[idx * 3 + 0] = 0;
                rgb_data[idx * 3 + 1] = 0;
                rgb_data[idx * 3 + 2] = 0;
            }
        }

#if defined (__AVX2__) and !defined(ANDROID)
	        template<typename T, typename F>
        void colorize_pixel_avx2(uint8_t* rgb_data, int idx, color_map* cm, const T* data, F coloring_func_avx)
        {
            auto f = coloring_func_avx(data+idx); // 0-255 based on histogram locationcolorize_pixel
            __m256 c256[3]; // for r,g,b values.
            cm->get_avx2(c256, f);
            __m256 depth_data = _mm256_cvtepi32_ps (_mm256_set_epi32( (int)*(data+idx+7),
                                    (int)*(data+idx+6),
                                    (int)*(data+idx+5),
                                    (int)*(data+idx+4),
                                    (int)*(data+idx+3),
                                    (int)*(data+idx+2),
                                    (int)*(data+idx+1),
                                    (int)*(data+idx+0) ) );
            __m256 zero = _mm256_set1_ps(0.0f);
            __m256 not_zero = _mm256_cmp_ps (depth_data, zero, _CMP_GT_OS);
            c256[0] = _mm256_blendv_ps( zero, c256[0], not_zero );
            c256[1] = _mm256_blendv_ps( zero, c256[1], not_zero );
            c256[2] = _mm256_blendv_ps( zero, c256[2], not_zero ); // if data[i], set rgb to calcualted value, otherwise set rgb to zero.  following behavior from colorize_pixel ( c impl )
	    __m256i r_planar = _mm256_cvtps_epi32(c256[0]);
            __m256i g_planar = _mm256_cvtps_epi32(c256[1]);
            __m256i b_planar = _mm256_cvtps_epi32(c256[2]);

            __m256i a32to8_mask = _mm256_setr_epi8(0,4,8,12,16,20,24,28,0,4,8,12,16,20,24,28,0,4,8,12,16,20,24,28,0,4,8,12,16,20,24,28); // 4 duplicates of 8 8bit pixel values coming from 32bit depth values
            __m256i rgbatorgb_mask = _mm256_setr_epi8(0,1,2,4,5,6,8,9,10,12,13,14,16,17,18,20,21,22,24,25,26,28,29,30,3,7,11,15,19,23,27,31); // moves all the a values to back, we only care about rgb.

            __m256i rg_packed = _mm256_unpacklo_epi8(_mm256_shuffle_epi8(r_planar, a32to8_mask),
                                                     _mm256_shuffle_epi8(g_planar, a32to8_mask) );
            __m256i ba_packed  = _mm256_unpacklo_epi8(_mm256_shuffle_epi8(b_planar, a32to8_mask ),
                                                      _mm256_set1_epi8(-1));
            __m256i rgba_packed = _mm256_unpacklo_epi16( rg_packed, ba_packed );
            __m256i rgb_packed = _mm256_shuffle_epi8( rgba_packed, rgbatorgb_mask );
            memcpy(rgb_data+(idx*3), &rgb_packed,24);
        }
#endif

        float _min, _max;
        bool _equalize;

        std::vector<color_map*> _maps;
        int _map_index = 0;

        std::vector<int> _histogram;
        int* _hist_data;

        int _preset = 0;
        rs2::stream_profile _target_stream_profile;
        rs2::stream_profile _source_stream_profile;

        float   _depth_units = 0.f;
        float   _d2d_convert_factor = 0.f;

        const std::set<rs2_format> _supported_formats = {RS2_FORMAT_Z16, RS2_FORMAT_DISPARITY32};
    };
}
