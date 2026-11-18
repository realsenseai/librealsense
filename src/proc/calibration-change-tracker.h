// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include <librealsense2/hpp/rs_frame.hpp>

#include <map>


namespace librealsense
{
    class calibration_change_device;

    // Lets a processing block drop calibration it caches per stream profile when the device reports a change.
    // The device keeps the stream profile objects across such changes, so profile identity alone cannot tell.
    class calibration_change_tracker
    {
    public:
        // True when the frame's device changed calibration since the previous frame from that device
        bool changed( const rs2::frame & f );

    private:
        // Per device, so frames alternating between devices still see each one's changes. Keys are identity only,
        // never dereferenced: a new device reusing a freed one's address costs at most one extra re-read.
        std::map< calibration_change_device const *, unsigned > _counts;
    };
}
