// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#ifdef BUILD_WITH_MINZ

#include <librealsense2/rs.hpp>
#include "min-z-depth-improver.h"

// rs2::filter adapter for min_z_depth_improver.
// Plugs directly into the per-sensor post_processing chain so it can be
// positioned relative to temporal / spatial / hole-filling by the user.
// Upstream depth filters (temporal, spatial) never touch IR frames, so the
// frameset arriving here carries original IR alongside the already-filtered
// depth — exactly what DepthRangeImprover needs.
class minz_filter : public rs2::filter
{
    min_z_depth_improver _improver;

public:
    minz_filter()
        : rs2::filter( [this]( rs2::frame f, rs2::frame_source & src )
            {
                src.frame_ready( _improver.apply( f, src ) );
            } )
    {}
};

#endif  // BUILD_WITH_MINZ
