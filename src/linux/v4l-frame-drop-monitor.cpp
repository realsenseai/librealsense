// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "v4l-frame-drop-monitor.h"

#include <algorithm>

namespace librealsense
{
    namespace platform
    {
        bool frame_drop_monitor::update_and_check_kpi(const stream_profile& profile, const timeval& timestamp)
        {
            bool is_kpi_violated = false;
            long int timestamp_usec = static_cast<long int> (timestamp.tv_sec * 1000000 + timestamp.tv_usec);

            // checking if the current profile is already in the drops_per_stream container
            auto it = std::find_if(drops_per_stream.begin(), drops_per_stream.end(), 
                [profile](std::pair<stream_profile, std::deque<long int>>& sp_deq)
                {return  profile == sp_deq.first; });

            // if the profile is already in the drops_per_stream container, 
            // checking kpi with the new partial frame caught
            if (it != drops_per_stream.end())
            {
                // setting the kpi checking to be done on the last 30 seconds
                int time_limit = 30;
                
                // max number of drops that can be received in the time_limit, without violation of the kpi
                int max_num_of_drops = profile.fps * _kpi_frames_drops_pct * time_limit;

                auto& queue_for_profile = it->second;
                // removing too old timestamps of partial frames
                while (queue_for_profile.size() > 0)
                {
                    auto delta_ts_usec = timestamp_usec - queue_for_profile.front();
                    if (delta_ts_usec > (time_limit * 1000000))
                    {
                        queue_for_profile.pop_front();
                    }
                    else
                        break; // correct because the frames are added chronologically
                }
                // checking kpi violation
                if (queue_for_profile.size() >= max_num_of_drops)
                {
                    is_kpi_violated = true;
                    queue_for_profile.clear();
                }
                else
                    queue_for_profile.push_back(timestamp_usec);
            }
            else
            {
                // adding the the current partial frame's profile and timestamp to the container
                std::deque<long int> deque_to_add;
                deque_to_add.push_back(timestamp_usec);
                drops_per_stream.push_back(std::make_pair(profile, deque_to_add));
            }
            return is_kpi_violated;
        }
    }  // namespace platform
}  // namespace librealsense
