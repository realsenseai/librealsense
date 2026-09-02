// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rggb-converter.h"
#include <src/core/video.h>     // video_stream_profile_interface
#include <src/image.h>          // get_image_bpp
#include <src/stream.h>         // struct rs2_stream_profile (->profile)
#include <librealsense2/hpp/rs_frame.hpp>   // rs2::video_stream_profile
#include <cstdio>
#include <thread>
#include <vector>

#ifdef RS2_USE_CUDA
#include "cuda/cuda-rggb.cuh"
#include "rsutils/accelerators/gpu.h"   // rsutils::rs2_is_cuda_available
#endif

namespace librealsense
{
    void rggb_converter::init_profiles_info( const rs2::frame * f )
    {
        auto p = f->get_profile();
        if( p.get() != _source_stream_profile.get() )
        {
            _source_stream_profile = p;

            // Source dimensions. RAW8 passthrough is 1 byte/pixel and the V4L2 profile width is
            // the padded transport width (e.g. 1612). The real row stride is larger still — the
            // kernel pads each row to 64 bytes (1612 -> 1664) — so process_function() derives the
            // true stride from the frame's raw size rather than trusting width.
            if( auto vsp = p.as< rs2::video_stream_profile >() )
            {
                _src_width  = vsp.width();
                _src_height = vsp.height();
            }
            else
            {
                _src_width = _native_width;
            }

            _target_stream_profile = p.clone( p.stream_type(), p.stream_index(), _target_format );
            _target_bpp = get_image_bpp( _target_format ) / 8;

            // Set the target profile dims to the requested OUTPUT resolution (crop-to-aspect +
            // scale from the native sensor image). This matches the advertised resolution
            // (registration's resolution_transform) and the frame allocated in prepare_frame().
            auto target_spi = (stream_profile_interface *)_target_stream_profile.get()->profile;
            if( auto target_vspi = dynamic_cast< video_stream_profile_interface * >( target_spi ) )
                target_vspi->set_dims( static_cast< uint32_t >( _out_width ),
                                       static_cast< uint32_t >( _out_height ) );
        }
    }

    rs2::frame rggb_converter::prepare_frame( const rs2::frame_source & source, const rs2::frame & f )
    {
        init_profiles_info( &f );
        // Allocate the output at the requested resolution (out_width x out_height), tight stride.
        return source.allocate_video_frame( _target_stream_profile, f, _target_bpp,
                                            _out_width, _out_height, _out_width * _target_bpp, _extension_type );
    }

    rs2::frame rggb_converter::process_frame( const rs2::frame_source & source, const rs2::frame & f )
    {
        // One shared instance serves both color pins (see _proc_mutex) - serialize the whole
        // convert so the per-instance scratch / AWB / _src_data_size can't be written concurrently.
        std::lock_guard< std::mutex > lock( _proc_mutex );
        // Capture the source frame's *actual* byte count - the authoritative way to recover the
        // real row stride regardless of whether the backend keeps the padded V4L2 buffer (1664)
        // or repacks to width*bpp (1612).
        // The source frame's actual byte count gives the true row stride (data_size / height),
        // independent of whether the backend keeps 1664-padded rows or hands us a 1612 frame.
        _src_data_size = f.get_data_size();
        return functional_processing_block::process_frame( source, f );
    }

