#pragma once
#ifdef RS2_USE_CUDA

#include <stdexcept>
#include <memory>
#include <cassert>

// CUDA headers
#include <cuda_runtime.h>

#ifdef _MSC_VER 
// Add library dependencies if using VS
#pragma comment(lib, "cudart_static")
#endif

namespace rscuda
{

    template <typename T>
    class DeviceBuffer final
    {
    public:
        DeviceBuffer() = default;

        explicit DeviceBuffer(std::size_t const num_elements) :
        data_{DeviceBuffer<T>::allocateBuffer(num_elements)},
        size_{num_elements}
        {}

        DeviceBuffer(std::size_t const num_elements, std::size_t const number_of_channels = 1U) :
        DeviceBuffer{num_elements * number_of_channels}
        {}

	void reserve(std::size_t const reserve_size)
        {
            if (size_ < reserve_size)
            {
                cudaFree(data_);
                data_ = DeviceBuffer<T>::allocateBuffer(reserve_size);
                size_ = reserve_size;
            }
        }

        void reserve(std::size_t const reserve_size, std::size_t const reserve_channels)
        {
            reserve(reserve_size * reserve_channels);
        }

        std::size_t size() const { return size_; }

        T* data() { return data_; }

    private:
        static T* allocateBuffer(std::size_t const reserve_size)
        {
            T* datatemp{nullptr};
            cudaMalloc(&datatemp, reserve_size * sizeof(T));
            return datatemp;
        }

        T* data_{};
        // Size is in number of elements, not bytes.
        std::size_t size_{};
    };

    template<typename  T>
    std::shared_ptr<T> alloc_dev(int elements)
    {
        T* d_data;
        auto res = cudaMalloc(&d_data, sizeof(T) * elements);
        if (res != cudaSuccess)
            throw std::runtime_error("cudaMalloc failed status: " + res);
        return std::shared_ptr<T>(d_data, [](T* p) { cudaFree(p); });
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
