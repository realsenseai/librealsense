// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once
#ifndef CUDA_RGGB_CUH
#define CUDA_RGGB_CUH

// Guarded on either macro (not just RS2_USE_CUDA) so this compiles under HIP regardless of
// whether global_config.cmake's BUILD_WITH_HIP branch also defines RS2_USE_CUDA (today, for
// back-compat) or drops it in favor of RS2_USE_HIP alone.
#if defined(RS2_USE_CUDA) || defined(RS2_USE_HIP)

// CUDA path for the D401 GMSL dual-RGB pipeline (mirrors src/proc/rggb-debayer.cpp and the remap
// in src/proc/stereo-rectify.cpp). Two fused kernels:
//   * rggb_debayer_raw10_cuda : MIPI RAW10 -> RGGB demosaic -> white-balance/digital gain -> tone
//   * rggb_remap_rgb8_cuda    : bilinear rectification remap (maps precomputed on the host)
//
// Both follow the project's zero-copy convention (see cuda-pointcloud.cu): when the frame buffers
// are CUDA pinned+mapped (integrated GPU, zero-copy build) the kernel reads/writes them in place
// with no host<->device copy; otherwise a per-call staging buffer + copy is used.
//
// This header is intentionally free of CUDA types so host translation units (rggb-converter.cpp,
// dual-rgb-rectify-filter.cpp) can include it under RS2_USE_CUDA without pulling in cuda_runtime.h.

#include <cstdint>
#include <cstddef>

namespace rscuda
{
    // Mirrors librealsense::rggb::isp_params (kept separate to avoid an SDK include here).
    struct rggb_isp_params
    {
        int   black_level;
        float gain_r, gain_g, gain_b;
        float digital_gain;
        float gamma;
        float s_curve;         // contrast S-curve strength baked after gamma (the "pop")
        float saturation;
        float contrast;
        int   swap_rb;         // 1 => BGGR (swap R/B after demosaic); 0 => RGGB
        float ccm[9];          // row-major 3x3 sensor-RGB -> display-RGB color-correction matrix
    };

    // RAW10 (4 px / 5 bytes, RGGB) -> interleaved RGB8. src_stride/dst_stride are bytes per row.
    // width must be a multiple of 4; src holds width*10/8 active bytes per row (plus alignment).
    void rggb_debayer_raw10_cuda( const uint8_t * src, int src_stride, int width, int height,
                                  uint8_t * dst, int dst_stride, const rggb_isp_params & isp );

    // RAW10 -> demosaic to native RGB8 (native_w x native_h), then center-crop to the output aspect
    // ratio and bilinear-scale to out_w x out_h, written tightly into dst (out_w*3 bytes/row). Used
    // for the user-selectable dual-RGB output resolutions (crop-to-aspect + scale, no stretch).
    // dst may be a zero-copy mapped frame buffer; a device scratch holds the native RGB in between.
    void rggb_debayer_scale_raw10_cuda( const uint8_t * src, int src_stride, int native_w, int native_h,
                                        const rggb_isp_params & isp,
                                        uint8_t * dst, int out_w, int out_h );

    // Bilinear remap of an interleaved RGB8 image. sx_dev/sy_dev are DEVICE pointers (out_w*out_h
    // floats each) holding, per output pixel, the source pixel to sample (out-of-range -> black).
    void rggb_remap_rgb8_cuda( const uint8_t * src, int src_w, int src_h, int src_stride,
                               const float * sx_dev, const float * sy_dev, int out_w, int out_h,
                               uint8_t * dst, int dst_stride );

    // cudaMalloc + H2D upload of `bytes` from host; returns the device pointer (nullptr on failure).
    // Used to stage the (constant) remap tables once. Free with rggb_cuda_free.
    void * rggb_cuda_alloc_upload( const void * host, size_t bytes );
    void   rggb_cuda_free( void * dev_ptr );
}

#endif // RS2_USE_CUDA || RS2_USE_HIP
#endif // CUDA_RGGB_CUH
