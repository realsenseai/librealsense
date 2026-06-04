// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 Intel Corporation. All Rights Reserved.
//
// This file is partly based on Mehran Maghoumi's work:
// https://github.com/Maghoumi/culibrealsense

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

#include "cuda-conversion.cuh"
#include <iostream>
#include <iomanip>
#include "rscuda_utils.cuh"
/*
// conversion to Y8 is currently not available in the API
__global__ void kernel_unpack_yuy2_y8_cuda(const uint8_t * src, uint8_t *dst, int superPixCount)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i >= superPixCount)
        return;

    int idx = i * 4;

    dst[idx] = src[idx];
    dst[idx + 1] = src[idx + 2];
    dst[idx + 2] = src[idx + 4];
    dst[idx + 3] = src[idx + 6];
    dst[idx + 4] = src[idx + 8];
    dst[idx + 5] = src[idx + 10];
    dst[idx + 6] = src[idx + 12];
    dst[idx + 7] = src[idx + 14];
    dst[idx + 8] = src[idx + 16];
    dst[idx + 9] = src[idx + 18];
    dst[idx + 10] = src[idx + 20];
    dst[idx + 11] = src[idx + 22];
    dst[idx + 12] = src[idx + 24];
    dst[idx + 13] = src[idx + 26];
    dst[idx + 14] = src[idx + 28];
    dst[idx + 15] = src[idx + 30];
}
*/

__global__ void kernel_unpack_yuy2_y16_cuda(const uint8_t * src, uint8_t *dst, int superPixCount)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    if (i >= superPixCount)
        return;

    for (; i < superPixCount; i += stride) {

        int idx = i * 4;

        dst[idx] = 0;
        dst[idx + 1] = src[idx + 0];
        dst[idx + 2] = 0;
        dst[idx + 3] = src[idx + 2];
    }
}


__global__ void kernel_unpack_yuy2_rgb8_cuda(const uint8_t * src, uint8_t *dst, int superPixCount)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    if (i >= superPixCount)
        return;

    for (; i < superPixCount; i += stride) {

        int idx = i * 4;

        uint8_t y0 = src[idx];
        uint8_t u0 = src[idx + 1];
        uint8_t y1 = src[idx + 2];
        uint8_t v0 = src[idx + 3];

        int16_t c = y0 - 16;
        int16_t d = u0 - 128;
        int16_t e = v0 - 128;

        int32_t t;
#define clamp(x)  ((t=(x)) > 255 ? 255 : t < 0 ? 0 : t)

        int odx = i * 6;

        dst[odx] = clamp((298 * c + 409 * e + 128) >> 8);
        dst[odx + 1] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
        dst[odx + 2] = clamp((298 * c + 516 * d + 128) >> 8);

        c = y1 - 16;

        dst[odx + 3] = clamp((298 * c + 409 * e + 128) >> 8);
        dst[odx + 4] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
        dst[odx + 5] = clamp((298 * c + 516 * d + 128) >> 8);

#undef clamp

    }
}

__global__ void kernel_unpack_yuy2_bgr8_cuda(const uint8_t * src, uint8_t *dst, int superPixCount)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    if (i >= superPixCount)
        return;

    for (; i < superPixCount; i += stride) {

        int idx = i * 4;

        uint8_t y0 = src[idx];
        uint8_t u0 = src[idx + 1];
        uint8_t y1 = src[idx + 2];
        uint8_t v0 = src[idx + 3];

        int16_t c = y0 - 16;
        int16_t d = u0 - 128;
        int16_t e = v0 - 128;

        int32_t t;
#define clamp(x)  ((t=(x)) > 255 ? 255 : t < 0 ? 0 : t)

        int odx = i * 6;

        dst[odx + 2] = clamp((298 * c + 409 * e + 128) >> 8);
        dst[odx + 1] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
        dst[odx] = clamp((298 * c + 516 * d + 128) >> 8);

        c = y1 - 16;

        dst[odx + 5] = clamp((298 * c + 409 * e + 128) >> 8);
        dst[odx + 4] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
        dst[odx + 3] = clamp((298 * c + 516 * d + 128) >> 8);

#undef clamp
    }
}


