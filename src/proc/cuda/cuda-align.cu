#ifdef RS2_USE_CUDA

#include "cuda-align.cuh"
#include "../../../include/librealsense2/rsutil.h"
#include "../../cuda/rscuda_utils.cuh"

// CUDA headers
#include <cuda_runtime.h>

#ifdef _MSC_VER 
// Add library dependencies if using VS
#pragma comment(lib, "cudart_static")
#endif

using namespace librealsense;
using namespace rscuda;

namespace
{
    constexpr int ALIGN_BLOCK_X = THREADS_IN_WARP; // warp size so a warp's lanes hit consecutive image-x pixels (coalesced reads).
    constexpr int ALIGN_BLOCK_Y = 4;               // 4 chosen empirically, best run times on tested platforms (~12 blocks/SM at 33 regs/thread)
}

template<int N> struct bytes { unsigned char b[N]; };

int calc_block_size(int pixel_count, int thread_count)
{
    return ((pixel_count % thread_count) == 0) ? (pixel_count / thread_count) : (pixel_count / thread_count + 1);
}

__device__ void kernel_transfer_pixels(int2* mapped_pixels, const rs2_intrinsics* depth_intrin,
    const rs2_intrinsics* other_intrin, const rs2_extrinsics* depth_to_other, float depth_val, int depth_x, int depth_y, int block_index)
{
    float shift = block_index ? 0.5 : -0.5;
    auto depth_size = depth_intrin->width * depth_intrin->height;
    auto mapped_index = block_index * depth_size + (depth_y * depth_intrin->width + depth_x);

    if (mapped_index >= depth_size * 2)
        return;

    // Skip over depth pixels with the value of zero, we have no depth data so we will not write anything into our aligned images
    if (depth_val == 0)
    {
        mapped_pixels[mapped_index] = { -1, -1 };
        return;
    }

    //// Map the top-left corner of the depth pixel onto the other image
    float depth_pixel[2] = { depth_x + shift, depth_y + shift }, depth_point[3], other_point[3], other_pixel[2];
    rscuda::rs2_deproject_pixel_to_point(depth_point, depth_intrin, depth_pixel, depth_val);
    rscuda::rs2_transform_point_to_point(other_point, depth_to_other, depth_point);
    rscuda::rs2_project_point_to_pixel(other_pixel, other_intrin, other_point);
    mapped_pixels[mapped_index].x = static_cast<int>(other_pixel[0] + 0.5f);
    mapped_pixels[mapped_index].y = static_cast<int>(other_pixel[1] + 0.5f);
}

__global__  void kernel_map_depth_to_other(int2* mapped_pixels, const uint16_t* depth_in, const rs2_intrinsics* depth_intrin, const rs2_intrinsics* other_intrin,
    const rs2_extrinsics* depth_to_other, float depth_scale)
{
    int depth_x = blockIdx.x * blockDim.x + threadIdx.x;
    int depth_y = blockIdx.y * blockDim.y + threadIdx.y;

    // Bound x and y separately, as the other kernels here do. Testing the flattened index
    // instead lets an out-of-range x wrap onto the next row: at 848x480 the grid is rounded
    // up to 27*32 = 864 columns, so threads x=848..863 of row y alias onto x=0..15 of row
    // y+1 and race the legitimate threads for those pixels, corrupting the pixel map.
    if (depth_x >= depth_intrin->width || depth_y >= depth_intrin->height)
        return;

    int depth_pixel_index = depth_y * depth_intrin->width + depth_x;
    float depth_val = depth_in[depth_pixel_index] * depth_scale;
    kernel_transfer_pixels(mapped_pixels, depth_intrin, other_intrin, depth_to_other, depth_val, depth_x, depth_y, blockIdx.z);
}

