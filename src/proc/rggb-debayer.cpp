// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rggb-debayer.h"

#include <cstddef>   // size_t
#include <cmath>     // std::pow
#include <cstring>   // std::memcpy
#include <vector>

namespace librealsense {
namespace rggb {

namespace {

inline int clampi( int v, int lo, int hi )
{
    return v < lo ? lo : ( v > hi ? hi : v );
}

inline uint8_t to_u8( float v )
{
    int i = static_cast< int >( v + 0.5f );
    return static_cast< uint8_t >( i < 0 ? 0 : ( i > 255 ? 255 : i ) );
}

inline float clamp01( float v ) { return v < 0.f ? 0.f : ( v > 1.f ? 1.f : v ); }

}  // namespace

void unpack_raw10( const uint8_t * src, int src_stride, int real_width, int height, uint8_t * bayer8 )
{
    const int groups = real_width / 4;   // 5 source bytes per 4 pixels
    for( int y = 0; y < height; ++y )
    {
        const uint8_t * s = src + static_cast< size_t >( y ) * src_stride;
        uint8_t * d = bayer8 + static_cast< size_t >( y ) * real_width;
        for( int g = 0; g < groups; ++g )
        {
            const uint8_t * q = s + g * 5;          // [m0 m1 m2 m3 lsb]; 8-bit value == MSB byte
            d[ g * 4 + 0 ] = q[ 0 ];
            d[ g * 4 + 1 ] = q[ 1 ];
            d[ g * 4 + 2 ] = q[ 2 ];
            d[ g * 4 + 3 ] = q[ 3 ];
        }
    }
}

void build_tone_lut( const isp_params & p, uint8_t tone[1024] )
{
    // The sensor data is linear; encode with 1/gamma (sRGB-like) so midtones aren't crushed on a
    // display. 1024-entry LUT indexed by the normalized [0,1] post-CCM value.
    const float inv_g = ( p.gamma > 0.f ) ? 1.f / p.gamma : 1.f;
    const float sc = p.s_curve;
    for( int i = 0; i < 1024; ++i )
    {
        float x = std::pow( i / 1023.f, inv_g );
        x = x + sc * x * ( 1.f - x ) * ( 2.f * x - 1.f );    // S-curve contrast (the "pop")
        tone[i] = to_u8( 255.f * clamp01( x ) );
    }
}

void debayer_rggb8( const uint8_t * bayer, int bayer_stride, int width, int height,
                    uint8_t * dst, const isp_params & p, int dst_stride_px,
                    int y_begin, int y_end, const uint8_t * tone_in )
{
    const int wmax = width - 1;
    const int hmax = height - 1;
    const int yb = y_begin;
    const int ye = ( y_end < 0 ) ? height : y_end;
    const int bl   = p.black_level;
    const int row_px = ( dst_stride_px > width ) ? dst_stride_px : width;

    uint8_t tone_local[1024];
    const uint8_t * tone = tone_in;
    if( ! tone )
    {
        build_tone_lut( p, tone_local );
        tone = tone_local;
    }

    const float gr = p.gain_r * p.digital_gain;
    const float gg = p.gain_g * p.digital_gain;
    const float gb = p.gain_b * p.digital_gain;
    const float sat = p.saturation, con = p.contrast;
    const float * m = p.ccm;
    // Normalize the black-subtracted value to [0,1] using the post-black range (255 - black), so a
    // full-scale sensor reading maps to 1.0 (matches the reference raw decode).
    const float inv_range = 1.f / ( 255.f - (float)p.black_level );

    // Tone curve and contrast folded into one output LUT (same values as applying them per pixel).
    uint8_t out_lut[1024];
    for( int i = 0; i < 1024; ++i )
        out_lut[i] = to_u8( ( tone[i] - 128.f ) * con + 128.f );

    // Black-level-subtracted Bayer sample at (x,y): edge-clamped for border pixels, direct inside.
    auto S_edge = [&]( int x, int y ) -> int {
        x = clampi( x, 0, wmax );
        y = clampi( y, 0, hmax );
        int v = static_cast< int >( bayer[ y * bayer_stride + x ] ) - bl;
        return v < 0 ? 0 : v;
    };
    auto S_inner = [&]( int x, int y ) -> int {
        int v = static_cast< int >( bayer[ y * bayer_stride + x ] ) - bl;
        return v < 0 ? 0 : v;
    };

    std::vector< float > fr( width ), fg( width ), fb( width );
    auto demosaic = [&]( const auto & S, int x, int y )
    {
            const int yodd = y & 1;
            const int xodd = x & 1;
            float R, G, B;

            if( !yodd && !xodd )            // R site
            {
                R = (float)S( x, y );
                G = ( S( x - 1, y ) + S( x + 1, y ) + S( x, y - 1 ) + S( x, y + 1 ) ) * 0.25f;
                B = ( S( x - 1, y - 1 ) + S( x + 1, y - 1 ) + S( x - 1, y + 1 ) + S( x + 1, y + 1 ) ) * 0.25f;
            }
            else if( !yodd && xodd )        // Gr site (red row): H=R, V=B
            {
                G = (float)S( x, y );
                R = ( S( x - 1, y ) + S( x + 1, y ) ) * 0.5f;
                B = ( S( x, y - 1 ) + S( x, y + 1 ) ) * 0.5f;
            }
            else if( yodd && !xodd )        // Gb site (blue row): H=B, V=R
            {
                G = (float)S( x, y );
                R = ( S( x, y - 1 ) + S( x, y + 1 ) ) * 0.5f;
                B = ( S( x - 1, y ) + S( x + 1, y ) ) * 0.5f;
            }
            else                            // B site
            {
                B = (float)S( x, y );
                G = ( S( x - 1, y ) + S( x + 1, y ) + S( x, y - 1 ) + S( x, y + 1 ) ) * 0.25f;
                R = ( S( x - 1, y - 1 ) + S( x + 1, y - 1 ) + S( x - 1, y + 1 ) + S( x + 1, y + 1 ) ) * 0.25f;
            }

            if( p.swap_rb ) { float t = R; R = B; B = t; }   // RGGB demosaic -> BGGR (real D401 phase)
            fr[x] = R; fg[x] = G; fb[x] = B;
    };

    for( int y = yb; y < ye; ++y )
    {
        uint8_t * row = dst + static_cast< size_t >( y ) * row_px * 3;
        if( y == 0 || y == hmax )
        {
            for( int x = 0; x < width; ++x )
                demosaic( S_edge, x, y );
        }
        else
        {
            demosaic( S_edge, 0, y );
            for( int x = 1; x < wmax; ++x )
                demosaic( S_inner, x, y );
            demosaic( S_edge, wmax, y );
        }
        for( int x = 0; x < width; ++x )
        {
            uint8_t * px = row + x * 3;
            const float R = fr[x], G = fg[x], B = fb[x];

            // White-balance + digital gain, normalized to [0,1].
            float r = clamp01( R * gr * inv_range );
            float g = clamp01( G * gg * inv_range );
            float b = clamp01( B * gb * inv_range );
            // Color-correction matrix (sensor RGB -> display primaries).
            float r2 = m[0] * r + m[1] * g + m[2] * b;
            float g2 = m[3] * r + m[4] * g + m[5] * b;
            float b2 = m[6] * r + m[7] * g + m[8] * b;
            // Saturation about luma (linear, Rec.709), then gamma (LUT) + contrast about mid-grey.
            const float yl = 0.2126f * r2 + 0.7152f * g2 + 0.0722f * b2;
            r2 = clamp01( yl + sat * ( r2 - yl ) );
            g2 = clamp01( yl + sat * ( g2 - yl ) );
            b2 = clamp01( yl + sat * ( b2 - yl ) );
            px[0] = out_lut[ static_cast< int >( r2 * 1023.f ) ];
            px[1] = out_lut[ static_cast< int >( g2 * 1023.f ) ];
            px[2] = out_lut[ static_cast< int >( b2 * 1023.f ) ];
        }
        // Zero any padding columns so a narrower image sits cleanly in a wider output frame.
        for( int x = width; x < row_px; ++x )
        {
            row[ x * 3 + 0 ] = 0;
            row[ x * 3 + 1 ] = 0;
            row[ x * 3 + 2 ] = 0;
        }
    }
}

void crop_rect_for_output( int src_w, int src_h, int out_w, int out_h,
                           int * crop_x, int * crop_y, int * crop_w, int * crop_h )
{
    // Centered crop matching the output aspect ratio (so the subsequent scale doesn't stretch).
    // Compare src_w*out_h vs out_w*src_h to avoid float rounding: target "wider" than source ->
    // crop the height; "narrower" -> crop the width.
    int cw = src_w, ch = src_h;
    const long long src_ar = (long long)src_w * out_h;   // src_w/src_h  vs
    const long long out_ar = (long long)out_w * src_h;   // out_w/out_h
    if( out_ar > src_ar )                                // output is wider -> limit by width, crop height
        ch = (int)( ( (long long)src_w * out_h ) / out_w );
    else if( out_ar < src_ar )                           // output is narrower/taller -> crop width
        cw = (int)( ( (long long)src_h * out_w ) / out_h );
    if( cw > src_w ) cw = src_w;
    if( ch > src_h ) ch = src_h;
    if( cw < 1 ) cw = 1;
    if( ch < 1 ) ch = 1;
    *crop_w = cw; *crop_h = ch;
    *crop_x = ( src_w - cw ) / 2;
    *crop_y = ( src_h - ch ) / 2;
}

void crop_scale_rgb8( const uint8_t * src, int src_w, int src_h, int src_stride_px,
                      uint8_t * dst, int out_w, int out_h, int y_begin, int y_end )
{
    if( y_end < 0 ) y_end = out_h;

    int cx, cy, cw, ch;
    crop_rect_for_output( src_w, src_h, out_w, out_h, &cx, &cy, &cw, &ch );

    // Fast path: crop already equals output (e.g. native res requested) -> straight row copy.
    if( cw == out_w && ch == out_h )
    {
        for( int y = y_begin; y < y_end; ++y )
        {
            const uint8_t * s = src + ( (size_t)( cy + y ) * src_stride_px + cx ) * 3;
            std::memcpy( dst + (size_t)y * out_w * 3, s, (size_t)out_w * 3 );
        }
        return;
    }

    // Bilinear scale of the crop rect [cx,cx+cw) x [cy,cy+ch) -> out_w x out_h. Map output pixel
    // centers back into the crop (the +0.5/-0.5 keeps the sampling centered, no half-pixel shift).
    const float sx = (float)cw / (float)out_w;
    const float sy = (float)ch / (float)out_h;
    for( int oy = y_begin; oy < y_end; ++oy )
    {
        float fy = ( oy + 0.5f ) * sy - 0.5f;
        int   y0 = (int)( fy < 0.f ? 0.f : fy );
        if( y0 > ch - 1 ) y0 = ch - 1;
        int   y1 = ( y0 + 1 < ch ) ? y0 + 1 : y0;
        float wy = fy - (float)y0; if( wy < 0.f ) wy = 0.f;
        const uint8_t * r0 = src + ( (size_t)( cy + y0 ) * src_stride_px + cx ) * 3;
        const uint8_t * r1 = src + ( (size_t)( cy + y1 ) * src_stride_px + cx ) * 3;
        uint8_t * orow = dst + (size_t)oy * out_w * 3;
        for( int ox = 0; ox < out_w; ++ox )
        {
            float fx = ( ox + 0.5f ) * sx - 0.5f;
            int   x0 = (int)( fx < 0.f ? 0.f : fx );
            if( x0 > cw - 1 ) x0 = cw - 1;
            int   x1 = ( x0 + 1 < cw ) ? x0 + 1 : x0;
            float wx = fx - (float)x0; if( wx < 0.f ) wx = 0.f;
            for( int c = 0; c < 3; ++c )
            {
                float top = r0[ x0 * 3 + c ] * ( 1.f - wx ) + r0[ x1 * 3 + c ] * wx;
                float bot = r1[ x0 * 3 + c ] * ( 1.f - wx ) + r1[ x1 * 3 + c ] * wx;
                float v   = top * ( 1.f - wy ) + bot * wy;
                orow[ ox * 3 + c ] = (uint8_t)( v < 0.f ? 0.f : ( v > 255.f ? 255.f : v + 0.5f ) );
            }
        }
    }
}

}  // namespace rggb
}  // namespace librealsense
