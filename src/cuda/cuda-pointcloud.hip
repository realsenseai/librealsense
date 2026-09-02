// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// Guard on either macro (not just RS2_USE_CUDA) so this file keeps compiling under HIP
// regardless of whether global_config.cmake's BUILD_WITH_HIP branch also defines
// RS2_USE_CUDA (today, for back-compat with other RS2_USE_CUDA-only guards) or drops it
// in favor of RS2_USE_HIP alone -- either way this file must still compile under HIP.
#if defined(RS2_USE_CUDA) || defined(RS2_USE_HIP)

#include "cuda-pointcloud.cuh"
#include "rscuda_utils.cuh"
#include <iostream>
#include <chrono>


__device__
float map_depth (float depth_scale, uint16_t z) {
    return depth_scale * z;
}

__device__
void deproject_pixel_to_point_cuda(float points[3], const struct rs2_intrinsics * intrin, const float pixel[2], float depth) {
    assert(intrin->model != RS2_DISTORTION_MODIFIED_BROWN_CONRADY); // Cannot deproject from a forward-distorted image
    assert(intrin->model != RS2_DISTORTION_FTHETA); // Cannot deproject to an ftheta image
    //assert(intrin->model != RS2_DISTORTION_BROWN_CONRADY); // Cannot deproject to an brown conrady model
    float x = (pixel[0] - intrin->ppx) / intrin->fx;
    float y = (pixel[1] - intrin->ppy) / intrin->fy;

    float xo = x;
    float yo = y;

    if (intrin->model == RS2_DISTORTION_INVERSE_BROWN_CONRADY)
    {
        // need to loop until convergence
        // 10 iterations determined empirically
        for (int i = 0; i < 10; i++)
        {
            float r2 = x * x + y * y;
            float icdist = (float)1 / (float)(1 + ((intrin->coeffs[4] * r2 + intrin->coeffs[1])*r2 + intrin->coeffs[0])*r2);
            float xq = x / icdist;
            float yq = y / icdist;
            float delta_x = 2 * intrin->coeffs[2] * xq*yq + intrin->coeffs[3] * (r2 + 2 * xq*xq);
            float delta_y = 2 * intrin->coeffs[3] * xq*yq + intrin->coeffs[2] * (r2 + 2 * yq*yq);
            x = (xo - delta_x)*icdist;
            y = (yo - delta_y)*icdist;
        }
    }
    else if (intrin->model == RS2_DISTORTION_BROWN_CONRADY)
    {
        // need to loop until convergence
        // 10 iterations determined empirically
        for (int i = 0; i < 10; i++)
        {
            float r2 = x * x + y * y;
            float icdist = (float)1 / (float)(1 + ((intrin->coeffs[4] * r2 + intrin->coeffs[1])*r2 + intrin->coeffs[0])*r2);
            float delta_x = 2 * intrin->coeffs[2] * x*y + intrin->coeffs[3] * (r2 + 2 * x*x);
            float delta_y = 2 * intrin->coeffs[3] * x*y + intrin->coeffs[2] * (r2 + 2 * y*y);
            x = (xo - delta_x)*icdist;
            y = (yo - delta_y)*icdist;
        }
    }
    points[0] = depth * x;
    points[1] = depth * y;
    points[2] = depth;

}


__global__
void kernel_deproject_depth_cuda(float * points, const rs2_intrinsics * intrin, const uint16_t * depth, float depth_scale)
{
    const int width  = intrin->width;
    const int height = intrin->height;

    // One thread = one pixel; the grid is sized to the image so no stride loop is needed.
    // Matches the 2D warp-aligned launch added on the CUDA side (cuda-pointcloud.cu) to
    // avoid a per-pixel integer division (b = j / width; a = j - b * width) that the old
    // 1D stride-loop kernel paid on every thread, every launch.
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height)
        return;

    const int j = y * width + x;
    const float pixel[] = { (float)x, (float)y };
    deproject_pixel_to_point_cuda(points + j * 3, intrin, pixel, depth_scale * depth[j]);
}


