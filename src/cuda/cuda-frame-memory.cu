// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "cuda-frame-memory.h"

#include <cstdlib>

#ifdef RS2_USE_CUDA
#include <cuda_runtime.h>
#include <rsutils/accelerators/gpu.h>
#endif

#ifdef _MSC_VER
#pragma comment(lib, "cudart_static")
#endif

namespace librealsense {

// Trailing slack added to every zero-copy frame buffer. Vectorized CPU consumers (notably
// pointcloud_neon::get_texture_map_neon) process in fixed-width blocks and over-read/write
// a partial tail past the logical end. malloc'd buffers had incidental slack that absorbed
// it; exactly-sized, page-aligned CUDA allocations do not, which faulted at 720p.
// 256 bytes comfortably covers a few float2/float3 NEON blocks (< 128 B).
static constexpr std::size_t RS_ZC_TAIL_PAD = 256;

bool rs_frame_zc_enabled()
{
#if defined( RS2_USE_CUDA ) && defined( RS2_USE_CUDA_ZEROCOPY )
    // Decide once: zero-copy only pays off on an integrated GPU (shared DRAM). On a
    // discrete GPU, mapped host memory is read per-element over PCIe and would be a
    // large regression, so we fall back to plain malloc + the existing copy path.
    static bool const enabled = rsutils::rs2_is_cuda_integrated();
    return enabled;
#else
    return false;
#endif
}

void * rs_frame_zc_alloc( std::size_t bytes )
{
    if( bytes == 0 )
        return nullptr;

#if defined( RS2_USE_CUDA ) && defined( RS2_USE_CUDA_ZEROCOPY )
    if( rs_frame_zc_enabled() )
    {
        void * p = nullptr;
        // Mapped pinned host memory (cudaHostAllocMapped) -- NOT cudaMallocManaged.
        // Managed memory is cached on the GPU (faster for the align atomics) BUT Jetson/Tegra
        // reports cudaDevAttrConcurrentManagedAccess=0, meaning the CPU may NOT touch managed
        // memory while ANY GPU kernel is running. In a multi-stream pipeline (separate capture
        // threads + GPU processing threads) that is unavoidable, and the CPU-side frame
        // zeroing faults (SIGSEGV). Mapped pinned memory is ordinary host memory the CPU can
        // always access concurrently, so it is the only safe zero-copy choice here. It is
        // uncached on the GPU, which is fine for streaming kernels (pointcloud); the align
        // kernels keep their atomic-heavy work on device memory to avoid that penalty.
        if( cudaHostAlloc( &p, bytes + RS_ZC_TAIL_PAD, cudaHostAllocMapped | cudaHostAllocPortable ) == cudaSuccess )
            return p;
        cudaGetLastError();  // clear sticky error before falling back
    }
#endif
    return std::malloc( bytes );
}

void rs_frame_zc_free( void * p )
{
    if( ! p )
        return;

#if defined( RS2_USE_CUDA ) && defined( RS2_USE_CUDA_ZEROCOPY )
    if( rs_frame_zc_enabled() )
    {
        // rs_frame_zc_enabled() is process-constant, so anything allocated while it was
        // true came from cudaHostAlloc and must be released with cudaFreeHost.
        cudaFreeHost( p );
        return;
    }
#endif
    std::free( p );
}

void * rs_frame_zc_device_ptr( const void * host_ptr )
{
#if defined( RS2_USE_CUDA ) && defined( RS2_USE_CUDA_ZEROCOPY )
    if( host_ptr && rs_frame_zc_enabled() )
    {
        // Works for managed (pool frames) AND host-registered mapped (V4L2 buffers):
        //  - managed     -> same pointer is valid on the device (unified addressing)
        //  - host-mapped  -> attr.devicePointer holds the device-side alias
        //  - unregistered -> devicePointer is null -> caller falls back to a copy
        cudaPointerAttributes attr{};
        if( cudaPointerGetAttributes( &attr, host_ptr ) == cudaSuccess )
        {
            if( attr.type == cudaMemoryTypeManaged )
                return const_cast< void * >( host_ptr );
            if( attr.devicePointer )
                return attr.devicePointer;
            // Mapped pinned host memory (cudaHostAllocMapped / cudaHostRegisterMapped): some
            // Jetson L4T CUDA driver versions leave cudaPointerGetAttributes().devicePointer
            // null even though the buffer IS device-mapped (verified on Orin / L4T R36.5).
            // cudaHostGetDevicePointer is the canonical accessor and resolves the alias on
            // those drivers; without this, zero-copy silently degrades to the upload path.
            if( attr.type == cudaMemoryTypeHost )
            {
                void * dptr = nullptr;
                if( cudaHostGetDevicePointer( &dptr, const_cast< void * >( host_ptr ), 0 ) == cudaSuccess && dptr )
                    return dptr;
            }
        }
        cudaGetLastError();  // clear error, caller falls back to copy
    }
#endif
    (void)host_ptr;
    return nullptr;
}

bool rs_v4l2_zc_register( void * ptr, std::size_t len )
{
#if defined( RS2_USE_CUDA ) && defined( RS2_USE_CUDA_ZEROCOPY )
    if( ptr && len && rs_frame_zc_enabled() )
    {
        // Mapped flag so the buffer is reachable from the GPU via cudaHostGetDevicePointer.
        if( cudaHostRegister( ptr, len, cudaHostRegisterMapped ) == cudaSuccess )
            return true;
        cudaGetLastError();  // failure -> GPU falls back to the copy path for this buffer
    }
#endif
    (void)ptr;
    (void)len;
    return false;
}

void rs_v4l2_zc_unregister( void * ptr )
{
#if defined( RS2_USE_CUDA ) && defined( RS2_USE_CUDA_ZEROCOPY )
    if( ptr && rs_frame_zc_enabled() )
    {
        cudaHostUnregister( ptr );
        cudaGetLastError();
    }
#endif
    (void)ptr;
}

void * rs_frame_gpu_upload( void ** cached, std::size_t * capacity, const void * host, std::size_t bytes )
{
#ifdef RS2_USE_CUDA
    if( ! host || ! bytes || ! cached || ! capacity )
        return nullptr;
    if( *capacity < bytes )  // (re)allocate only when the buffer must grow
    {
        if( *cached ) { cudaFree( *cached ); *cached = nullptr; *capacity = 0; }
        void * p = nullptr;
        if( cudaMalloc( &p, bytes ) != cudaSuccess ) { cudaGetLastError(); return nullptr; }
        *cached = p;
        *capacity = bytes;
    }
    if( cudaMemcpy( *cached, host, bytes, cudaMemcpyHostToDevice ) != cudaSuccess )
    {
        cudaGetLastError();
        return nullptr;
    }
    return *cached;
#else
    (void)cached; (void)capacity; (void)host; (void)bytes;
    return nullptr;
#endif
}

void rs_frame_gpu_free( void * buf )
{
#ifdef RS2_USE_CUDA
    if( buf ) { cudaFree( buf ); cudaGetLastError(); }
#endif
    (void)buf;
}

}  // namespace librealsense
