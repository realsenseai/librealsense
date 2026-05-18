#ifdef RS2_USE_CUDA

#include "cuda-align.cuh"
#include "../../../include/librealsense2/rsutil.h"
#include "../../cuda/rscuda_utils.cuh"

#define RS2_CUDA_THREADS_PER_BLOCK 32

using namespace librealsense;
using namespace rscuda;

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

    int depth_pixel_index = depth_y * depth_intrin->width + depth_x;
    if (depth_pixel_index >= depth_intrin->width * depth_intrin->height)
        return;
    float depth_val = depth_in[depth_pixel_index] * depth_scale;
    kernel_transfer_pixels(mapped_pixels, depth_intrin, other_intrin, depth_to_other, depth_val, depth_x, depth_y, blockIdx.z);
}

template<int BPP>
__global__  void kernel_other_to_depth(unsigned char* aligned, const unsigned char* other, const int2* mapped_pixels, const rs2_intrinsics* depth_intrin, const rs2_intrinsics* other_intrin)
{
    int depth_x = blockIdx.x * blockDim.x + threadIdx.x;
    int depth_y = blockIdx.y * blockDim.y + threadIdx.y;

    auto depth_size = depth_intrin->width * depth_intrin->height;
    int depth_pixel_index = depth_y * depth_intrin->width + depth_x;

    if (depth_pixel_index >= depth_intrin->width * depth_intrin->height)
        return;

    int2 p0 = mapped_pixels[depth_pixel_index];
    int2 p1 = mapped_pixels[depth_size + depth_pixel_index];

    if (p0.x < 0 || p0.y < 0 || p1.x >= other_intrin->width || p1.y >= other_intrin->height)
        return;

    // Transfer between the depth pixels and the pixels inside the rectangle on the other image
    auto in_other = (const bytes<BPP> *)(other);
    auto out_other = (bytes<BPP> *)(aligned);
    for (int y = p0.y; y <= p1.y; ++y)
    {
        for (int x = p0.x; x <= p1.x; ++x)
        {
            auto other_pixel_index = y * other_intrin->width + x;
            out_other[depth_pixel_index] = in_other[other_pixel_index];
        }
    }
}

__global__  void kernel_depth_to_other(uint16_t* aligned_out, const uint16_t* depth_in, const int2* mapped_pixels, const rs2_intrinsics* depth_intrin, const rs2_intrinsics* other_intrin)
{
    int depth_x = blockIdx.x * blockDim.x + threadIdx.x;
    int depth_y = blockIdx.y * blockDim.y + threadIdx.y;

    auto depth_size = depth_intrin->width * depth_intrin->height;
    int depth_pixel_index = depth_y * depth_intrin->width + depth_x;

    if (depth_pixel_index >= depth_intrin->width * depth_intrin->height)
        return;

    int2 p0 = mapped_pixels[depth_pixel_index];
    int2 p1 = mapped_pixels[depth_size + depth_pixel_index];

    if (p0.x < 0 || p0.y < 0 || p1.x >= other_intrin->width || p1.y >= other_intrin->height)
        return;

    // Transfer between the depth pixels and the pixels inside the rectangle on the other image
    unsigned int new_val = depth_in[depth_pixel_index];
    unsigned int* arr = (unsigned int*)aligned_out;
    for (int y = p0.y; y <= p1.y; ++y)
    {
        for (int x = p0.x; x <= p1.x; ++x)
        {
            auto other_pixel_index = y * other_intrin->width + x;
            new_val = new_val << 16 | new_val;
            atomicMin(&arr[other_pixel_index / 2], new_val);
        }
    }
}

