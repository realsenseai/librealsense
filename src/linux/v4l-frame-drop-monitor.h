// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <cstdint>  // must precede stream-profile.h, which uses uint32_t without including it
#include <src/platform/stream-profile.h>

#include <deque>
#include <utility>
#include <vector>

#include <sys/time.h>

const double DEFAULT_KPI_FRAME_DROPS_PERCENTAGE = 0.05;

namespace librealsense
{
    namespace platform
    {
        // The aim of the frame_drop_monitor is to check the frames drops kpi - which requires
        // that no more than some percentage of the frames are dropped
        // It is checked using the fps, and the previous corrupted frames, on the last 30 seconds
        // for example, for frame rate of 30 fps, and kpi of 5%, the criteria will be:
        // if at least 45 frames (= 30[fps] * 5%[kpi]* 30[sec]) drops have occured in the previous 30 seconds,
        // then the kpi is violated
        class frame_drop_monitor
        {
        public:
            frame_drop_monitor(double kpi_frames_drops_percentage) : _kpi_frames_drops_pct(kpi_frames_drops_percentage) {}
            // update_and_check_kpi method returns whether the kpi has been violated
            // it should be called each time a partial frame is caught
            bool update_and_check_kpi(const stream_profile& profile, const timeval& timestamp); 

        private:
            // container used to store the latest timestamps of the partial frames, per profile
            std::vector<std::pair<stream_profile, std::deque<long int>>> drops_per_stream;
            double _kpi_frames_drops_pct;
        };
    }  // namespace platform
}  // namespace librealsense
