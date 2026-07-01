// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

// Allocator used for the frame pixel buffer (frame::data).
//
// Default builds (and CUDA builds without zero-copy): this is exactly
// std::allocator<uint8_t>, so std::vector<uint8_t, frame_data_allocator> is the same
// type as the original std::vector<uint8_t> -- zero behavioral or ABI change.
//
// Zero-copy builds (RS2_USE_CUDA_ZEROCOPY): frame buffers are routed through
// rs_frame_zc_alloc/free, which hand out CUDA pinned+mapped memory on an integrated
// GPU (so the GPU kernels can read/write them in place) and plain malloc everywhere
// else. The pointer remains a valid CPU pointer in all cases.

#if defined( RS2_USE_CUDA_ZEROCOPY )

#include "../cuda/cuda-frame-memory.h"

namespace librealsense {

template< class T >
struct cuda_zc_allocator
{
    using value_type = T;

    cuda_zc_allocator() noexcept = default;
    template< class U >
    cuda_zc_allocator( const cuda_zc_allocator< U > & ) noexcept {}

    T * allocate( std::size_t n )
    {
        if( void * p = rs_frame_zc_alloc( n * sizeof( T ) ) )
            return static_cast< T * >( p );
        throw std::bad_alloc();
    }

    void deallocate( T * p, std::size_t ) noexcept { rs_frame_zc_free( p ); }
};

// Stateless and always-equal: vectors may be freely moved between instances.
template< class T, class U >
bool operator==( const cuda_zc_allocator< T > &, const cuda_zc_allocator< U > & ) noexcept { return true; }
template< class T, class U >
bool operator!=( const cuda_zc_allocator< T > &, const cuda_zc_allocator< U > & ) noexcept { return false; }

using frame_data_allocator = cuda_zc_allocator< uint8_t >;

}  // namespace librealsense

#else  // !RS2_USE_CUDA_ZEROCOPY

namespace librealsense {
using frame_data_allocator = std::allocator< uint8_t >;
}  // namespace librealsense

#endif

namespace librealsense {

// Move (or, under zero-copy, copy) a plain std::vector<uint8_t> into a frame's data buffer.
// A few paths (rosbag/DDS readers) deserialize into a default-allocator vector and hand it
// to a frame. In default builds frame::data has the same type, so this is a true move with
// no copy -- behavior unchanged. In zero-copy builds frame::data uses a different allocator,
// so the buffer cannot be stolen and we copy element-wise into the pinned frame buffer.
inline void assign_frame_data( std::vector< uint8_t, frame_data_allocator > & dst,
                               std::vector< uint8_t > && src )
{
#ifdef RS2_USE_CUDA_ZEROCOPY
    dst.assign( src.begin(), src.end() );
#else
    dst = std::move( src );
#endif
}

// Overload for const sources (e.g. const message buffers). Always a copy -- which matches
// prior behavior, since `dst = std::move(const_vec)` already fell back to copy assignment.
inline void assign_frame_data( std::vector< uint8_t, frame_data_allocator > & dst,
                               const std::vector< uint8_t > & src )
{
#ifdef RS2_USE_CUDA_ZEROCOPY
    dst.assign( src.begin(), src.end() );
#else
    dst = src;
#endif
}

}  // namespace librealsense
