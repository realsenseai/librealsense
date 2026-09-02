// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// Guard on either macro (not just RS2_USE_CUDA) so this file keeps compiling under HIP
// regardless of whether global_config.cmake's BUILD_WITH_HIP branch also defines
// RS2_USE_CUDA (today, for back-compat with other RS2_USE_CUDA-only guards) or drops it
// in favor of RS2_USE_HIP alone -- either way this file must still compile under HIP.
#if defined(RS2_USE_CUDA) || defined(RS2_USE_HIP)

#include "cuda-rggb.cuh"
#include <librealsense2/rs.h>   // rs2_intrinsics/rs2_extrinsics (used by rscuda_utils.cuh)
#include "rscuda_utils.cuh"
#include <memory>

namespace {

// One 8-bit Bayer sample at (x,y): the RAW10 8-bit value is just the MSB byte of each 5-byte / 4-px
// group (msb<<2 | lsb) >> 2 == msb. Edge-clamped, black-level subtracted (matches rggb-debayer.cpp).
__device__ __forceinline__ int bayer_at( const uint8_t * src, int stride, int x, int y,
                                          int wmax, int hmax, int bl )
{
    x = x < 0 ? 0 : ( x > wmax ? wmax : x );
    y = y < 0 ? 0 : ( y > hmax ? hmax : y );
    int v = (int)src[ (size_t)y * stride + (size_t)( x >> 2 ) * 5 + ( x & 3 ) ] - bl;
    return v < 0 ? 0 : v;
}

__device__ __forceinline__ float clamp01( float v ) { return v < 0.f ? 0.f : ( v > 1.f ? 1.f : v ); }

__device__ __forceinline__ unsigned char clamp_u8( float v )
{
    int i = (int)( v + 0.5f );
    return (unsigned char)( i < 0 ? 0 : ( i > 255 ? 255 : i ) );
}

__global__ void kernel_rggb_debayer( const uint8_t * src, int src_stride, int width, int height,
                                     uint8_t * dst, int dst_stride, rscuda::rggb_isp_params p )
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    if( x >= width || y >= height )
        return;

    const int wmax = width - 1, hmax = height - 1, bl = p.black_level;
    const int xodd = x & 1, yodd = y & 1;
    float R, G, B;

    if( !yodd && !xodd )            // R site
    {
        R = (float)bayer_at( src, src_stride, x, y, wmax, hmax, bl );
        G = ( bayer_at( src, src_stride, x - 1, y, wmax, hmax, bl ) + bayer_at( src, src_stride, x + 1, y, wmax, hmax, bl )
            + bayer_at( src, src_stride, x, y - 1, wmax, hmax, bl ) + bayer_at( src, src_stride, x, y + 1, wmax, hmax, bl ) ) * 0.25f;
        B = ( bayer_at( src, src_stride, x - 1, y - 1, wmax, hmax, bl ) + bayer_at( src, src_stride, x + 1, y - 1, wmax, hmax, bl )
            + bayer_at( src, src_stride, x - 1, y + 1, wmax, hmax, bl ) + bayer_at( src, src_stride, x + 1, y + 1, wmax, hmax, bl ) ) * 0.25f;
    }
    else if( !yodd && xodd )        // Gr site (red row): H=R, V=B
    {
        G = (float)bayer_at( src, src_stride, x, y, wmax, hmax, bl );
        R = ( bayer_at( src, src_stride, x - 1, y, wmax, hmax, bl ) + bayer_at( src, src_stride, x + 1, y, wmax, hmax, bl ) ) * 0.5f;
        B = ( bayer_at( src, src_stride, x, y - 1, wmax, hmax, bl ) + bayer_at( src, src_stride, x, y + 1, wmax, hmax, bl ) ) * 0.5f;
    }
    else if( yodd && !xodd )        // Gb site (blue row): H=B, V=R
    {
        G = (float)bayer_at( src, src_stride, x, y, wmax, hmax, bl );
        R = ( bayer_at( src, src_stride, x, y - 1, wmax, hmax, bl ) + bayer_at( src, src_stride, x, y + 1, wmax, hmax, bl ) ) * 0.5f;
        B = ( bayer_at( src, src_stride, x - 1, y, wmax, hmax, bl ) + bayer_at( src, src_stride, x + 1, y, wmax, hmax, bl ) ) * 0.5f;
    }
    else                            // B site
    {
        B = (float)bayer_at( src, src_stride, x, y, wmax, hmax, bl );
        G = ( bayer_at( src, src_stride, x - 1, y, wmax, hmax, bl ) + bayer_at( src, src_stride, x + 1, y, wmax, hmax, bl )
            + bayer_at( src, src_stride, x, y - 1, wmax, hmax, bl ) + bayer_at( src, src_stride, x, y + 1, wmax, hmax, bl ) ) * 0.25f;
        R = ( bayer_at( src, src_stride, x - 1, y - 1, wmax, hmax, bl ) + bayer_at( src, src_stride, x + 1, y - 1, wmax, hmax, bl )
            + bayer_at( src, src_stride, x - 1, y + 1, wmax, hmax, bl ) + bayer_at( src, src_stride, x + 1, y + 1, wmax, hmax, bl ) ) * 0.25f;
    }

