// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/rs.hpp>
#include <memory>
#include <vector>

#ifdef BUILD_WITH_MINZ
// Forward-declare so the rs-enhanced-depth headers stay out of this file.
namespace rs_depth {
    class DepthRangeImprover;
}
#endif

// Viewer-side adapter for rs_depth::DepthRangeImprover (rs-enhanced-depth package).
// Lazily initialises from camera calibration on the first frameset that
// contains IR left, IR right, and depth together.
// When BUILD_WITH_MINZ is not defined apply() is a no-op pass-through.
class min_z_depth_improver
{
public:
    min_z_depth_improver();
    ~min_z_depth_improver();

    // Apply MinZ improvement to the frameset in `f`.
    // IR frames are taken from `f` (upstream filters leave them unmodified);
    // the depth in `f` is the already-filtered depth from upstream.
    // Returns `f` unchanged when MinZ is unavailable or inputs are missing.
    rs2::frame apply( rs2::frame f, rs2::frame_source const & src );

private:
#ifdef BUILD_WITH_MINZ
    bool init( rs2::video_frame const & ir_left,
               rs2::video_frame const & ir_right );

    rs2::frame run( rs2::frameset            original_fs,
                    rs2::video_frame         ir_left,
                    rs2::video_frame         ir_right,
                    rs2::depth_frame         depth,
                    rs2::frame_source const & src );

    rs2::frame replace_depth( rs2::frame            filtered,
                              rs2::frame            new_depth,
                              rs2::frame_source const & src );

    std::unique_ptr< rs_depth::DepthRangeImprover > _impl;
    std::vector< uint16_t >  _out_buf;
    std::vector< uint16_t >  _depth_mm_buf;
    std::vector< rs2::frame > _replace_buf;
    int _init_width  = 0;
    int _init_height = 0;
#endif
};