// Class-based helper: full port of the CUDA-side refactor in cuda-pointcloud.cu, including
// the persistent-buffer reuse and zero-copy fast path (see that file for the detailed design
// rationale). This is a mechanical per-API port: hipMemcpy/hipMalloc/hipGetLastError/
// hipStreamSynchronize in place of the cuda* names; try_device_ptr() and RS_CUDA_MEMTYPE
// resolve to their HIP equivalents via the macros in rscuda_utils.cuh / cuda-compat.h.
// Zero-copy only activates when BUILD_WITH_HIP_ZEROCOPY is enabled (-> RS2_USE_CUDA_ZEROCOPY);
// it is a no-op (try_device_ptr() always returns nullptr) on a plain HIP build, so the
// persistent-buffer path below is exercised unconditionally and zero-copy is strictly
// additive.
void rscuda::pointcloud_cuda_helper::deproject_depth_cuda(float * points, const rs2_intrinsics & intrin, const uint16_t * depth, float depth_scale)
{
    const int count = intrin.width * intrin.height;

    // Zero-copy fast path: when the frame buffers are HIP pinned+mapped (unified-memory AMD
    // APUs), the kernel reads depth and writes points directly in the frame memory -- no
    // H2D/D2H. try_device_ptr() returns nullptr otherwise (discrete GPU / plain build), and
    // we fall back to the persistent staging buffers below.
#ifdef RS2_USE_CUDA_ZEROCOPY
    uint16_t * depth_dev  = rscuda::try_device_ptr<uint16_t>(depth);
    float *    points_dev = rscuda::try_device_ptr<float>(points);
#else
    uint16_t * depth_dev  = nullptr;
    float *    points_dev = nullptr;
#endif
    const bool depth_mapped  = (depth_dev  != nullptr);
    const bool points_mapped = (points_dev != nullptr);

    // (Re)allocate persistent staging buffers only when the frame size changes (and only for
    // the side(s) that are not mapped). On a steady stream this branch runs once.
    if (count != _count)
    {
        _d_points.reset();
        _d_depth.reset();
        _d_intrin.reset();
        _count = count;
    }
    if (!points_mapped)
    {
        if (!_d_points) _d_points = alloc_dev<float>(count * 3);
        points_dev = _d_points.get();
    }
    if (!depth_mapped)
    {
        if (!_d_depth) _d_depth = alloc_dev<uint16_t>(count);
        depth_dev = _d_depth.get();
    }

    // Upload intrinsics once; refresh only if they actually change (e.g. recalibration).
    if (!_d_intrin || std::memcmp(&_intrin_cached, &intrin, sizeof(rs2_intrinsics)) != 0)
    {
        _d_intrin = make_device_copy(intrin);
        _intrin_cached = intrin;
    }

    if (!depth_mapped)
        RS_CUDA_CHECK(hipMemcpy(depth_dev, depth, count * sizeof(uint16_t), hipMemcpyHostToDevice));

    // 2D launch: wavefront-sized x dimension keeps a wavefront's lanes aligned with
    // consecutive pixels, so depth reads stay coalesced. THREADS_IN_WARP == 32 is also
    // RDNA's default wave32 front-end size on the gfx1100/gfx1101 targets this PR builds
    // for (CDNA's 64-wide wavefronts are still handled correctly, just not maximally
    // coalesced -- a tuning follow-up, not a correctness gap).
    const dim3 block(rscuda::THREADS_IN_WARP, pointcloud_cuda_helper::THREADS_PER_BLOCK / rscuda::THREADS_IN_WARP);
    const dim3 grid((intrin.width  + block.x - 1) / block.x,
                     (intrin.height + block.y - 1) / block.y);
    kernel_deproject_depth_cuda<<<grid, block>>>(points_dev, _d_intrin.get(), depth_dev, depth_scale);
    RS_CUDA_CHECK(hipGetLastError());

    if (points_mapped)
        // Mapped output: no D2H copy, but the CPU consumer downstream must see the kernel's
        // writes, so block until the kernel completes.
        RS_CUDA_CHECK(hipStreamSynchronize(0));
    else
        RS_CUDA_CHECK(hipMemcpy(points, _d_points.get(), count * sizeof(float) * 3, hipMemcpyDeviceToHost));
}

#endif
