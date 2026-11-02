// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d500-device.h"
#include "d500-options.h"
#include "stream.h"
#include <src/platform/stream-profile.h>

#include <atomic>
#include <memory>
#include <vector>


namespace librealsense
{
    class frame_interface;

    // Supports two color streams over USB endpoints (pins) of the depth interface, instead of through a dedicated sensor
    class d500_dual_color : public virtual d500_device
    {
    public:
        d500_dual_color( std::shared_ptr< const d500_info > const & dev_info );


    protected:
        std::shared_ptr< stream_interface > _color_stream_1;
        std::shared_ptr< stream_interface > _color_stream_2;
        // Endpoint bound to the pin that hosts the RGB PU. Null when the backend routes by
        // topology node instead (WMF) - RGB options then use the depth raw endpoint.
        std::shared_ptr< uvc_sensor > _raw_rgb_ep;
        // Dual RGB Passive Depth mode; null when the firmware does not publish the control.
        std::shared_ptr< passive_depth_mode_option > _passive_depth_mode;

    private:
        // Stream-combination rules for the shared imagers, registered as validators at construction.
        void frame_rates_allowed_or_throw( const stream_profiles & requests ) const;
        static void color_resolutions_allowed_or_throw( const stream_profiles & requests );

        void register_color_extrinsics();
        void register_color_metadata();
        void register_passive_depth_option();
        void register_ae_policy_option();
        void register_color_options( std::shared_ptr< const d500_info > const & dev_info );
        std::shared_ptr< uvc_sensor > pick_rgb_pu_raw_endpoint(
            std::shared_ptr< const d500_info > const & dev_info,
            const platform::processing_unit & rgb_pu );

        // Stream-id resolver: route color pins (NV12/M420/YUY2) to Color 1 / Color 2 streams, or to a single
        // multiplexed color stream (index 0) when one pin carries both sources.
        void resolve_color_stream( const std::vector< platform::stream_profile > & all,
                                   const platform::stream_profile & p, rs2_stream & type, int & index );
        // Frame resolver for the multiplexed pin: Color 1 / Color 2 from the frame's source index metadata
        bool resolve_color_frame( const frame_interface * frame, int & index );
        static bool is_color_pin( const std::vector< platform::stream_profile > & all, uint32_t pin );

        std::atomic< int > _color_pins{ 0 };
        std::atomic< bool > _warned_no_source_index{ false };
    };
}
