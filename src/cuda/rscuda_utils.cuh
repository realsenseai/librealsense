#pragma once
#ifdef RS2_USE_CUDA

#include <stdexcept>
#include <string>
#include <memory>
#include <cassert>

// CUDA headers
#include <cuda_runtime.h>

#ifdef _MSC_VER
// Add library dependencies if using VS
#pragma comment(lib, "cudart_static")
#endif

// Throws std::runtime_error with a descriptive message if a CUDA call returns non-success.
#define RS_CUDA_CHECK(expr) do {                                                                     \
    cudaError_t _rs_cuda_err = (expr);                                                               \
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
        cudaPointerAttributes attr{};
        if (host && cudaPointerGetAttributes(&attr, host) == cudaSuccess)
        {
            if (attr.type == cudaMemoryTypeManaged)
                return static_cast<T*>(const_cast<void*>(host));
            if (attr.devicePointer)
                return static_cast<T*>(attr.devicePointer);
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
#endif //RS2_USE_CUDA