__global__ void kernel_unpack_yuy2_rgba8_cuda(const uint8_t * src, uint8_t *dst, int superPixCount)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    if (i >= superPixCount)
        return;

    for (; i < superPixCount; i += stride) {

        int idx = i * 4;

        uint8_t y0 = src[idx];
        uint8_t u0 = src[idx + 1];
        uint8_t y1 = src[idx + 2];
        uint8_t v0 = src[idx + 3];

        int16_t c = y0 - 16;
        int16_t d = u0 - 128;
        int16_t e = v0 - 128;

        int32_t t;
#define clamp(x)  ((t=(x)) > 255 ? 255 : t < 0 ? 0 : t)

        int odx = i * 8;

        dst[odx] = clamp((298 * c + 409 * e + 128) >> 8);
        dst[odx + 1] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
        dst[odx + 2] = clamp((298 * c + 516 * d + 128) >> 8);
        dst[odx + 3] = 255;

        c = y1 - 16;

        dst[odx + 4] = clamp((298 * c + 409 * e + 128) >> 8);
        dst[odx + 5] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
        dst[odx + 6] = clamp((298 * c + 516 * d + 128) >> 8);
        dst[odx + 7] = 255;

#undef clamp
    }
}

__global__ void kernel_unpack_yuy2_bgra8_cuda(const uint8_t * src, uint8_t *dst, int superPixCount)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    if (i >= superPixCount)
        return;

    for (; i < superPixCount; i += stride) {

        int idx = i * 4;

        uint8_t y0 = src[idx];
        uint8_t u0 = src[idx + 1];
        uint8_t y1 = src[idx + 2];
        uint8_t v0 = src[idx + 3];

        int16_t c = y0 - 16;
        int16_t d = u0 - 128;
        int16_t e = v0 - 128;

        int32_t t;

#define clamp(x)  ((t=(x)) > 255 ? 255 : t < 0 ? 0 : t)

        int odx = i * 8;

        dst[odx + 3] = 255;
        dst[odx + 2] = clamp((298 * c + 409 * e + 128) >> 8);
        dst[odx + 1] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
        dst[odx] = clamp((298 * c + 516 * d + 128) >> 8);

        c = y1 - 16;

        dst[odx + 7] = 255;
        dst[odx + 6] = clamp((298 * c + 409 * e + 128) >> 8);
        dst[odx + 5] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
        dst[odx + 4] = clamp((298 * c + 516 * d + 128) >> 8);

#undef clamp
    }
}

__global__ void kernel_uyvy_to_yuyv_cuda(uint16_t* src, uint16_t* dst, int count)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i >= count)
        return;

    dst[i] = ( ( src[i] >> 8 ) & 0x00FF ) | ( ( src[i] << 8 ) & 0xFF00 );
}

void rscuda::uyvy_to_yuyv_cuda_helper(const uint16_t* src, uint16_t* dst, int count)
{
    // Ceiling division: avoid numBlocks == 0 when count < RS2_CUDA_THREADS_PER_BLOCK,
    // which would launch the kernel with 0 blocks and leave d_dst uninitialised.
    int numBlocks = (count + RS2_CUDA_THREADS_PER_BLOCK - 1) / RS2_CUDA_THREADS_PER_BLOCK;
    auto d_src = alloc_dev<uint16_t>(count);
    auto d_dst = alloc_dev<uint16_t>(count);

    RS_CUDA_CHECK(hipMemcpy(d_src.get(), src, count * sizeof(uint16_t), hipMemcpyHostToDevice));

    kernel_uyvy_to_yuyv_cuda <<<numBlocks, RS2_CUDA_THREADS_PER_BLOCK>>>(d_src.get(), d_dst.get(), count);
    RS_CUDA_CHECK(hipStreamSynchronize(0));
    RS_CUDA_CHECK(hipGetLastError());

    RS_CUDA_CHECK(hipMemcpy(dst, d_dst.get(), count * sizeof(uint16_t), hipMemcpyDeviceToHost));
}