    if( p.swap_rb ) { float t = R; R = B; B = t; }   // RGGB demosaic -> BGGR (real D401 phase)

    const float gr = p.gain_r * p.digital_gain;
    const float gg = p.gain_g * p.digital_gain;
    const float gb = p.gain_b * p.digital_gain;
    const float inv_g = ( p.gamma > 0.f ) ? 1.f / p.gamma : 1.f;
    const float * m = p.ccm;
    const float inv_range = 1.f / ( 255.f - (float)p.black_level );

    // white-balance + digital gain, normalized to [0,1] (matches rggb-debayer.cpp)
    float r = clamp01( R * gr * inv_range );
    float g = clamp01( G * gg * inv_range );
    float b = clamp01( B * gb * inv_range );
    // color-correction matrix
    float r2 = m[0] * r + m[1] * g + m[2] * b;
    float g2 = m[3] * r + m[4] * g + m[5] * b;
    float b2 = m[6] * r + m[7] * g + m[8] * b;
    // saturation about luma (linear, Rec.709), gamma, then contrast about mid-grey
    const float yl = 0.2126f * r2 + 0.7152f * g2 + 0.0722f * b2;
    r2 = clamp01( yl + p.saturation * ( r2 - yl ) );
    g2 = clamp01( yl + p.saturation * ( g2 - yl ) );
    b2 = clamp01( yl + p.saturation * ( b2 - yl ) );
    // gamma, then S-curve contrast (matches rggb-debayer.cpp's tone LUT)
    float tr = powf( r2, inv_g ); tr = clamp01( tr + p.s_curve * tr * ( 1.f - tr ) * ( 2.f * tr - 1.f ) );
    float tg = powf( g2, inv_g ); tg = clamp01( tg + p.s_curve * tg * ( 1.f - tg ) * ( 2.f * tg - 1.f ) );
    float tb = powf( b2, inv_g ); tb = clamp01( tb + p.s_curve * tb * ( 1.f - tb ) * ( 2.f * tb - 1.f ) );
    const float rd = 255.f * tr, gd = 255.f * tg, bd = 255.f * tb;

    uint8_t * o = dst + (size_t)y * dst_stride + (size_t)x * 3;
    o[0] = clamp_u8( ( rd - 128.f ) * p.contrast + 128.f );
    o[1] = clamp_u8( ( gd - 128.f ) * p.contrast + 128.f );
    o[2] = clamp_u8( ( bd - 128.f ) * p.contrast + 128.f );
}