template<int BPP>
__global__  void kernel_other_to_depth(unsigned char* aligned, const unsigned char* other, const int2* mapped_pixels, const rs2_intrinsics* depth_intrin, const rs2_intrinsics* other_intrin)
{
    // Cache intrinsic dimensions in registers; the kernel uses them many times (loop bounds, indexing)
    // reading via global pointer each time is inefficient, caching them in registers speeds up the kernel significantly.
    const int depth_w = depth_intrin->width;
    const int depth_h = depth_intrin->height;
    const int other_w = other_intrin->width;
    const int other_h = other_intrin->height;
    const int depth_size = depth_w * depth_h;

    const int depth_x = blockIdx.x * blockDim.x + threadIdx.x;
    const int depth_y = blockIdx.y * blockDim.y + threadIdx.y;
    if (depth_x >= depth_w || depth_y >= depth_h)
        return;

    const int depth_pixel_index = depth_y * depth_w + depth_x;

    const int2 p0 = mapped_pixels[depth_pixel_index];
    const int2 p1 = mapped_pixels[depth_size + depth_pixel_index];

    if (p0.x < 0 || p0.y < 0 || p1.x >= other_w || p1.y >= other_h)
        return;

    // Copy the pixel value from the other image to the aligned output at depth_pixel_index.
    // Originally looped over mapped rectangle but only the last iteration's value (bottom-right corner, p1) survived.
    // Skip the loop and do a single write, guarded by p1 >= p0 to preserve the "no iterations -> no write" edge case.
    if (p1.x >= p0.x && p1.y >= p0.y)
    {
        auto in_other = (const bytes<BPP> *)(other);
        auto out_other = (bytes<BPP> *)(aligned);
        out_other[depth_pixel_index] = in_other[p1.y * other_w + p1.x];
    }
}

__global__  void kernel_depth_to_other(uint16_t* aligned_out, const uint16_t* depth_in, const int2* mapped_pixels, const rs2_intrinsics* depth_intrin, const rs2_intrinsics* other_intrin)
{
    // Cache intrinsic dimensions in registers (see kernel_other_to_depth for rationale).
    const int depth_w = depth_intrin->width;
    const int depth_h = depth_intrin->height;
    const int other_w = other_intrin->width;
    const int other_h = other_intrin->height;
    const int depth_size = depth_w * depth_h;

    const int depth_x = blockIdx.x * blockDim.x + threadIdx.x;
    const int depth_y = blockIdx.y * blockDim.y + threadIdx.y;
    if (depth_x >= depth_w || depth_y >= depth_h)
        return;

    const int depth_pixel_index = depth_y * depth_w + depth_x;

    const int2 p0 = mapped_pixels[depth_pixel_index];
    const int2 p1 = mapped_pixels[depth_size + depth_pixel_index];

    if (p0.x < 0 || p0.y < 0 || p1.x >= other_w || p1.y >= other_h)
        return;

    // Pack the 16-bit depth value into both halves of a 32-bit word once (out of the loop)
    unsigned int new_val = depth_in[depth_pixel_index];
    new_val = (new_val << 16) | new_val;
    unsigned int* arr = (unsigned int*)aligned_out;

    // Two consecutive x positions share same uint32 (idx = (y*w + x) / 2).
    // Iterating by uint32 index (not over uint16 x) saves redundant atomicMin calls.
    for (int y = p0.y; y <= p1.y; ++y)
    {
        const int row_base = y * other_w;
        const int start_idx = (row_base + p0.x) / 2;
        const int end_idx   = (row_base + p1.x) / 2;
        for (int idx = start_idx; idx <= end_idx; ++idx)
        {
            atomicMin(&arr[idx], new_val);
        }
    }
}

// Turns the 0xffff "untouched" sentinel left by the atomicMin scatter into 0, and delivers the
// result to `dst`. dst == src is the in-place case (staging path, identical to what this kernel
// always did); on the zero-copy path dst is the mapped frame buffer, which folds what would
// otherwise be a separate D2H copy of the whole image into this pass.
__global__  void kernel_replace_to_zero(uint16_t* dst, const uint16_t* src, const rs2_intrinsics* other_intrin)
{
    const int other_w = other_intrin->width;
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    // Guard both axes: the grid is rounded up to whole blocks, so without this the tail threads
    // write past the end of the last row -- out of bounds for the image, and on the zero-copy
    // path that lands in the frame buffer's memory rather than cudaMalloc slack.
    if (x >= other_w || y >= other_intrin->height)
        return;

    const int other_pixel_index = y * other_w + x;
    const uint16_t v = src[other_pixel_index];
    dst[other_pixel_index] = (v == 0xffff) ? 0 : v;
}

