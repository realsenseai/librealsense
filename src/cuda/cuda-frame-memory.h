// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#pragma once

#include <cstddef>

// Zero-copy frame memory helpers (Layer C).
//
// These wrap CUDA pinned/mapped allocation behind a plain C++ interface so that
// host translation units (frame.h / the frame archive) can use them WITHOUT pulling
// in cuda_runtime.h. The definitions live in cuda-frame-memory.cu (compiled by nvcc,
// only under BUILD_WITH_CUDA).
//
// Behavior is decided once per process by rs_frame_zc_enabled():
//   * zero-copy build (RS2_USE_CUDA_ZEROCOPY) AND an integrated GPU  -> pinned+mapped
//   * anything else                                                 -> plain malloc
// allocate and free are therefore always symmetric.

namespace librealsense {

// True only when this is a zero-copy build AND the runtime GPU is integrated (Jetson).
bool rs_frame_zc_enabled();

// Allocate `bytes` for a frame buffer. Returns cudaHostAlloc(cudaHostAllocMapped)
// memory when rs_frame_zc_enabled(), otherwise std::malloc. Never returns nullptr for
// a non-zero request (throws-free: returns nullptr only on genuine allocation failure).
void * rs_frame_zc_alloc( std::size_t bytes );

// Free a buffer obtained from rs_frame_zc_alloc(). Safe on nullptr.
void rs_frame_zc_free( void * p );

// For a host pointer returned by rs_frame_zc_alloc() under zero-copy, return the GPU
// device pointer aliasing the same memory (cudaHostGetDevicePointer). Returns nullptr
// when the pointer is not mapped (plain malloc, discrete GPU, or non-zero-copy build),
// in which case the caller must fall back to cudaMalloc + cudaMemcpy.
void * rs_frame_zc_device_ptr( const void * host_ptr );

// Layer B - register a V4L2 (or any externally-mmap'd) capture buffer with CUDA so the
// GPU can read it directly via cudaHostGetDevicePointer. Registered with
// cudaHostRegisterMapped. Returns true on success. No-op returning false unless this is a
// zero-copy build on an integrated GPU. Must be paired with rs_v4l2_zc_unregister before
// the buffer is munmap'd/freed. Idempotent-safe: a failed register simply leaves the
// buffer unmapped to the GPU, so the pipeline falls back to the copy path.
bool rs_v4l2_zc_register( void * ptr, std::size_t len );
void rs_v4l2_zc_unregister( void * ptr );

// Upload `bytes` of host frame data to a per-frame cached device buffer and return its device
// pointer. `*cached`/`*capacity` hold the frame's reused device buffer: (re)allocated via
// cudaMalloc only when it must grow, then cudaMemcpy host->device each call. This backs
// get_gpu_data_or_upload() on builds/platforms where true zero-copy isn't available -- it is a
// real copy, just SDK-managed and amortized across the frame pool. Returns nullptr on failure.
void * rs_frame_gpu_upload( void ** cached, std::size_t * capacity, const void * host, std::size_t bytes );

// Free a device buffer obtained from rs_frame_gpu_upload (cudaFree). Safe on nullptr.
void rs_frame_gpu_free( void * buf );

}  // namespace librealsense