__global__ void kernel_remap_rgb8( const uint8_t * src, int src_w, int src_h, int src_stride,
                                   const float * sx, const float * sy, int out_w, int out_h,
                                   uint8_t * dst, int dst_stride )
{
    const int u = blockIdx.x * blockDim.x + threadIdx.x;
    const int v = blockIdx.y * blockDim.y + threadIdx.y;
    if( u >= out_w || v >= out_h )
        return;

    const int idx = v * out_w + u;
    const float fx = sx[idx], fy = sy[idx];
    const int x0 = (int)floorf( fx ), y0 = (int)floorf( fy );
    uint8_t * o = dst + (size_t)v * dst_stride + (size_t)u * 3;

    if( x0 < 0 || y0 < 0 || x0 + 1 >= src_w || y0 + 1 >= src_h )
    {
        o[0] = o[1] = o[2] = 0;
        return;
    }
    const float ax = fx - x0, ay = fy - y0;
    const uint8_t * p00 = src + (size_t)y0 * src_stride + (size_t)x0 * 3;
    const uint8_t * p01 = p00 + 3;
    const uint8_t * p10 = p00 + src_stride;
    const uint8_t * p11 = p10 + 3;
    for( int ch = 0; ch < 3; ++ch )
    {
        float top = p00[ch] * ( 1 - ax ) + p01[ch] * ax;
        float bot = p10[ch] * ( 1 - ax ) + p11[ch] * ax;
        o[ch] = (uint8_t)( top * ( 1 - ay ) + bot * ay + 0.5f );
    }
}

// Bilinear scale of a crop rectangle [crop_x,crop_x+crop_w) x [crop_y,crop_y+crop_h) within an
// interleaved RGB8 source to out_w x out_h. Output pixel centers map back into the crop (the
// +0.5/-0.5 keeps sampling centered). Mirrors rggb::crop_scale_rgb8 on the CPU.
__global__ void kernel_crop_scale_rgb8( const uint8_t * src, int src_stride,
                                        int crop_x, int crop_y, int crop_w, int crop_h,
                                        int out_w, int out_h, uint8_t * dst, int dst_stride )
{
    const int u = blockIdx.x * blockDim.x + threadIdx.x;
    const int v = blockIdx.y * blockDim.y + threadIdx.y;
    if( u >= out_w || v >= out_h )
        return;

    const float sx = (float)crop_w / (float)out_w;
    const float sy = (float)crop_h / (float)out_h;
    float fx = ( u + 0.5f ) * sx - 0.5f; if( fx < 0.f ) fx = 0.f;
    float fy = ( v + 0.5f ) * sy - 0.5f; if( fy < 0.f ) fy = 0.f;
    int x0 = (int)fx; if( x0 > crop_w - 1 ) x0 = crop_w - 1;
    int y0 = (int)fy; if( y0 > crop_h - 1 ) y0 = crop_h - 1;
    const int x1 = ( x0 + 1 < crop_w ) ? x0 + 1 : x0;
    const int y1 = ( y0 + 1 < crop_h ) ? y0 + 1 : y0;
    const float ax = fx - x0, ay = fy - y0;
    const uint8_t * p00 = src + (size_t)( crop_y + y0 ) * src_stride + (size_t)( crop_x + x0 ) * 3;
    const uint8_t * p01 = src + (size_t)( crop_y + y0 ) * src_stride + (size_t)( crop_x + x1 ) * 3;
    const uint8_t * p10 = src + (size_t)( crop_y + y1 ) * src_stride + (size_t)( crop_x + x0 ) * 3;
    const uint8_t * p11 = src + (size_t)( crop_y + y1 ) * src_stride + (size_t)( crop_x + x1 ) * 3;
    uint8_t * o = dst + (size_t)v * dst_stride + (size_t)u * 3;
    for( int ch = 0; ch < 3; ++ch )
    {
        float top = p00[ch] * ( 1 - ax ) + p01[ch] * ax;
        float bot = p10[ch] * ( 1 - ax ) + p11[ch] * ax;
        o[ch] = (uint8_t)( top * ( 1 - ay ) + bot * ay + 0.5f );
    }
}

// 32x8 = 256 threads/block; x wavefront-aligned so consecutive lanes hit consecutive columns.
// THREADS_IN_WARP == 32 matches RDNA's wave32 front-end on the gfx1100/gfx1101/gfx1151 targets
// this PR builds for (CDNA's 64-wide wavefronts are still handled correctly, just not maximally
// coalesced -- consistent with the existing tuning note in cuda-pointcloud.hip).
inline dim3 block_2d() { return dim3( rscuda::THREADS_IN_WARP, 8 ); }
inline dim3 grid_2d( int w, int h, dim3 b ) { return dim3( ( w + b.x - 1 ) / b.x, ( h + b.y - 1 ) / b.y ); }

}  // namespace

