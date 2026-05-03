// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "min-z-depth-improver.h"

#ifdef BUILD_WITH_MINZ
#include <rs_depth_calibration.hpp>
#include <rs_depth_range.hpp>
#include <cstring>
#include <cmath>
#endif

min_z_depth_improver::min_z_depth_improver()  = default;
min_z_depth_improver::~min_z_depth_improver() = default;

rs2::frame min_z_depth_improver::apply( rs2::frame f, rs2::frame_source const & src )
{
#ifdef BUILD_WITH_MINZ
    auto fs = f.as< rs2::frameset >();
    if( ! fs )
        return f;

    rs2::video_frame ir_left  = fs.get_infrared_frame( 1 );
    rs2::video_frame ir_right = fs.get_infrared_frame( 2 );
    if( ! ir_left || ! ir_right )
        return f;

    auto depth = fs.first_or_default( RS2_STREAM_DEPTH ).as< rs2::depth_frame >();
    if( ! depth )
        return f;

    int w = ir_left.get_width();
    int h = ir_left.get_height();
    if( ! _impl || w != _init_width || h != _init_height )
        if( ! init( ir_left, ir_right ) )
            return f;

    auto new_depth = run( fs, ir_left, ir_right, depth, src );
    if( ! new_depth )
        return f;

    return replace_depth( f, new_depth, src );
#else
    return f;
#endif
}

#ifdef BUILD_WITH_MINZ

bool min_z_depth_improver::init( rs2::video_frame const & ir_left,
                                 rs2::video_frame const & ir_right )
{
    auto ir1_prof = ir_left.get_profile().as< rs2::video_stream_profile >();
    auto ir2_prof = ir_right.get_profile().as< rs2::video_stream_profile >();
    auto intrin   = ir1_prof.get_intrinsics();
    auto extrin   = ir1_prof.get_extrinsics_to( ir2_prof );

    auto cal = rs_depth::Calibration::from_sdk( intrin, extrin );

    try
    {
        _impl = std::make_unique< rs_depth::DepthRangeImprover >( cal );
    }
    catch( std::exception const & )
    {
        _impl.reset();
        return false;
    }

    _init_width  = intrin.width;
    _init_height = intrin.height;
    return true;
}

rs2::frame min_z_depth_improver::run( rs2::frameset            original_fs,
                                      rs2::video_frame         ir_left,
                                      rs2::video_frame         ir_right,
                                      rs2::depth_frame         depth,
                                      rs2::frame_source const & src )
{
    int   w        = depth.get_width();
    int   h        = depth.get_height();
    float units    = depth.get_units();
    float scale_mm = units * 1000.f;
    bool  is_1mm   = std::abs( scale_mm - 1.f ) < 1e-3f;

    auto const * raw = reinterpret_cast< const uint16_t * >( depth.get_data() );

    // DepthRangeImprover expects depth in mm (1 unit = 1 mm).
    // For the standard D4xx depth_units=0.001 case raw values are already in mm —
    // pass the frame buffer directly.  Only convert (and reuse the member scratch
    // buffer to avoid per-frame allocation) when depth_units is non-standard.
    const uint16_t * depth_input = raw;
    if( ! is_1mm )
    {
        _depth_mm_buf.resize( w * h );
        for( int i = 0; i < w * h; ++i )
            _depth_mm_buf[i] = static_cast< uint16_t >( raw[i] * scale_mm );
        depth_input = _depth_mm_buf.data();
    }

    _out_buf.resize( w * h );

    auto meta = FrameMetadata::from_rs2_frameset( original_fs );

    _impl->process(
        reinterpret_cast< const uint8_t * >( ir_left.get_data() ),
        reinterpret_cast< const uint8_t * >( ir_right.get_data() ),
        depth_input,
        _out_buf.data(),
        meta );

    auto new_frame = src.allocate_video_frame(
        depth.get_profile(), depth, 0, w, h, 0, RS2_EXTENSION_DEPTH_FRAME );
    if( ! new_frame )
        return {};

    auto * dst = reinterpret_cast< uint16_t * >( const_cast< void * >( new_frame.get_data() ) );
    if( is_1mm )
        std::memcpy( dst, _out_buf.data(), w * h * sizeof( uint16_t ) );
    else
    {
        float inv = 1.f / scale_mm;
        for( int i = 0; i < w * h; ++i )
            dst[i] = static_cast< uint16_t >( _out_buf[i] * inv );
    }

    return new_frame;
}

rs2::frame min_z_depth_improver::replace_depth( rs2::frame            filtered,
                                                 rs2::frame            new_depth,
                                                 rs2::frame_source const & src )
{
    auto fs = filtered.as< rs2::frameset >();
    if( ! fs )
        return new_depth;

    _replace_buf.clear();
    for( auto && fr : fs )
        _replace_buf.push_back(
            fr.get_profile().stream_type() == RS2_STREAM_DEPTH ? new_depth : fr );

    return src.allocate_composite_frame( _replace_buf );
}

#endif
