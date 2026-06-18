// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/rs.hpp>
#include <memory>
#include <vector>

#ifdef BUILD_WITH_CLOSE_RANGE_DEPTH
// Forward-declare — full definition lives in rs-depth-range-loader.h (included by the .cpp).
class rs_depth_range_impl;
#endif

// Viewer-side adapter for the librealsense2-enhanced-depth (Improved Close Range Depth) library.
// Loads librs_depth_range.so at runtime via dlopen (see rs-depth-range-loader.h);
// lazily initialises from camera calibration on the first frameset that
// contains IR left, IR right, and depth together.
// When BUILD_WITH_CLOSE_RANGE_DEPTH is not defined, or when the library is absent at runtime,
// apply() is a no-op pass-through.
//
// Threading: apply() must be called from a single thread (the viewer render loop).
// The scratch buffers (_depth_mm_buf, _replace_buf) are not protected by a mutex;
// concurrent calls would race on them.
class close_range_depth_improver
{
public:
    close_range_depth_improver();
    ~close_range_depth_improver();

    // Apply close-range depth improvement to the frameset in `f`.
    // Runs before decimation so depth and IR are at matching full resolution.
    // Returns `f` unchanged when the library is unavailable or inputs are missing.
    rs2::frame apply( rs2::frame f, rs2::frame_source const & src );

private:
#ifdef BUILD_WITH_CLOSE_RANGE_DEPTH
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

    std::unique_ptr< rs_depth_range_impl > _impl;
    std::vector< uint16_t >  _depth_mm_buf;
    std::vector< rs2::frame > _replace_buf;
    int  _init_width    = 0;
    int  _init_height   = 0;
    bool _library_absent = false;  // set on first failed init(); stops per-frame retry
#endif
};