// Mechanical per-API port of cuda-rggb.cu: hipMemcpy/hipMalloc/hipFree/hipGetLastError/
// hipStreamSynchronize in place of the cuda* names, wrapped in RS_CUDA_CHECK instead of the
// CUDA side's cudaSuccess-comparisons/cudaGetLastError()-clears. try_device_ptr<T>/alloc_dev<T>
// are shared with the CUDA build unchanged (resolved to their HIP form via the macros in
// rscuda_utils.cuh). Kernel bodies above are copied verbatim -- HIP's clang frontend accepts the
// same __global__/__device__/<<<>>> syntax as nvcc.
void rscuda::rggb_debayer_raw10_cuda( const uint8_t * h_src, int src_stride, int width, int height,
                                      uint8_t * h_dst, int dst_stride, const rscuda::rggb_isp_params & isp )
{
    const size_t src_bytes = (size_t)src_stride * height;
    const size_t dst_bytes = (size_t)dst_stride * height;

    uint8_t * src_dev = rscuda::try_device_ptr<uint8_t>( h_src );
    uint8_t * dst_dev = rscuda::try_device_ptr<uint8_t>( h_dst );
    std::shared_ptr<uint8_t> s_src, s_dst;
    if( !src_dev )
    {
        s_src = rscuda::alloc_dev<uint8_t>( (int)src_bytes );
        RS_CUDA_CHECK( hipMemcpy( s_src.get(), h_src, src_bytes, hipMemcpyHostToDevice ) );
        src_dev = s_src.get();
    }
    if( !dst_dev )
    {
        s_dst = rscuda::alloc_dev<uint8_t>( (int)dst_bytes );
        dst_dev = s_dst.get();
    }

    const dim3 block = block_2d();
    const dim3 grid  = grid_2d( width, height, block );
    kernel_rggb_debayer<<< grid, block >>>( src_dev, src_stride, width, height, dst_dev, dst_stride, isp );
    RS_CUDA_CHECK( hipGetLastError() );

    if( s_dst )
        RS_CUDA_CHECK( hipMemcpy( h_dst, dst_dev, dst_bytes, hipMemcpyDeviceToHost ) );
    else
        RS_CUDA_CHECK( hipStreamSynchronize( 0 ) );   // mapped output: ensure CPU sees the writes
}

