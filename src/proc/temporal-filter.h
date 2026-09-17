// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once
#include "types.h"

#include <map>
#include <utility>

namespace librealsense
{
    const size_t PRESISTENCY_LUT_SIZE = 256;

    class temporal_filter : public depth_processing_block
    {
    public:
        temporal_filter();

    protected:
        void    update_configuration(const rs2::frame& f);
        rs2::frame process_frame(const rs2::frame_source& source, const rs2::frame& f) override;

        rs2::frame prepare_target_frame(const rs2::frame& f, const rs2::frame_source& source);

        template<typename T>
        void temp_jw_smooth(void* frame_data, void * _last_frame_data, uint8_t *history, uint8_t & cur_frame_index)
        {
            static_assert((std::is_arithmetic<T>::value), "temporal filter assumes numeric types");

            const bool fp = (std::is_floating_point<T>::value);

            T delta_z = static_cast<T>(_delta_param);

            auto frame          = reinterpret_cast<T*>(frame_data);
            auto _last_frame    = reinterpret_cast<T*>(_last_frame_data);

            unsigned char mask = 1 << cur_frame_index;

            // Copy locally, to remove need for a lock.
            float alpha = _alpha_param;
            float one_minus_alpha = 1.f - alpha;
            // pass one -- go through image and update all
            for (size_t i = 0; i < _current_frm_size_pixels; i++)
            {
                T cur_val = frame[i];
                T prev_val = _last_frame[i];

                if (cur_val)
                {
                    if (!prev_val)
                    {
                        _last_frame[i] = cur_val;
                        history[i] = mask;
                    }
                    else
                    {  // old and new val
                        T diff = static_cast<T>(fabs(cur_val - prev_val));

                        if (diff < delta_z)
                        {  // old and new val agree
                            history[i] |= mask;
                            float filtered = alpha * cur_val + one_minus_alpha * prev_val;
                            T result = static_cast<T>(filtered);
                            frame[i] = result;
                            _last_frame[i] = result;
                        }
                        else
                        {
                            _last_frame[i] = cur_val;
                            history[i] = mask;
                        }
                    }
                }
                else
                {  // no cur_val
                    if (prev_val)
                    { // only case we can help
                        unsigned char hist = history[i];
                        unsigned char classification = _persistence_map[hist];
                        if (classification & mask)
                        { // we have had enough samples lately
                            frame[i] = prev_val;
                        }
                    }
                    history[i] &= ~mask;
                }
            }

            cur_frame_index = (cur_frame_index + 1) % 8;  // at end of cycle
        }

    private:
        // What the filter carries from one frame to the next. A sensor can send more than one depth
        // stream through the same filter - raw depth next to device-aligned depth - and a single
        // history would mix them, so each stream keeps its own.
        struct stream_state
        {
            std::vector< uint8_t > last_frame;  // last frame received for this stream
            std::vector< uint8_t > history;     // the last 8 frames, 1 bit per frame
            uint8_t cur_frame_index = 0;
        };

        void reset_history();
        // The history that belongs to the frame's own stream, sized for it
        stream_state & state_of( const rs2::frame & f );

        void on_set_persistence_control(uint8_t val);
        void on_set_alpha(float val);
        void on_set_delta(float val);

        void recalc_persistence_map();
        uint8_t                 _persistence_param;

        float                   _alpha_param;               // The normalized weight of the current pixel
        uint8_t                 _delta_param;               // A threshold when a filter is invoked
        size_t                  _width, _height, _stride;
        size_t                  _bpp;
        rs2_extension           _extension_type;            // Strictly Depth/Disparity
        size_t                  _current_frm_size_pixels;
        rs2::stream_profile     _source_stream_profile;
        rs2::stream_profile     _target_stream_profile;
        // Keyed by stream type and index, which stay the same down a processing chain - unlike the
        // profile, which every block re-clones
        std::map< std::pair< rs2_stream, int >, stream_state > _states;
        // encodes whether a particular 8 bit history is good enough for all 8 phases of storage
        std::array<uint8_t, PRESISTENCY_LUT_SIZE> _persistence_map;
    };
    MAP_EXTENSION(RS2_EXTENSION_TEMPORAL_FILTER, librealsense::temporal_filter);
}