    void rggb_converter::process_function( uint8_t * const dest[], const uint8_t * source,
                                           int /*width*/, int /*height*/, int /*actual_size*/, int /*input_size*/ )
    {
        // The 'RGGB 8-bit' node actually carries MIPI RAW10 (4 px / 5 bytes). Recover the row
        // stride from the frame's real byte count (fallback: 64-byte-aligned source width), unpack
        // RAW10 -> 8-bit Bayer at the native sensor width, demosaic to native RGB, then center-crop
        // to the output aspect ratio and bilinear-scale to the requested output resolution.
        int src_stride = ( _src_width + 63 ) & ~63;
        if( _src_data_size > 0 && _src_height > 0 && ( _src_data_size % _src_height ) == 0 )
            src_stride = _src_data_size / _src_height;

        const int native_w = _native_width;   // real sensor width, e.g. 1288 (multiple of 4)
        const int native_h = _src_height;      // native rows, e.g. 808
        const int real_width = native_w;       // AWB samples the native image
        const int height = native_h;           // AWB/debayer iterate native rows

        // RAW10 packs 4 px / 5 bytes, so a full native row occupies (native_w / 4) * 5 bytes and both
        // the AWB sampling below and unpack_raw10 read that far into every row. A truncated frame (a
        // partial V4L2 buffer) would make them read past the end of the source buffer.
        const int min_src_stride = ( native_w / 4 ) * 5;
        if( src_stride < min_src_stride )
        {
            LOG_WARNING( "RGGB converter: truncated frame, row stride " << src_stride << " < " << min_src_stride
                                                                       << " bytes - dropping" );
            return;
        }

        // Hybrid auto white balance: white-patch (balance the brightest unclipped surfaces to neutral),
        // falling back to gray-world when the bright band is a tiny fraction of the frame - i.e. isolated
        // light sources, where white-patch would balance the whole scene to the lamp colour. EMA-smoothed.
        {
            const int bl = _isp.black_level;
            auto bval = [&]( int x, int y ) -> int {
                int v = (int)source[ (size_t)y * src_stride + (size_t)( x >> 2 ) * 5 + ( x & 3 ) ] - bl;
                return v < 0 ? 0 : v;
            };
            const int step = 16;          // sample one RGGB cell every 16 px
            const int hi = 220;           // clip threshold: a channel at/above this is blown -> skip
            int gmax = 1;                 // brightest unclipped green among the samples
            for( int y = 0; y + 1 < height; y += step )
                for( int x = 0; x + 1 < real_width; x += step )
                {
                    const int g = ( bval( x + 1, y ) + bval( x, y + 1 ) ) >> 1;
                    if( g < hi && g > gmax ) gmax = g;
                }
            const int gthr = ( gmax * 3 ) / 5;   // top ~40% brightness band = light/white surfaces
            double sR = 0, sG = 0, sB = 0;   long n = 0;    // white-patch (bright band)
            double wR = 0, wG = 0, wB = 0;   long wn = 0;   // gray-world (all unclipped samples)
            for( int y = 0; y + 1 < height; y += step )
                for( int x = 0; x + 1 < real_width; x += step )   // x,y even -> land on R sites
                {
                    int rr  = bval( x, y );                                // R site (BGGR: this is B)
                    const int gg2 = ( bval( x + 1, y ) + bval( x, y + 1 ) ) >> 1;  // (Gr + Gb) / 2
                    int bb  = bval( x + 1, y + 1 );                        // B site (BGGR: this is R)
                    if( _isp.swap_rb ) { int t = rr; rr = bb; bb = t; }    // BGGR: real R/B are swapped
                    if( rr < hi && gg2 < hi && bb < hi ) { wR += rr; wG += gg2; wB += bb; ++wn; }  // gray-world
                    if( gg2 < gthr )         continue;                     // not a bright surface
                    if( rr >= hi || gg2 >= hi || bb >= hi ) continue;      // any channel clipped
                    sR += rr;  sG += gg2;  sB += bb;  ++n;
                }
            auto clampg = []( float g ) { return g < 0.5f ? 0.5f : ( g > 4.f ? 4.f : g ); };
            // Bright band under 10% of the sampled pixels => isolated light sources, not a white surface,
            // so use gray-world; otherwise white-patch (a white surface or a colour-dominated scene).
            const bool bright_isolated = ( wn > 20 ) && ( (double)n / (double)wn < 0.10 );
            float tR = -1.f, tB = -1.f;
            if( bright_isolated )                          // isolated light source -> gray-world
            {
                if( wR > 1.0 && wB > 1.0 ) { tR = clampg( float( wG / wR ) ); tB = clampg( float( wG / wB ) ); }
            }
            else if( n > 20 && sR > 1.0 && sB > 1.0 )      // real bright/white surface -> white-patch
            {
                tR = clampg( float( sG / sR ) ); tB = clampg( float( sG / sB ) );
            }
            else if( wn > 20 && wR > 1.0 && wB > 1.0 )     // no trustworthy bright band -> gray-world
            {
                tR = clampg( float( wG / wR ) ); tB = clampg( float( wG / wB ) );
            }
            if( tR > 0.f )
            {
                const float a = 0.1f;                              // EMA: converges in ~30 frames
                _awb_gain_r += a * ( tR - _awb_gain_r );
                _awb_gain_b += a * ( tB - _awb_gain_b );
            }
        }
        rggb::isp_params isp = _isp;     // per-frame ISP with the auto-white-balance gains
        isp.gain_r = _awb_gain_r;
        isp.gain_g = 1.f;
        isp.gain_b = _awb_gain_b;

        (void)real_width; (void)height;  // aliases for the AWB loop above; native_w/native_h below

#ifdef RS2_USE_CUDA
        // GPU path: fused RAW10 unpack + demosaic + tone to native RGB, then crop-to-aspect +
        // bilinear scale, writing the output frame in place under zero-copy (no host round-trip).
        if( rsutils::rs2_is_cuda_available() )
        {
            rscuda::rggb_isp_params ip{};
            ip.black_level = isp.black_level;
            ip.gain_r = isp.gain_r;  ip.gain_g = isp.gain_g;  ip.gain_b = isp.gain_b;
            ip.digital_gain = isp.digital_gain;  ip.gamma = isp.gamma;  ip.s_curve = isp.s_curve;
            ip.saturation = isp.saturation;  ip.contrast = isp.contrast;
            ip.swap_rb = isp.swap_rb ? 1 : 0;
            for( int i = 0; i < 9; ++i ) ip.ccm[i] = isp.ccm[i];
            rscuda::rggb_debayer_scale_raw10_cuda( source, src_stride, native_w, native_h, ip,
                                                   dest[0], _out_width, _out_height );
            return;
        }
#endif

        // CPU: demosaic to native RGB scratch, then crop-to-aspect + scale to the output frame.
        _bayer.resize( static_cast< size_t >( native_w ) * native_h );
        rggb::unpack_raw10( source, src_stride, native_w, native_h, _bayer.data() );
        _rgb_native.resize( static_cast< size_t >( native_w ) * native_h * 3 );
        // The tone LUT depends only on gamma / s_curve, so build it once here rather than letting each
        // of the bands below rebuild it (1024 std::pow per band).
        if( _tone_gamma != isp.gamma || _tone_s_curve != isp.s_curve )
        {
            rggb::build_tone_lut( isp, _tone );
            _tone_gamma = isp.gamma;
            _tone_s_curve = isp.s_curve;
        }
        // Demosaic is the CPU hot path; split it across row bands (the Jetson has spare cores).
        {
            const int nthreads = 4;
            const int band = ( native_h + nthreads - 1 ) / nthreads;
            std::vector< std::thread > pool;
            for( int t = 1; t < nthreads; ++t )
            {
                const int b0 = t * band, b1 = ( native_h < b0 + band ) ? native_h : b0 + band;
                if( b0 >= b1 ) break;
                pool.emplace_back( [&, b0, b1]() {
                    rggb::debayer_rggb8( _bayer.data(), native_w, native_w, native_h,
                                         _rgb_native.data(), isp, native_w, b0, b1, _tone );
                } );
            }
            rggb::debayer_rggb8( _bayer.data(), native_w, native_w, native_h, _rgb_native.data(), isp, native_w,
                                 0, ( native_h < band ) ? native_h : band, _tone );
            for( auto & th : pool ) th.join();
        }
        // Crop-to-aspect + bilinear scale native RGB -> output frame (threaded over output rows).
        {
            const int nthreads = 4;
            const int band = ( _out_height + nthreads - 1 ) / nthreads;
            std::vector< std::thread > pool;
            for( int t = 1; t < nthreads; ++t )
            {
                const int b0 = t * band, b1 = ( _out_height < b0 + band ) ? _out_height : b0 + band;
                if( b0 >= b1 ) break;
                pool.emplace_back( [&, b0, b1]() {
                    rggb::crop_scale_rgb8( _rgb_native.data(), native_w, native_h, native_w,
                                           dest[0], _out_width, _out_height, b0, b1 );
                } );
            }
            rggb::crop_scale_rgb8( _rgb_native.data(), native_w, native_h, native_w,
                                   dest[0], _out_width, _out_height, 0, ( _out_height < band ) ? _out_height : band );
            for( auto & th : pool ) th.join();
        }
    }
}