void align_cuda_helper::align_other_to_depth(unsigned char* h_aligned_out, const uint16_t* h_depth_in,
    float depth_scale, const rs2_intrinsics& h_depth_intrin, const rs2_extrinsics& h_depth_to_other,
    const rs2_intrinsics& h_other_intrin, const unsigned char* h_other_in, rs2_format other_format, int other_bytes_per_pixel)
{
    int depth_pixel_count = h_depth_intrin.width * h_depth_intrin.height;
    int other_pixel_count = h_other_intrin.width * h_other_intrin.height;
    int depth_size = depth_pixel_count * 2;
    int other_size = other_pixel_count * other_bytes_per_pixel;
    int aligned_pixel_count = depth_pixel_count;
    int aligned_size = aligned_pixel_count * other_bytes_per_pixel;

    // Zero-copy fast path. Every buffer this direction touches is streamed exactly once --
    // depth and colour are read once each, the output is written once with a plain store --
    // so there is no reuse for a cache to capture and the uncached mapped access the GPU gets
    // on Tegra costs nothing here. try_device_ptr returns nullptr for unmapped memory
    // (plain malloc / discrete GPU / non-zero-copy build), which keeps the staging path below.
    uint16_t*      depth_dev   = try_device_ptr<uint16_t>(h_depth_in);
    unsigned char* other_dev   = try_device_ptr<unsigned char>(h_other_in);
    unsigned char* aligned_dev = try_device_ptr<unsigned char>(h_aligned_out);
    const bool aligned_mapped = (aligned_dev != nullptr);

    // allocate and copy objects to cuda device memory
    if (!_d_depth_intrinsics) _d_depth_intrinsics = make_device_copy(h_depth_intrin);
    if (!_d_other_intrinsics) _d_other_intrinsics = make_device_copy(h_other_intrin);
    if (!_d_depth_other_extrinsics) _d_depth_other_extrinsics = make_device_copy(h_depth_to_other);

    if (!depth_dev)
    {
        if (!_d_depth_in) _d_depth_in = alloc_dev<uint16_t>(aligned_pixel_count);
        depth_dev = _d_depth_in.get();
        RS_CUDA_CHECK(cudaMemcpy(depth_dev, h_depth_in, depth_size, cudaMemcpyHostToDevice));
    }

    if (!other_dev)
    {
        if (!_d_other_in) _d_other_in = alloc_dev<unsigned char>(other_size);
        other_dev = _d_other_in.get();
        RS_CUDA_CHECK(cudaMemcpy(other_dev, h_other_in, other_size, cudaMemcpyHostToDevice));
    }

    if (!aligned_dev)
    {
        if (!_d_aligned_out) _d_aligned_out = alloc_dev<unsigned char>(aligned_size);
        aligned_dev = _d_aligned_out.get();
    }
    // Clears whichever buffer the kernel writes: the staging buffer, or the frame itself when
    // mapped. This is what leaves unmapped pixels zeroed, so the caller need not pre-clear.
    RS_CUDA_CHECK(cudaMemset(aligned_dev, 0, aligned_size));

    if (!_d_pixel_map) _d_pixel_map = alloc_dev<int2>(depth_pixel_count * 2);

    dim3 block(ALIGN_BLOCK_X, ALIGN_BLOCK_Y);
    dim3 depth_blocks(calc_block_size(h_depth_intrin.width, block.x), calc_block_size(h_depth_intrin.height, block.y));
    dim3 mapping_blocks(depth_blocks.x, depth_blocks.y, 2);

    kernel_map_depth_to_other <<<mapping_blocks,block>>> (_d_pixel_map.get(), depth_dev, _d_depth_intrinsics.get(), _d_other_intrinsics.get(),
        _d_depth_other_extrinsics.get(), depth_scale);

    switch (other_bytes_per_pixel)
    {
    case 1: kernel_other_to_depth<1> <<<depth_blocks,block>>> (aligned_dev, other_dev, _d_pixel_map.get(), _d_depth_intrinsics.get(), _d_other_intrinsics.get()); break;
    case 2: kernel_other_to_depth<2> <<<depth_blocks,block>>> (aligned_dev, other_dev, _d_pixel_map.get(), _d_depth_intrinsics.get(), _d_other_intrinsics.get()); break;
    case 3: kernel_other_to_depth<3> <<<depth_blocks,block>>> (aligned_dev, other_dev, _d_pixel_map.get(), _d_depth_intrinsics.get(), _d_other_intrinsics.get()); break;
    case 4: kernel_other_to_depth<4> <<<depth_blocks,block>>> (aligned_dev, other_dev, _d_pixel_map.get(), _d_depth_intrinsics.get(), _d_other_intrinsics.get()); break;
    }
    RS_CUDA_CHECK(cudaGetLastError());

    if (aligned_mapped)
        // The kernel wrote the frame buffer directly; just make the writes visible to the CPU
        // consumer downstream.
        RS_CUDA_CHECK(cudaStreamSynchronize(0));
    else
        // cudaMemcpy on the null stream is ordered after the kernels and blocks until done,
        // so it subsumes the synchronize.
        RS_CUDA_CHECK(cudaMemcpy(h_aligned_out, aligned_dev, aligned_size, cudaMemcpyDeviceToHost));
}

