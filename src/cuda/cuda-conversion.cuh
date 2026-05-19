//This file is partly based on Mehran Maghoumi's work: https://github.com/Maghoumi/culibrealsense

#pragma once
#ifndef CUDA_CONVERSION_CUH
#define CUDA_CONVERSION_CUH

#ifdef RS2_USE_CUDA

// Types
#include <stdint.h>
#include "../../include/librealsense2/rs.h"
#include "assert.h"
//#include "../types.h"

// GPU runtime headers
#ifdef RS2_USE_HIP
#include <hip/hip_runtime.h>
#else
#include <cuda_runtime.h>
#endif

//#ifdef _MSC_VER 
// Add library dependencies if using VS
//#pragma comment(lib, "cudart_static")
//#endif

#define RS2_CUDA_THREADS_PER_BLOCK 256

namespace rscuda
{   
    struct y8i_pixel { uint8_t l; uint8_t r; };  
    struct y8i_pixel_mipi { uint8_t l; uint8_t r; };  
    struct y12i_pixel { uint8_t rl : 8, rh : 4, ll : 4, lh : 8; __host__ __device__ int l() const { return lh << 4 | ll; } __host__ __device__ int r() const { return rh << 8 | rl; } };
    struct y12i_pixel_mipi { uint8_t rl : 8, rh : 4, ll : 4, lh : 8, padding : 8; __host__ __device__ int l() const { return lh << 4 | ll; } __host__ __device__ int r() const { return rh << 8 | rl; } };
    void y8_y8_from_y8i_cuda_helper(uint8_t* const dest[], int count, const y8i_pixel * source);
    void y8_y8_from_y8i_mipi_cuda_helper(uint8_t* const dest[], int count, const y8i_pixel_mipi * source);
    template<class SOURCE>
    void y16_y16_from_y12i_10_cuda_helper(uint8_t* const dest[], int count, const SOURCE * source);
    void unpack_yuy2_cuda_helper(const uint8_t* src, uint8_t* dst, int n, rs2_format format);
    void uyvy_to_yuyv_cuda_helper(const uint16_t* src, uint16_t* dst, int n);
    
    template<rs2_format FORMAT> void unpack_yuy2_cuda(uint8_t * const d[], const uint8_t * s, int n)
    {
        const uint8_t *src = reinterpret_cast<const uint8_t *>(s);
        uint8_t *dst = reinterpret_cast<uint8_t *>(d[0]);

        unpack_yuy2_cuda_helper(src, dst, n, FORMAT);
    }
    
    template<class SOURCE> void split_frame_y8_y8_from_y8i_cuda(uint8_t* const dest[], int count, const SOURCE * source)
    {
        y8_y8_from_y8i_cuda_helper(dest, count, reinterpret_cast<const y8i_pixel*>(source));
    }

    template<class SOURCE> void split_frame_y8_y8_from_y8i_mipi_cuda(uint8_t* const dest[], int count, const SOURCE * source)
    {
        y8_y8_from_y8i_mipi_cuda_helper(dest, count, reinterpret_cast<const y8i_pixel_mipi*>(source));
    }
    
    template<class SOURCE> void split_frame_y16_y16_from_y12i_cuda(uint8_t* const dest[], int count, const SOURCE * source)
    {
        y16_y16_from_y12i_10_cuda_helper(dest, count, reinterpret_cast<const SOURCE*>(source));
    }
    
    void unpack_z16_y8_from_sr300_inzi_cuda(uint8_t* const dest, const uint16_t* source, int count);
    
    void unpack_z16_y16_from_sr300_inzi_cuda(uint16_t* const dest, const uint16_t* source, int count);

}


#endif // RS2_USE_CUDA

#endif // LIBREALSENSE_CUDA_CONVERSION_H
