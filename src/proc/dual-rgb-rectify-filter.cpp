// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "dual-rgb-rectify-filter.h"
#include <librealsense2/hpp/rs_processing.hpp>
#include <algorithm>
#include <cstring>

#ifdef RS2_USE_CUDA
#include "cuda/cuda-rggb.cuh"
#include "rsutils/accelerators/gpu.h"   // rsutils::rs2_is_gpu_available
#endif

namespace librealsense {

dual_rgb_rectify_filter::dual_rgb_rectify_filter()
    : stream_filter_processing_block( "RGB Rectification" )
{
    // Only act on color streams; everything else passes through.
    _stream_filter.stream = RS2_STREAM_COLOR;
    _stream_filter.format = RS2_FORMAT_RGB8;
}

dual_rgb_rectify_filter::~dual_rgb_rectify_filter()
{
    // Lock so free_device_maps() can't race an in-flight process_frame() on the other pin's thread.
    std::lock_guard< std::mutex > lock( _mutex );
    free_device_maps();
}

void dual_rgb_rectify_filter::free_device_maps()
{
#ifdef RS2_USE_CUDA
    for( int i = 0; i < 2; ++i )
    {
        rscuda::rggb_cuda_free( _dmap_sx[i] );
        rscuda::rggb_cuda_free( _dmap_sy[i] );
        _dmap_sx[i] = nullptr;
        _dmap_sy[i] = nullptr;
    }
#endif
}

void dual_rgb_rectify_filter::ensure_maps()
{
    if( ! _p0 || ! _p1 )
        return;

    // Both color eyes stream at the same selected output resolution; build the rectification at
    // that geometry. The color profile intrinsics are already resolution-correct -- for the D401
    // GMSL get_color_intrinsics reports the native 1288x808 calibration transformed by the same
    // center-crop + bilinear-scale the image path applies (rggb_converter / crop_scale_rgb8), so we
    // use them as-is. (The old path hardcoded the native 1288x808 size, which no advertised
    // resolution matches -> the map geometry never lined up with the frame and rectify never ran.)
    const int out_w = _p0.width(), out_h = _p0.height();
    if( out_w <= 0 || out_h <= 0 )
        return;

    rs2_intrinsics inL = _p0.get_intrinsics();
    rs2_intrinsics inR = _p1.get_intrinsics();

    rs2_extrinsics lr = _p0.get_extrinsics_to( _p1 );   // left -> right (carries the baseline)

    _rc = rect::compute( inL, inR, lr, out_w, out_h );
    _maps_w = out_w;   // remember the geometry these tables were built for, so a resolution change
    _maps_h = out_h;   // invalidates them (see process_frame) instead of cutting the frame.
    _ready = true;

    // The rectified image is a pinhole projection with a new common focal and the principal point at
    // the image center (see rect::build_table), and the source distortion has been remapped out. Clone
    // each eye's profile with those intrinsics so consumers that deproject (rs2_deproject_pixel_to_point,
    // pointcloud, align) get the geometry of the image we actually hand them.
    auto rectified_intrinsics = [&]( const rs2::video_stream_profile & src ) {
        rs2_intrinsics in = src.get_intrinsics();
        in.fx = in.fy = _rc.new_f;
        in.ppx = out_w * 0.5f;
        in.ppy = out_h * 0.5f;
        in.model = RS2_DISTORTION_NONE;
        for( auto & c : in.coeffs )
            c = 0.f;
        return in;
    };
    _tgt_p[0] = _p0.clone( _p0.stream_type(), _p0.stream_index(), _p0.format(), out_w, out_h, rectified_intrinsics( _p0 ) );
    _tgt_p[1] = _p1.clone( _p1.stream_type(), _p1.stream_index(), _p1.format(), out_w, out_h, rectified_intrinsics( _p1 ) );

#ifdef RS2_USE_CUDA
    // Upload the (constant) remap tables to the device once so the GPU remap reads them directly.
    if( rsutils::rs2_is_gpu_available() )
    {
        const size_t lbytes = _rc.left.sx.size()  * sizeof( float );
        const size_t rbytes = _rc.right.sx.size() * sizeof( float );
        _dmap_sx[0] = rscuda::rggb_cuda_alloc_upload( _rc.left.sx.data(),  lbytes );
        _dmap_sy[0] = rscuda::rggb_cuda_alloc_upload( _rc.left.sy.data(),  lbytes );
        _dmap_sx[1] = rscuda::rggb_cuda_alloc_upload( _rc.right.sx.data(), rbytes );
        _dmap_sy[1] = rscuda::rggb_cuda_alloc_upload( _rc.right.sy.data(), rbytes );
    }
#endif
}

rs2::frame dual_rgb_rectify_filter::process_frame( const rs2::frame_source & source, const rs2::frame & f )
{
    auto vf = f.as< rs2::video_frame >();
    if( ! vf || vf.get_profile().format() != RS2_FORMAT_RGB8 )
        return f;

    // Both color pins' threads call this on one shared instance; serialize all shared-state access
    // (profiles, _ready, maps, _rc, _tmp, device pointers) and the internal free_device_maps() call.
    std::lock_guard< std::mutex > lock( _mutex );

    const int idx = vf.get_profile().stream_index();

    // If the stream resolution changed since the maps were built, they are for the old geometry and
    // would remap into only part of the frame (e.g. 848-wide maps into a 1280 frame -> right ~34%
    // black). Drop the cached maps + captured profiles so they rebuild at the new size.
    if( _ready && ( vf.get_width() != _maps_w || vf.get_height() != _maps_h ) )
    {
        free_device_maps();
        _p0 = rs2::video_stream_profile{};
        _p1 = rs2::video_stream_profile{};
        _tgt_p[0] = rs2::stream_profile{};
        _tgt_p[1] = rs2::stream_profile{};
        _ready = false;
    }

    if( auto vsp = vf.get_profile().as< rs2::video_stream_profile >() )
    {
        if( idx == 0 && ! _p0 ) _p0 = vsp;
        else if( idx == 1 && ! _p1 ) _p1 = vsp;
    }

    if( ! _ready )
    {
        ensure_maps();
        if( ! _ready )
            return f;   // pass through until both eyes' calibration is available
    }

    const int eye = ( idx == 1 ) ? 1 : 0;
    const rect::remap_table & t = ( eye == 1 ) ? _rc.right : _rc.left;
    const int w = vf.get_width(), h = vf.get_height();
    const int src_stride = vf.get_stride_in_bytes();   // honor the frame's real (possibly padded) stride
    if( t.w > w || t.h > h )
        return f;   // unexpected geometry; don't touch
    if( ! _tgt_p[eye] )
        return f;   // no rectified profile to advertise; better to pass the frame through unchanged

    rs2::frame tgt = source.allocate_video_frame( _tgt_p[eye], f );
    auto tvf = tgt.as< rs2::video_frame >();
    if( ! tvf )
        return f;
    uint8_t * dst = static_cast< uint8_t * >( const_cast< void * >( tvf.get_data() ) );
    const int dstride = tvf.get_stride_in_bytes();

#ifdef RS2_USE_CUDA
    // GPU remap straight into the output frame (in place under zero-copy, no host round-trip).
    if( rsutils::rs2_is_gpu_available() && _dmap_sx[eye] && _dmap_sy[eye] )
    {
        rscuda::rggb_remap_rgb8_cuda( static_cast< const uint8_t * >( vf.get_data() ), w, h, src_stride,
                                      static_cast< const float * >( _dmap_sx[eye] ),
                                      static_cast< const float * >( _dmap_sy[eye] ),
                                      t.w, t.h, dst, dstride );
        return tgt;
    }
#endif

    // CPU: rectify the real-width content into scratch, then place it (left-aligned) into the
    // output frame (any padding columns stay zero), so the stream's advertised geometry is unchanged.
    _tmp.resize( (size_t)t.w * t.h * 3 );
    rect::remap_rgb8( static_cast< const uint8_t * >( vf.get_data() ), w, h, src_stride, t, _tmp.data() );
    const int rows = std::min( h, t.h );   // never read past the (t.h-row) remap scratch
    for( int y = 0; y < rows; ++y )
    {
        std::memcpy( dst + (size_t)y * dstride, _tmp.data() + (size_t)y * t.w * 3, (size_t)t.w * 3 );
        if( w > t.w )
            std::memset( dst + (size_t)y * dstride + t.w * 3, 0, (size_t)( w - t.w ) * 3 );
    }
    for( int y = rows; y < h; ++y )        // frame taller than the table: zero the remaining rows
        std::memset( dst + (size_t)y * dstride, 0, (size_t)w * 3 );
    return tgt;
}

}  // namespace librealsense