__global__  void kernel_replace_to_zero(uint16_t* aligned_out, const rs2_intrinsics* other_intrin)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    auto other_pixel_index = y * other_intrin->width + x;
    if (aligned_out[other_pixel_index] == 0xffff)
        aligned_out[other_pixel_index] = 0;
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

    // allocate and copy objects to cuda device memory
    if (!_d_depth_intrinsics) _d_depth_intrinsics = make_device_copy(h_depth_intrin);
    if (!_d_other_intrinsics) _d_other_intrinsics = make_device_copy(h_other_intrin);
    if (!_d_depth_other_extrinsics) _d_depth_other_extrinsics = make_device_copy(h_depth_to_other);

    if (!_d_depth_in) _d_depth_in = alloc_dev<uint16_t>(aligned_pixel_count);
    hipMemcpy(_d_depth_in.get(), h_depth_in, depth_size, hipMemcpyHostToDevice);

    if (!_d_other_in) _d_other_in = alloc_dev<unsigned char>(other_size);
    hipMemcpy(_d_other_in.get(), h_other_in, other_size, hipMemcpyHostToDevice);

    if (!_d_aligned_out)
        _d_aligned_out = alloc_dev<unsigned char>(aligned_size);
    hipMemset(_d_aligned_out.get(), 0, aligned_size);

    if (!_d_pixel_map) _d_pixel_map = alloc_dev<int2>(depth_pixel_count * 2);

    // config threads
    dim3 threads(RS2_CUDA_THREADS_PER_BLOCK, RS2_CUDA_THREADS_PER_BLOCK);
    dim3 depth_blocks(calc_block_size(h_depth_intrin.width, threads.x), calc_block_size(h_depth_intrin.height, threads.y));
    dim3 mapping_blocks(depth_blocks.x, depth_blocks.y, 2);

    kernel_map_depth_to_other <<<mapping_blocks,threads>>> (_d_pixel_map.get(), _d_depth_in.get(), _d_depth_intrinsics.get(), _d_other_intrinsics.get(),
        _d_depth_other_extrinsics.get(), depth_scale);

    switch (other_bytes_per_pixel)
    {
    case 1: kernel_other_to_depth<1> <<<depth_blocks,threads>>> (_d_aligned_out.get(), _d_other_in.get(), _d_pixel_map.get(), _d_depth_intrinsics.get(), _d_other_intrinsics.get()); break;
    case 2: kernel_other_to_depth<2> <<<depth_blocks,threads>>> (_d_aligned_out.get(), _d_other_in.get(), _d_pixel_map.get(), _d_depth_intrinsics.get(), _d_other_intrinsics.get()); break;
    case 3: kernel_other_to_depth<3> <<<depth_blocks,threads>>> (_d_aligned_out.get(), _d_other_in.get(), _d_pixel_map.get(), _d_depth_intrinsics.get(), _d_other_intrinsics.get()); break;
    case 4: kernel_other_to_depth<4> <<<depth_blocks,threads>>> (_d_aligned_out.get(), _d_other_in.get(), _d_pixel_map.get(), _d_depth_intrinsics.get(), _d_other_intrinsics.get()); break;
    }

    hipStreamSynchronize(0);

    hipMemcpy(h_aligned_out, _d_aligned_out.get(), aligned_size, hipMemcpyDeviceToHost);
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

    // allocate and copy objects to cuda device memory
    if (!_d_depth_intrinsics) _d_depth_intrinsics = make_device_copy(h_depth_intrin);
    if (!_d_other_intrinsics) _d_other_intrinsics = make_device_copy(h_other_intrin);
    if (!_d_depth_other_extrinsics) _d_depth_other_extrinsics = make_device_copy(h_depth_to_other);

    if (!_d_depth_in) _d_depth_in = alloc_dev<uint16_t>(depth_pixel_count);
    hipMemcpy(_d_depth_in.get(), h_depth_in, depth_byte_size, hipMemcpyHostToDevice);

    if (!_d_aligned_out) _d_aligned_out = alloc_dev<unsigned char>(aligned_byte_size);
    hipMemset(_d_aligned_out.get(), 0xff, aligned_byte_size);

    if (!_d_pixel_map) _d_pixel_map = alloc_dev<int2>(depth_pixel_count * 2);

    // config threads
    dim3 threads(RS2_CUDA_THREADS_PER_BLOCK, RS2_CUDA_THREADS_PER_BLOCK);
    dim3 depth_blocks(calc_block_size(h_depth_intrin.width, threads.x), calc_block_size(h_depth_intrin.height, threads.y));
    dim3 other_blocks(calc_block_size(h_other_intrin.width, threads.x), calc_block_size(h_other_intrin.height, threads.y));
    dim3 mapping_blocks(depth_blocks.x, depth_blocks.y, 2);

    kernel_map_depth_to_other <<<mapping_blocks,threads>>> (_d_pixel_map.get(), _d_depth_in.get(), _d_depth_intrinsics.get(),
        _d_other_intrinsics.get(), _d_depth_other_extrinsics.get(), depth_scale);

    kernel_depth_to_other <<<depth_blocks,threads>>> ((uint16_t*)_d_aligned_out.get(), _d_depth_in.get(), _d_pixel_map.get(),
        _d_depth_intrinsics.get(), _d_other_intrinsics.get());

    kernel_replace_to_zero <<<other_blocks, threads>>> ((uint16_t*)_d_aligned_out.get(), _d_other_intrinsics.get());

    hipStreamSynchronize(0);

    hipMemcpy(h_aligned_out, _d_aligned_out.get(), aligned_pixel_count * 2, hipMemcpyDeviceToHost);
}

#endif //RS2_USE_CUDA
