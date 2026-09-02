// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once
// Guarded on either macro (not just RS2_USE_CUDA) so this compiles under HIP regardless of
// whether global_config.cmake's BUILD_WITH_HIP branch also defines RS2_USE_CUDA (today, for
// back-compat) or drops it in favor of RS2_USE_HIP alone.
#if defined(RS2_USE_CUDA) || defined(RS2_USE_HIP)

#include <stdexcept>
#include <string>
#include <memory>
#include <cassert>

// GPU runtime headers
#ifdef RS2_USE_HIP
#include <hip/hip_runtime.h>
#define cudaMalloc hipMalloc
#define cudaFree hipFree
#define cudaMemcpy hipMemcpy
#define cudaMemcpyHostToDevice hipMemcpyHostToDevice
#define cudaMemcpyDeviceToHost hipMemcpyDeviceToHost
#define cudaSuccess hipSuccess
#define cudaError_t hipError_t
#define cudaGetErrorString hipGetErrorString
#define cudaGetLastError hipGetLastError
#define cudaStreamSynchronize hipStreamSynchronize
#define cudaMemset hipMemset
// Zero-copy pointer-attribute probing (see try_device_ptr() below). Only referenced when
// RS2_USE_CUDA_ZEROCOPY is also defined (BUILD_WITH_HIP_ZEROCOPY); harmless otherwise since
// the whole try_device_ptr() body they appear in is itself compiled out.
#define cudaPointerAttributes hipPointerAttribute_t
#define cudaPointerGetAttributes hipPointerGetAttributes
#define cudaMemoryTypeManaged hipMemoryTypeManaged
#define cudaMemoryTypeHost hipMemoryTypeHost
#define cudaHostGetDevicePointer hipHostGetDevicePointer
#else
#include <cuda_runtime.h>
#ifdef _MSC_VER
// Add library dependencies if using VS.  Gated to the CUDA branch only;
// when building for HIP the linker must not pull in cudart_static.lib.
#pragma comment(lib, "cudart_static")
#endif
#endif

#include "cuda-compat.h"        // RS_CUDA_MEMTYPE — single definition shared across CUDA TUs
#include "cuda-frame-memory.h"  // rs_frame_zc_enabled — the one place zero-copy is switched on

