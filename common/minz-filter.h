// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#ifdef BUILD_WITH_MINZ

#include <librealsense2/rs.hpp>
#include <memory>
#include "min-z-depth-improver.h"

// rs2::filter adapter for min_z_depth_improver.
// Placed first in the per-sensor post_processing chain, before decimation and
// other depth filters, so that depth and IR frames arrive at the same full
// resolution.  Decimation reduces depth resolution while leaving IR unchanged,
// which would trigger the resolution-mismatch guard in apply() — running MinZ
// first avoids that.
class minz_filter : public rs2::filter
{
    std::shared_ptr< min_z_depth_improver > _improver;

    // Private delegating ctor: receives the already-constructed shared_ptr so
    // the lambda can capture it by value before the base rs2::filter is init'd.
    // This avoids capturing 'this': the lambda keeps _improver alive independently
    // of the minz_filter object's lifetime.
    explicit minz_filter( std::shared_ptr< min_z_depth_improver > imp )
        : rs2::filter( [imp]( rs2::frame f, rs2::frame_source & src )
            {
                src.frame_ready( imp->apply( f, src ) );
            } )
        , _improver( imp )
    {}

public:
    minz_filter() : minz_filter( std::make_shared< min_z_depth_improver >() ) {}
};

#endif  // BUILD_WITH_MINZ
