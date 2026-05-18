#pragma once
#ifdef RS2_USE_CUDA

#include "../../../include/librealsense2/rs.h"
#include <memory>
#include <stdint.h>

// GPU runtime headers (for int2 vector type)
#ifdef RS2_USE_HIP
#include <hip/hip_runtime.h>
#else
#include <cuda_runtime.h>
#endif

namespace librealsense
{
    class align_cuda_helper
    {
    public:
        align_cuda_helper() :
            _d_depth_in(nullptr),
            _d_other_in(nullptr),
            _d_aligned_out(nullptr) {}

        void align_other_to_depth(unsigned char* h_aligned_out, const uint16_t* h_depth_in,
            float depth_scale, const rs2_intrinsics& h_depth_intrin, const rs2_extrinsics& h_depth_to_other,
            const rs2_intrinsics& h_other_intrin, const unsigned char* h_other_in, rs2_format other_format, int other_bytes_per_pixel);

        void align_depth_to_other(unsigned char* h_aligned_out, const uint16_t* h_depth_in,
            float depth_scale, const rs2_intrinsics& h_depth_intrin, const rs2_extrinsics& h_depth_to_other,
            const rs2_intrinsics& h_other_intrin);

    private:
        std::shared_ptr<uint16_t>       _d_depth_in;
        std::shared_ptr<unsigned char>  _d_other_in;
        std::shared_ptr<unsigned char>  _d_aligned_out;
        std::shared_ptr<int2>           _d_pixel_map;

        std::shared_ptr<rs2_intrinsics> _d_other_intrinsics;
        std::shared_ptr<rs2_intrinsics> _d_depth_intrinsics;
        std::shared_ptr<rs2_extrinsics> _d_depth_other_extrinsics;
    };
}
#endif // RS2_USE_CUDA
