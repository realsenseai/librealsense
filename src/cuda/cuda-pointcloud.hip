// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 Intel Corporation. All Rights Reserved.

// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifdef RS2_USE_CUDA

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
//void kernel_deproject_depth_cuda(float * points, const rs2_intrinsics & intrin, const uint16_t * depth, std::function<uint16_t(float)> map_depth)

void kernel_deproject_depth_cuda(float * points, const rs2_intrinsics* intrin, const uint16_t * depth, float depth_scale)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i >= (*intrin).height * (*intrin).width) {
        return;
    }
    int stride = blockDim.x * gridDim.x;
    int a, b;

    for (int j = i; j < (*intrin).height * (*intrin).width; j += stride) {
        b = j / (*intrin).width;
        a = j - b * (*intrin).width;
        const float pixel[] = { (float)a, (float)b };
        deproject_pixel_to_point_cuda(points + j * 3, intrin, pixel, depth_scale * depth[j]);
   }
}


void rscuda::deproject_depth_cuda(float * points, const rs2_intrinsics & intrin, const uint16_t * depth, float depth_scale)
{
    int count = intrin.height * intrin.width;
    // Ceiling division: avoid numBlocks == 0 when count < RS2_CUDA_THREADS_PER_BLOCK.
    int numBlocks = (count + RS2_CUDA_THREADS_PER_BLOCK - 1) / RS2_CUDA_THREADS_PER_BLOCK;

    // Use the RAII allocator from rscuda_utils.cuh so device memory is
    // automatically released on any exception thrown by RS_CUDA_CHECK
    // (the previous raw hipMalloc + manual hipFree code leaked on error
    // paths and required a `if (result);` warning-suppression hack).
    auto d_points = alloc_dev<float>(count * 3);
    auto d_depth  = alloc_dev<uint16_t>(count);
    auto d_intrin = alloc_dev<rs2_intrinsics>(1);

    RS_CUDA_CHECK(hipMemcpy(d_depth.get(), depth, count * sizeof(uint16_t), hipMemcpyHostToDevice));
    RS_CUDA_CHECK(hipMemcpy(d_intrin.get(), &intrin, sizeof(rs2_intrinsics), hipMemcpyHostToDevice));

    kernel_deproject_depth_cuda<<<numBlocks, RS2_CUDA_THREADS_PER_BLOCK>>>(d_points.get(), d_intrin.get(), d_depth.get(), depth_scale);

    // Kernel launches are asynchronous on HIP: synchronise before reading
    // the output back to the host, otherwise hipMemcpy may race with an
    // unfinished kernel and return stale data.
    RS_CUDA_CHECK(hipStreamSynchronize(0));
    RS_CUDA_CHECK(hipGetLastError());

    RS_CUDA_CHECK(hipMemcpy(points, d_points.get(), count * sizeof(float) * 3, hipMemcpyDeviceToHost));
}

#endif