void rscuda::unpack_yuy2_cuda_helper(const uint8_t* h_src, uint8_t* h_dst, int n, rs2_format format)
{
    /*    hipEvent_t start, stop;
        hipEventCreate(&start);
        hipEventCreate(&stop);
        hipEventRecord(start); */

        // How many super pixels do we have?
    int superPix = n / 2;
    std::shared_ptr<uint8_t> d_dst;
    std::shared_ptr<uint8_t> d_src = alloc_dev<uint8_t>(superPix * 4);

    RS_CUDA_CHECK(hipMemcpy(d_src.get(), h_src, superPix * sizeof(uint8_t) * 4, hipMemcpyHostToDevice));

    int numBlocks = (superPix + RS2_CUDA_THREADS_PER_BLOCK - 1) / RS2_CUDA_THREADS_PER_BLOCK;
    int size;

    switch (format)
    {
        // conversion to Y8 is currently not available in the API
        /*	case RS2_FORMAT_Y8:
            size = 1;
             d_dst = alloc_dev<uint8_t>(n * size);
            kernel_unpack_yuy2_y8_cuda<<<numBlocks, RS2_CUDA_THREADS_PER_BLOCK>>>(devSrc, devDst, superPix);
            break;
        */
    case RS2_FORMAT_Y16:
        size = 2;
        d_dst = alloc_dev<uint8_t>(n * size);
        kernel_unpack_yuy2_y16_cuda <<<numBlocks, RS2_CUDA_THREADS_PER_BLOCK>>> (d_src.get(), d_dst.get(), superPix);
        break;
    case RS2_FORMAT_RGB8:
        size = 3;
        d_dst = alloc_dev<uint8_t>(n * size);
        kernel_unpack_yuy2_rgb8_cuda <<<numBlocks, RS2_CUDA_THREADS_PER_BLOCK>>> (d_src.get(), d_dst.get(), superPix);
        break;
    case RS2_FORMAT_BGR8:
        size = 3;
        d_dst = alloc_dev<uint8_t>(n * size);
        kernel_unpack_yuy2_bgr8_cuda <<<numBlocks, RS2_CUDA_THREADS_PER_BLOCK>>> (d_src.get(), d_dst.get(), superPix);
        break;
    case RS2_FORMAT_RGBA8:
        size = 4;
        d_dst = alloc_dev<uint8_t>(n * size);
        kernel_unpack_yuy2_rgba8_cuda <<<numBlocks, RS2_CUDA_THREADS_PER_BLOCK>>> (d_src.get(), d_dst.get(), superPix);
        break;
    case RS2_FORMAT_BGRA8:
        size = 4;
        d_dst = alloc_dev<uint8_t>(n * size);
        kernel_unpack_yuy2_bgra8_cuda <<<numBlocks, RS2_CUDA_THREADS_PER_BLOCK>>> (d_src.get(), d_dst.get(), superPix);
        break;
    default:
        throw std::runtime_error("unpack_yuy2_cuda_helper: unsupported rs2_format");
    }
    RS_CUDA_CHECK(hipStreamSynchronize(0));
    RS_CUDA_CHECK(hipGetLastError());

    RS_CUDA_CHECK(hipMemcpy(h_dst, d_dst.get(), n * sizeof(uint8_t) * size, hipMemcpyDeviceToHost));

    /*	hipEventRecord(stop);
        hipEventSynchronize(stop);
        float milliseconds = 0;
        hipEventElapsedTime(&milliseconds, start, stop);
        std::cout << milliseconds << "\n"; */
}


__global__ void kernel_split_frame_y8_y8_from_y8i_cuda(uint8_t* a, uint8_t* b, int count, const rscuda::y8i_pixel * source)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i >= count)
        return;

    a[i] = source[i].l;
    b[i] = source[i].r;
}

void rscuda::y8_y8_from_y8i_cuda_helper(uint8_t* const dest[], int count, const rscuda::y8i_pixel * source)
{
    /*    hipEvent_t start, stop;
        hipEventCreate(&start);
        hipEventCreate(&stop);
        hipEventRecord(start); */

    int numBlocks = (count + RS2_CUDA_THREADS_PER_BLOCK - 1) / RS2_CUDA_THREADS_PER_BLOCK;
    uint8_t* a = dest[0];
    uint8_t* b = dest[1];

    auto d_src = alloc_dev<rscuda::y8i_pixel>(count);
    auto d_dst_0 = alloc_dev<uint8_t>(count);
    auto d_dst_1 = alloc_dev<uint8_t>(count);

    RS_CUDA_CHECK(hipMemcpy(d_src.get(), source, count * sizeof(rscuda::y8i_pixel), hipMemcpyHostToDevice));

    kernel_split_frame_y8_y8_from_y8i_cuda <<<numBlocks, RS2_CUDA_THREADS_PER_BLOCK>>> (d_dst_0.get(), d_dst_1.get(), count, d_src.get());
    RS_CUDA_CHECK(hipStreamSynchronize(0));
    RS_CUDA_CHECK(hipGetLastError());

    RS_CUDA_CHECK(hipMemcpy(a, d_dst_0.get(), count * sizeof(uint8_t), hipMemcpyDeviceToHost));
    RS_CUDA_CHECK(hipMemcpy(b, d_dst_1.get(), count * sizeof(uint8_t), hipMemcpyDeviceToHost));

    /*    hipEventRecord(stop);
        hipEventSynchronize(stop);
        float milliseconds = 0;
        hipEventElapsedTime(&milliseconds, start, stop);
        std::cout << milliseconds << std::endl; */
}