// Throws std::runtime_error with a descriptive message if a CUDA / HIP call returns non-success.
// Uses `auto` for the return value so the same macro compiles for both cudaError_t and hipError_t.
#define RS_CUDA_CHECK(expr) do {                                                                     \
    auto _rs_cuda_err = (expr);                                                                      \
    if (_rs_cuda_err != cudaSuccess)                                                                 \
        throw std::runtime_error(std::string(#expr " failed: ") + cudaGetErrorString(_rs_cuda_err)); \
} while (0)

namespace rscuda
{
    constexpr int THREADS_IN_WARP = 32; // CUDA warp size; constant across all current NVIDIA archs.

    template<typename  T>
    std::shared_ptr<T> alloc_dev(int elements)
    {
        T* d_data;
        auto res = cudaMalloc(&d_data, sizeof(T) * elements);
        if (res != cudaSuccess)
            throw std::runtime_error("cudaMalloc failed status: " + res);
        return std::shared_ptr<T>(d_data, [](T* p) { cudaFree(p); });
    }

    // Zero-copy probe: if `host` is CUDA pinned+mapped memory (frame buffers on an
    // integrated GPU), return the aliasing device pointer so a kernel can read/write it
    // in place with no cudaMemcpy. Returns nullptr otherwise (plain malloc / discrete /
    // non-zero-copy build), signalling the caller to use the cudaMalloc + copy path.
    // Clears the CUDA error state on the not-mapped path so it doesn't leak to RS_CUDA_CHECK.
    template<typename T>
    T* try_device_ptr(const void* host)
    {
#ifdef RS2_USE_CUDA_ZEROCOPY
        // Only probe in zero-copy builds. In a plain CUDA build this compiles to
        // `return nullptr;`, so the existing cudaMalloc + cudaMemcpy path is taken
        // unchanged (no extra per-frame probe, byte-for-byte identical behavior).
        //
        // Handles both memory kinds the zero-copy path produces:
        //   - managed (frame pool, cudaMallocManaged) -> same ptr is device-usable
        //   - host-registered mapped (V4L2 capture buffers) -> attr.devicePointer
        // Unregistered (plain malloc / discrete GPU) -> nullptr -> caller copies.
        //
        // Gate on the same switch rs_frame_zc_alloc() uses, so this agrees with the contract
        // documented on rs_frame_zc_device_ptr(): zero-copy only on an integrated GPU. Without
        // the gate, a discrete-GPU host whose driver reports pageableMemoryAccess (HMM, common
        // on recent Linux drivers) hands back a device pointer for ordinary malloc'd frame
        // memory, and the kernels would stream a whole frame over PCIe every call instead of
        // taking the staging copy that is much faster there.
        if (!librealsense::rs_frame_zc_enabled())
            return nullptr;

        cudaPointerAttributes attr{};
        if (host && cudaPointerGetAttributes(&attr, host) == cudaSuccess)
        {
            if (RS_CUDA_MEMTYPE(attr) == cudaMemoryTypeManaged)
                return static_cast<T*>(const_cast<void*>(host));
            if (attr.devicePointer)
                return static_cast<T*>(attr.devicePointer);
            // Some Jetson L4T CUDA drivers leave attr.devicePointer null for mapped pinned
            // memory even though it IS device-mapped; cudaHostGetDevicePointer resolves the alias.
            if (RS_CUDA_MEMTYPE(attr) == cudaMemoryTypeHost)
            {
                void* dptr = nullptr;
                if (cudaHostGetDevicePointer(&dptr, const_cast<void*>(host), 0) == cudaSuccess && dptr)
                    return static_cast<T*>(dptr);
            }
        }
        cudaGetLastError();
#endif
        (void)host;
        return nullptr;
    }

    template<typename  T>
    std::shared_ptr<T> make_device_copy(T obj)
    {
        T* d_data;
        auto res = cudaMalloc(&d_data, sizeof(T));
        if (res != cudaSuccess)
            throw std::runtime_error("cudaMalloc failed status: " + res);
        cudaMemcpy(d_data, &obj, sizeof(T), cudaMemcpyHostToDevice);
        return std::shared_ptr<T>(d_data, [](T* data) { cudaFree(data); });
    }

    /* Given a point in 3D space, compute the corresponding pixel coordinates in an image with no distortion or forward distortion coefficients produced by the same camera */
    __device__ static void rs2_project_point_to_pixel(float pixel[2], const struct rs2_intrinsics * intrin, const float point[3])
    {
        //assert(intrin->model != RS2_DISTORTION_INVERSE_BROWN_CONRADY); // Cannot project to an inverse-distorted image

        float x = point[0] / point[2], y = point[1] / point[2];

        if (intrin->model == RS2_DISTORTION_MODIFIED_BROWN_CONRADY)
        {

            float r2 = x * x + y * y;
            float f = 1 + intrin->coeffs[0] * r2 + intrin->coeffs[1] * r2*r2 + intrin->coeffs[4] * r2*r2*r2;
            x *= f;
            y *= f;
            float dx = x + 2 * intrin->coeffs[2] * x*y + intrin->coeffs[3] * (r2 + 2 * x*x);
            float dy = y + 2 * intrin->coeffs[3] * x*y + intrin->coeffs[2] * (r2 + 2 * y*y);
            x = dx;
            y = dy;
        }

        if (intrin->model == RS2_DISTORTION_FTHETA)
        {
            float r = sqrtf(x*x + y * y);
            float rd = (float)(1.0f / intrin->coeffs[0] * atan(2 * r* tan(intrin->coeffs[0] / 2.0f)));
            x *= rd / r;
            y *= rd / r;
        }

        pixel[0] = x * intrin->fx + intrin->ppx;
        pixel[1] = y * intrin->fy + intrin->ppy;
    }

    /* Given pixel coordinates and depth in an image with no distortion or inverse distortion coefficients, compute the corresponding point in 3D space relative to the same camera */
    __device__ static void rs2_deproject_pixel_to_point(float point[3], const struct rs2_intrinsics * intrin, const float pixel[2], float depth)
    {
        assert(intrin->model != RS2_DISTORTION_MODIFIED_BROWN_CONRADY); // Cannot deproject from a forward-distorted image
        assert(intrin->model != RS2_DISTORTION_FTHETA); // Cannot deproject to an ftheta image
        //assert(intrin->model != RS2_DISTORTION_BROWN_CONRADY); // Cannot deproject to an brown conrady model

        float x = (pixel[0] - intrin->ppx) / intrin->fx;
        float y = (pixel[1] - intrin->ppy) / intrin->fy;

        if (intrin->model == RS2_DISTORTION_INVERSE_BROWN_CONRADY)
        {
            float r2 = x * x + y * y;
            float f = 1 + intrin->coeffs[0] * r2 + intrin->coeffs[1] * r2*r2 + intrin->coeffs[4] * r2*r2*r2;
            float ux = x * f + 2 * intrin->coeffs[2] * x*y + intrin->coeffs[3] * (r2 + 2 * x*x);
            float uy = y * f + 2 * intrin->coeffs[3] * x*y + intrin->coeffs[2] * (r2 + 2 * y*y);
            x = ux;
            y = uy;
        }
        point[0] = depth * x;
        point[1] = depth * y;
        point[2] = depth;
    }

    /* Transform 3D coordinates relative to one sensor to 3D coordinates relative to another viewpoint */
    __device__ static void rs2_transform_point_to_point(float to_point[3], const struct rs2_extrinsics * extrin, const float from_point[3])
    {
        to_point[0] = extrin->rotation[0] * from_point[0] + extrin->rotation[3] * from_point[1] + extrin->rotation[6] * from_point[2] + extrin->translation[0];
        to_point[1] = extrin->rotation[1] * from_point[0] + extrin->rotation[4] * from_point[1] + extrin->rotation[7] * from_point[2] + extrin->translation[1];
        to_point[2] = extrin->rotation[2] * from_point[0] + extrin->rotation[5] * from_point[1] + extrin->rotation[8] * from_point[2] + extrin->translation[2];
    }
}
#endif //RS2_USE_CUDA || RS2_USE_HIP
