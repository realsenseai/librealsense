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

void kernel_deproject_depth_cuda( float * points, const rs2_intrinsics * intrin, const uint16_t * depth, float depth_scale )
{
    const int width  = intrin->width;
    const int height = intrin->height;

    // One thread = one pixel; the grid is sized to the image so no stride loop is needed.
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    if( x >= width || y >= height )
        return;

    const int j = y * width + x;
    const float pixel[] = { (float)x, (float)y };
    deproject_pixel_to_point_cuda( points + j * 3, intrin, pixel, depth_scale * depth[j] );
}


void rscuda::pointcloud_cuda_helper::deproject_depth_cuda( float * points, const rs2_intrinsics & intrin,
                                                           const uint16_t * depth, float depth_scale )
{
    const int count = intrin.width * intrin.height;

    // Zero-copy fast path: when the frame buffers are CUDA pinned+mapped (integrated GPU),
    // the kernel reads depth and writes points directly in the frame memory -- no H2D/D2H.
    // try_device_ptr returns nullptr otherwise, and we fall back to the staging buffers.
    // Gated to zero-copy builds so a plain CUDA build keeps the original staging path
    // verbatim (no per-frame cudaPointerGetAttributes probing).
#ifdef RS2_USE_CUDA_ZEROCOPY
    uint16_t * depth_dev  = rscuda::try_device_ptr<uint16_t>( depth );
    float *    points_dev = rscuda::try_device_ptr<float>( points );
#else
    uint16_t * depth_dev  = nullptr;
    float *    points_dev = nullptr;
#endif
    const bool depth_mapped  = ( depth_dev  != nullptr );
    const bool points_mapped = ( points_dev != nullptr );

    // (Re)allocate persistent staging buffers only when the frame size changes (and only
    // for the side(s) that are not mapped). On a steady stream this branch runs once.
    if( count != _count )
    {
        _d_points.reset();
        _d_depth.reset();
        _d_intrin.reset();
        _count = count;
    }
    if( ! points_mapped )
    {
        if( ! _d_points ) _d_points = rscuda::alloc_dev<float>( count * 3 );
        points_dev = _d_points.get();
    }
    if( ! depth_mapped )
    {
        if( ! _d_depth ) _d_depth = rscuda::alloc_dev<uint16_t>( count );
        depth_dev = _d_depth.get();
    }

    // Upload intrinsics once; refresh only if they actually change (e.g. recalibration).
    if( !_d_intrin || std::memcmp( &_intrin_cached, &intrin, sizeof( rs2_intrinsics ) ) != 0 )
    {
        _d_intrin = rscuda::make_device_copy( intrin );
        _intrin_cached = intrin;
    }

    if( ! depth_mapped )
        RS_CUDA_CHECK( cudaMemcpy( depth_dev, depth, count * sizeof( uint16_t ), cudaMemcpyHostToDevice ) );

    // 2D launch: warp-sized x dimension keeps a warp's lanes aligned with consecutive pixels,
    // so depth reads stay coalesced. y dimension is sized to hit the threads-per-block target.
    // Using 1D grid will require integer division (which is ~20-30 cycles on the GPU since there
    // is no hardware integer divider).
    const dim3 block( rscuda::THREADS_IN_WARP, THREADS_PER_BLOCK / rscuda::THREADS_IN_WARP );
    const dim3 grid( ( intrin.width  + block.x - 1 ) / block.x,
                     ( intrin.height + block.y - 1 ) / block.y );
    kernel_deproject_depth_cuda<<< grid, block >>>( points_dev, _d_intrin.get(), depth_dev, depth_scale );
    RS_CUDA_CHECK( cudaGetLastError() );

    if( points_mapped )
        // Mapped output: no D2H copy, but the CPU consumer downstream must see the kernel's
        // writes, so block until the kernel completes.
        RS_CUDA_CHECK( cudaStreamSynchronize( 0 ) );
    else
        RS_CUDA_CHECK( cudaMemcpy( points, _d_points.get(), count * sizeof( float ) * 3, cudaMemcpyDeviceToHost ) );
}

#endif