__global__ void kernel_split_frame_y8_y8_from_y8i_mipi_cuda(uint8_t* a, uint8_t* b, int count, const rscuda::y8i_pixel_mipi * source)
{
    int pair_index = blockDim.x * blockIdx.x + threadIdx.x;
    int pair_count = count / 2;
    if (pair_index >= pair_count)
        return;

    int i = pair_index * 2;
    
    // temporary workaround - swapping ordering of left, caused due to received frame
    // when fixed we can use the regular kernel_split_frame_y8_y8_from_y8i_cuda
    a[i]     = source[i + 1].l;
    b[i]     = source[i].r;

    a[i + 1] = source[i].l;
    b[i + 1] = source[i + 1].r;
}

void rscuda::y8_y8_from_y8i_mipi_cuda_helper(uint8_t* const dest[], int count, const rscuda::y8i_pixel_mipi * source)
{
    int pair_count = count / 2;
    int numBlocks = (pair_count + RS2_CUDA_THREADS_PER_BLOCK - 1) / RS2_CUDA_THREADS_PER_BLOCK;
    uint8_t* a = dest[0];
    uint8_t* b = dest[1];

    auto d_src = alloc_dev<rscuda::y8i_pixel_mipi>(count);
    auto d_dst_0 = alloc_dev<uint8_t>(count);
    auto d_dst_1 = alloc_dev<uint8_t>(count);

    RS_CUDA_CHECK(hipMemcpy(d_src.get(), source, count * sizeof(rscuda::y8i_pixel_mipi), hipMemcpyHostToDevice));

    kernel_split_frame_y8_y8_from_y8i_mipi_cuda <<<numBlocks, RS2_CUDA_THREADS_PER_BLOCK>>> (d_dst_0.get(), d_dst_1.get(), count, d_src.get());
    RS_CUDA_CHECK(hipStreamSynchronize(0));
    RS_CUDA_CHECK(hipGetLastError());

    RS_CUDA_CHECK(hipMemcpy(a, d_dst_0.get(), count * sizeof(uint8_t), hipMemcpyDeviceToHost));
    RS_CUDA_CHECK(hipMemcpy(b, d_dst_1.get(), count * sizeof(uint8_t), hipMemcpyDeviceToHost));
}


template<class SOURCE>
__global__ void kernel_split_frame_y16_y16_from_y12i_cuda(uint16_t* a, uint16_t* b, int count, const SOURCE * source)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i >= count)
        return;

    a[i] = source[i].l() << 6 | source[i].l() >> 4;
    b[i] = source[i].r() << 6 | source[i].r() >> 4;
}


template<class SOURCE>
void rscuda::y16_y16_from_y12i_10_cuda_helper(uint8_t* const dest[], int count, const SOURCE * source)
{
    /*
        hipEvent_t start, stop;
        hipEventCreate(&start);
        hipEventCreate(&stop);
        hipEventRecord(start); */

    // (Removed no-op `source = reinterpret_cast<const SOURCE*>(source);` --
    //  `source` is already const SOURCE*; the cast was a copy-paste artifact
    //  from a CUDA version that started from void*.)

    int numBlocks = (count + RS2_CUDA_THREADS_PER_BLOCK - 1) / RS2_CUDA_THREADS_PER_BLOCK;
    uint16_t* a = reinterpret_cast<uint16_t*>(dest[0]);
    uint16_t* b = reinterpret_cast<uint16_t*>(dest[1]);

    auto d_src = alloc_dev<SOURCE>(count);
    auto d_dst_0 = alloc_dev<uint16_t>(count);
    auto d_dst_1 = alloc_dev<uint16_t>(count);

    RS_CUDA_CHECK(hipMemcpy(d_src.get(), source, count * sizeof(SOURCE), hipMemcpyHostToDevice));

    kernel_split_frame_y16_y16_from_y12i_cuda <<<numBlocks, RS2_CUDA_THREADS_PER_BLOCK>>> (d_dst_0.get(), d_dst_1.get(), count, d_src.get());
    RS_CUDA_CHECK(hipStreamSynchronize(0));
    RS_CUDA_CHECK(hipGetLastError());

    RS_CUDA_CHECK(hipMemcpy(a, d_dst_0.get(), count * sizeof(uint16_t), hipMemcpyDeviceToHost));
    RS_CUDA_CHECK(hipMemcpy(b, d_dst_1.get(), count * sizeof(uint16_t), hipMemcpyDeviceToHost));

    /*
    hipEventRecord(stop);
    hipEventSynchronize(stop);
    float milliseconds = 0;
    hipEventElapsedTime(&milliseconds, start, stop);
    std::cout << milliseconds << std::endl;
    */
}