void rscuda::rggb_debayer_scale_raw10_cuda( const uint8_t * h_src, int src_stride, int native_w, int native_h,
                                            const rscuda::rggb_isp_params & isp,
                                            uint8_t * h_dst, int out_w, int out_h )
{
    const int native_stride = native_w * 3;
    const int dst_stride     = out_w * 3;
    const size_t src_bytes    = (size_t)src_stride    * native_h;
    const size_t native_bytes = (size_t)native_stride * native_h;
    const size_t dst_bytes    = (size_t)dst_stride    * out_h;

    // Source (RAW10) and destination (RGB8) may be zero-copy mapped frame buffers; native RGB is an
    // intermediate device scratch either way.
    uint8_t * src_dev = rscuda::try_device_ptr<uint8_t>( h_src );
    uint8_t * dst_dev = rscuda::try_device_ptr<uint8_t>( h_dst );
    std::shared_ptr<uint8_t> s_src, s_dst;
    if( !src_dev )
    {
        s_src = rscuda::alloc_dev<uint8_t>( (int)src_bytes );
        RS_CUDA_CHECK( hipMemcpy( s_src.get(), h_src, src_bytes, hipMemcpyHostToDevice ) );
        src_dev = s_src.get();
    }
    if( !dst_dev )
    {
        s_dst = rscuda::alloc_dev<uint8_t>( (int)dst_bytes );
        dst_dev = s_dst.get();
    }
    auto native = rscuda::alloc_dev<uint8_t>( (int)native_bytes );

    const dim3 block = block_2d();
    // 1) RAW10 -> demosaic + tone into the native RGB scratch.
    kernel_rggb_debayer<<< grid_2d( native_w, native_h, block ), block >>>(
        src_dev, src_stride, native_w, native_h, native.get(), native_stride, isp );
    RS_CUDA_CHECK( hipGetLastError() );

    // 2) Centered crop-to-aspect (same math as rggb::crop_rect_for_output) + bilinear scale -> dst.
    int cw = native_w, ch = native_h;
    const long long src_ar = (long long)native_w * out_h;
    const long long out_ar = (long long)out_w * native_h;
    if( out_ar > src_ar )      ch = (int)( ( (long long)native_w * out_h ) / out_w );
    else if( out_ar < src_ar ) cw = (int)( ( (long long)native_h * out_w ) / out_h );
    if( cw > native_w ) cw = native_w;
    if( ch > native_h ) ch = native_h;
    if( cw < 1 ) cw = 1;
    if( ch < 1 ) ch = 1;
    const int cx = ( native_w - cw ) / 2, cy = ( native_h - ch ) / 2;

    kernel_crop_scale_rgb8<<< grid_2d( out_w, out_h, block ), block >>>(
        native.get(), native_stride, cx, cy, cw, ch, out_w, out_h, dst_dev, dst_stride );
    RS_CUDA_CHECK( hipGetLastError() );

    if( s_dst )
        RS_CUDA_CHECK( hipMemcpy( h_dst, dst_dev, dst_bytes, hipMemcpyDeviceToHost ) );
    else
        RS_CUDA_CHECK( hipStreamSynchronize( 0 ) );   // mapped output: ensure CPU sees the writes
}

void rscuda::rggb_remap_rgb8_cuda( const uint8_t * h_src, int src_w, int src_h, int src_stride,
                                   const float * sx_dev, const float * sy_dev, int out_w, int out_h,
                                   uint8_t * h_dst, int dst_stride )
{
    const size_t src_bytes = (size_t)src_stride * src_h;
    const size_t dst_bytes = (size_t)dst_stride * out_h;

    uint8_t * src_dev = rscuda::try_device_ptr<uint8_t>( h_src );
    uint8_t * dst_dev = rscuda::try_device_ptr<uint8_t>( h_dst );
    std::shared_ptr<uint8_t> s_src, s_dst;
    if( !src_dev )
    {
        s_src = rscuda::alloc_dev<uint8_t>( (int)src_bytes );
        RS_CUDA_CHECK( hipMemcpy( s_src.get(), h_src, src_bytes, hipMemcpyHostToDevice ) );
        src_dev = s_src.get();
    }
    if( !dst_dev )
    {
        s_dst = rscuda::alloc_dev<uint8_t>( (int)dst_bytes );
        dst_dev = s_dst.get();
    }

    const dim3 block = block_2d();
    const dim3 grid  = grid_2d( out_w, out_h, block );
    kernel_remap_rgb8<<< grid, block >>>( src_dev, src_w, src_h, src_stride, sx_dev, sy_dev, out_w, out_h, dst_dev, dst_stride );
    RS_CUDA_CHECK( hipGetLastError() );

    if( s_dst )
        RS_CUDA_CHECK( hipMemcpy( h_dst, dst_dev, dst_bytes, hipMemcpyDeviceToHost ) );
    else
        RS_CUDA_CHECK( hipStreamSynchronize( 0 ) );
}

void * rscuda::rggb_cuda_alloc_upload( const void * host, size_t bytes )
{
    void * dev = nullptr;
    if( hipMalloc( &dev, bytes ) != hipSuccess )
    {
        hipGetLastError();
        return nullptr;
    }
    if( hipMemcpy( dev, host, bytes, hipMemcpyHostToDevice ) != hipSuccess )
    {
        hipFree( dev );
        hipGetLastError();
        return nullptr;
    }
    return dev;
}

void rscuda::rggb_cuda_free( void * dev_ptr )
{
    if( dev_ptr )
        hipFree( dev_ptr );
}

#endif // RS2_USE_CUDA || RS2_USE_HIP
