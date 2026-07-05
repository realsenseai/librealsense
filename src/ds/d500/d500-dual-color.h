// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d500-device.h"
#include "stream.h"
#include <src/platform/stream-profile.h>

#include <memory>
#include <vector>


namespace librealsense
{
    // Supports two color streams over USB endpoints (pins) of the depth interface, instead of through a dedicated sensor
    class d500_dual_color : public virtual d500_device
    {
    public:
        d500_dual_color( std::shared_ptr< const d500_info > const & dev_info );

    protected:
        std::shared_ptr< stream_interface > _color_stream_1;
        std::shared_ptr< stream_interface > _color_stream_2;

    private:
        void register_color_extrinsics();
        void register_color_metadata();

        // Stream-id resolver: route color pins (NV12/M420/YUY2) to Color 1 / Color 2 streams
        static void resolve_color_stream( const std::vector< platform::stream_profile > & all,
                                          const platform::stream_profile & p, rs2_stream & type, int & index );
        static bool is_color_pin( const std::vector< platform::stream_profile > & all, uint32_t pin );
    };
}
