/* License: Apache 2.0. See LICENSE file in root directory. */
/* Copyright(c) 2019 RealSense, Inc. All Rights Reserved. */
#pragma once
#ifdef RS2_USE_CUDA

#include "proc/align.h"
#include "cuda-align.cuh"
#include <memory>
#include <stdint.h>

namespace librealsense
{
    // Unlike the scalar/SIMD aligners, these overrides do not pre-clear the output frame: both
    // align_cuda_helper entry points clear it on the GPU and then write every pixel of it, so a
    // host-side memset of the whole image was dead work on every frame.
    class align_cuda : public align
    {
    public:
        align_cuda(rs2_stream align_to) : align(align_to, "Align (CUDA)") {}

    protected:
        void reset_cache(rs2_stream from, rs2_stream to) override
        {
            aligners[std::tuple<rs2_stream, rs2_stream>(from, to)] = align_cuda_helper();
        }

        void align_z_to_other(rs2::video_frame& aligned, const rs2::video_frame& depth, const rs2::video_stream_profile& other_profile, float z_scale) override
        {
            uint8_t * aligned_data = reinterpret_cast<uint8_t *>(const_cast<void*>(aligned.get_data()));

            auto depth_profile = depth.get_profile().as<rs2::video_stream_profile>();

            auto z_intrin = depth_profile.get_intrinsics();
            auto other_intrin = other_profile.get_intrinsics();
            auto z_to_other = depth_profile.get_extrinsics_to(other_profile);

            auto z_pixels = reinterpret_cast<const uint16_t*>(depth.get_data());
            auto& aligner = aligners[std::tuple<rs2_stream, rs2_stream>(RS2_STREAM_DEPTH, other_profile.stream_type())];
            aligner.align_depth_to_other(aligned_data, z_pixels, z_scale, z_intrin, z_to_other, other_intrin);
        }

        void align_other_to_z(rs2::video_frame& aligned, const rs2::video_frame& depth, const rs2::video_frame& other, float z_scale) override
        {
            uint8_t * aligned_data = reinterpret_cast<uint8_t *>(const_cast<void*>(aligned.get_data()));

            auto depth_profile = depth.get_profile().as<rs2::video_stream_profile>();
            auto other_profile = other.get_profile().as<rs2::video_stream_profile>();

            auto z_intrin = depth_profile.get_intrinsics();
            auto other_intrin = other_profile.get_intrinsics();
            auto z_to_other = depth_profile.get_extrinsics_to(other_profile);

            auto z_pixels = reinterpret_cast<const uint16_t*>(depth.get_data());
            auto other_pixels = reinterpret_cast<const uint8_t *>(other.get_data());

            auto& aligner = aligners[std::tuple<rs2_stream, rs2_stream>(other_profile.stream_type(), RS2_STREAM_DEPTH)];
            aligner.align_other_to_depth(
                aligned_data, z_pixels, z_scale, z_intrin, z_to_other, other_intrin, other_pixels, other_profile.format(), other.get_bytes_per_pixel());
        }

    private:
        std::map<std::tuple<rs2_stream, rs2_stream>, align_cuda_helper> aligners;
    };
}
#endif // RS2_USE_CUDA