void align_cuda_helper::align_depth_to_other(unsigned char* h_aligned_out, const uint16_t* h_depth_in,
    float depth_scale, const rs2_intrinsics& h_depth_intrin, const rs2_extrinsics& h_depth_to_other,
    const rs2_intrinsics& h_other_intrin)
{
    int depth_pixel_count = h_depth_intrin.width * h_depth_intrin.height;
    int other_pixel_count = h_other_intrin.width * h_other_intrin.height;
    int aligned_pixel_count = other_pixel_count;

    int depth_byte_size = depth_pixel_count * 2;
    int aligned_byte_size = aligned_pixel_count * 2;

    // Zero-copy fast path for the input: depth is read once per kernel and coalesced, so the
    // upload buys nothing that mapped memory does not already give us.
    //
    // The output deliberately does NOT alias the frame buffer. kernel_depth_to_other scatters
    // into it with atomicMin, once per pixel of every mapped rectangle, and on Tegra a mapped
    // frame buffer is uncached host memory (see rs_frame_zc_alloc) -- system-scope atomics
    // into it would cost far more than the D2H copy being saved. So the scatter always runs
    // against real device memory and kernel_replace_to_zero, a pass we run anyway, delivers
    // the finished image: straight into the frame buffer when it is mapped, in place otherwise.
    uint16_t* depth_dev   = try_device_ptr<uint16_t>(h_depth_in);
    uint16_t* aligned_dev = try_device_ptr<uint16_t>(h_aligned_out);
    const bool aligned_mapped = (aligned_dev != nullptr);

    // allocate and copy objects to cuda device memory
    if (!_d_depth_intrinsics) _d_depth_intrinsics = make_device_copy(h_depth_intrin);
    if (!_d_other_intrinsics) _d_other_intrinsics = make_device_copy(h_other_intrin);
    if (!_d_depth_other_extrinsics) _d_depth_other_extrinsics = make_device_copy(h_depth_to_other);

    if (!depth_dev)
    {
        if (!_d_depth_in) _d_depth_in = alloc_dev<uint16_t>(depth_pixel_count);
        depth_dev = _d_depth_in.get();
        RS_CUDA_CHECK(cudaMemcpy(depth_dev, h_depth_in, depth_byte_size, cudaMemcpyHostToDevice));
    }

    if (!_d_aligned_out) _d_aligned_out = alloc_dev<unsigned char>(aligned_byte_size);
    uint16_t* scatter_dev = (uint16_t*)_d_aligned_out.get();
    RS_CUDA_CHECK(cudaMemset(scatter_dev, 0xff, aligned_byte_size));

    if (!_d_pixel_map) _d_pixel_map = alloc_dev<int2>(depth_pixel_count * 2);

    dim3 block(ALIGN_BLOCK_X, ALIGN_BLOCK_Y);
    dim3 depth_blocks(calc_block_size(h_depth_intrin.width, block.x), calc_block_size(h_depth_intrin.height, block.y));
    dim3 other_blocks(calc_block_size(h_other_intrin.width, block.x), calc_block_size(h_other_intrin.height, block.y));
    dim3 mapping_blocks(depth_blocks.x, depth_blocks.y, 2);

    kernel_map_depth_to_other <<<mapping_blocks,block>>> (_d_pixel_map.get(), depth_dev, _d_depth_intrinsics.get(),
        _d_other_intrinsics.get(), _d_depth_other_extrinsics.get(), depth_scale);

    kernel_depth_to_other <<<depth_blocks,block>>> (scatter_dev, depth_dev, _d_pixel_map.get(),
        _d_depth_intrinsics.get(), _d_other_intrinsics.get());

    kernel_replace_to_zero <<<other_blocks,block>>> (aligned_mapped ? aligned_dev : scatter_dev, scatter_dev,
        _d_other_intrinsics.get());
    RS_CUDA_CHECK(cudaGetLastError());

    if (aligned_mapped)
        RS_CUDA_CHECK(cudaStreamSynchronize(0));
    else
        RS_CUDA_CHECK(cudaMemcpy(h_aligned_out, scatter_dev, aligned_byte_size, cudaMemcpyDeviceToHost));
}

#endif //RS2_USE_CUDA