template void rscuda::y16_y16_from_y12i_10_cuda_helper<rscuda::y12i_pixel>(uint8_t* const dest[], int count, const rscuda::y12i_pixel * source);


template void rscuda::y16_y16_from_y12i_10_cuda_helper<rscuda::y12i_pixel_mipi>(uint8_t* const dest[], int count, const rscuda::y12i_pixel_mipi * source);

__global__ void kernel_z16_y8_from_sr300_inzi_cuda(const uint16_t* source, uint8_t* const dest, int count)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i >= count)
        return;

    dest[i] = source[i] >> 2;
}

void rscuda::unpack_z16_y8_from_sr300_inzi_cuda(uint8_t * const dest, const uint16_t * source, int count)
{
    /*  hipEvent_t start, stop;
        hipEventCreate(&start);
        hipEventCreate(&stop);
        hipEventRecord(start); */

    auto d_src = alloc_dev<uint16_t>(count);
    auto d_dst = alloc_dev<uint8_t>(count);

    int numBlocks = (count + RS2_CUDA_THREADS_PER_BLOCK - 1) / RS2_CUDA_THREADS_PER_BLOCK;

    RS_CUDA_CHECK(hipMemcpy(d_src.get(), source, count * sizeof(uint16_t), hipMemcpyHostToDevice));

    kernel_z16_y8_from_sr300_inzi_cuda <<<numBlocks, RS2_CUDA_THREADS_PER_BLOCK>>> (d_src.get(), d_dst.get(), count);
    RS_CUDA_CHECK(hipStreamSynchronize(0));
    RS_CUDA_CHECK(hipGetLastError());

    RS_CUDA_CHECK(hipMemcpy(dest, d_dst.get(), count * sizeof(uint8_t), hipMemcpyDeviceToHost));

    /*  hipEventRecord(stop);
        hipEventSynchronize(stop);
        float milliseconds = 0;
        hipEventElapsedTime(&milliseconds, start, stop);
        std::cout << milliseconds << std::endl; */
}

__global__ void kernel_z16_y16_from_sr300_inzi_cuda(uint16_t* const source, uint16_t* const dest, int count)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i >= count)
        return;

    dest[i] = source[i] << 6;
}

void rscuda::unpack_z16_y16_from_sr300_inzi_cuda(uint16_t * const dest, const uint16_t * source, int count)
{
    /*  hipEvent_t start, stop;
        hipEventCreate(&start);
        hipEventCreate(&stop);
        hipEventRecord(start); */

    auto d_src = alloc_dev<uint16_t>(count);
    auto d_dst = alloc_dev<uint16_t>(count);

    int numBlocks = (count + RS2_CUDA_THREADS_PER_BLOCK - 1) / RS2_CUDA_THREADS_PER_BLOCK;

    RS_CUDA_CHECK(hipMemcpy(d_src.get(), source, count * sizeof(uint16_t), hipMemcpyHostToDevice));

    kernel_z16_y16_from_sr300_inzi_cuda <<<numBlocks, RS2_CUDA_THREADS_PER_BLOCK>>> (d_src.get(), d_dst.get(), count);
    RS_CUDA_CHECK(hipStreamSynchronize(0));
    RS_CUDA_CHECK(hipGetLastError());

    RS_CUDA_CHECK(hipMemcpy(dest, d_dst.get(), count * sizeof(uint16_t), hipMemcpyDeviceToHost));

    /*	hipEventRecord(stop);
        hipEventSynchronize(stop);
        float milliseconds = 0;
        hipEventElapsedTime(&milliseconds, start, stop);
        std::cout << milliseconds << std::endl; */
}

#endif
