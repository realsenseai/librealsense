#ifdef RS2_USE_CUDA

#include "cuda-pointcloud.cuh"
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
    int numBlocks = count / RS2_CUDA_THREADS_PER_BLOCK;
    
    float *dev_points = 0;	
    uint16_t *dev_depth = 0;
    rs2_intrinsics* dev_intrin = 0;
    hipError_t result;

    result = hipMalloc(&dev_points, count * sizeof(float) * 3);
    assert(result == hipSuccess);
    result = hipMalloc(&dev_depth, count * sizeof(uint16_t));
    assert(result == hipSuccess);
    result = hipMalloc(&dev_intrin, sizeof(rs2_intrinsics));
    assert(result == hipSuccess);
       
    result = hipMemcpy(dev_depth, depth, count * sizeof(uint16_t), hipMemcpyHostToDevice);
    assert(result == hipSuccess); 
    result = hipMemcpy(dev_intrin, &intrin, sizeof(rs2_intrinsics), hipMemcpyHostToDevice);
    assert(result == hipSuccess); 
     
    kernel_deproject_depth_cuda<<<numBlocks, RS2_CUDA_THREADS_PER_BLOCK>>>(dev_points, dev_intrin, dev_depth, depth_scale); 

     result = hipMemcpy(points, dev_points, count * sizeof(float) * 3, hipMemcpyDeviceToHost);
     assert(result == hipSuccess);

     if (result); // suppress warning about "variable "result" was set but never used"

    hipFree(dev_points);
    hipFree(dev_depth);
    hipFree(dev_intrin);
}

#endif
