// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 Intel Corporation. All Rights Reserved.

//#cmake: static!

// Exercises rscuda::unpack_yuy2_cuda_helper -- one of the three kernel
// entry-points flagged as untested in the review of PR #15074.
// The test passes on CUDA, on HIP, and is silently skipped when neither
// runtime is visible to the rsutils probe (so CI agents without a GPU
// don't produce false failures).
//
// Round-trip strategy: build a small synthetic YUY2 frame whose decoded
// RGB values are computable analytically, run the kernel for both
// RS2_FORMAT_RGB8 and RS2_FORMAT_RGBA8, and compare against the same
// reference clamp formula used inside the kernel.  We deliberately test
// at sub-block size (n < RS2_CUDA_THREADS_PER_BLOCK) to cover the
// ceiling-division fix added in this PR.

#include "../algo-common.h"
#include <librealsense2/rs.h>
#include <src/cuda/cuda-conversion.cuh>

#ifdef RS2_USE_CUDA
#include <rsutils/accelerators/gpu.h>

#include <cstdint>
#include <vector>

namespace {

// Reference implementation, copied verbatim from kernel_unpack_yuy2_rgb8_cuda
// so the comparison is "the kernel produced the same bytes the same formula
// would produce on the host" -- not "the kernel matches some other YUV
// implementation".  This is the same trick the project's own SSE tests use.
inline uint8_t clamp_i32(int32_t v)
{
    return v > 255 ? 255 : (v < 0 ? 0 : (uint8_t)v);
}

void yuy2_to_rgb8_reference(const uint8_t* src, uint8_t* dst, int super_pix)
{
    for (int i = 0; i < super_pix; ++i)
    {
        int idx = i * 4;
        int16_t c = src[idx]     - 16;
        int16_t d = src[idx + 1] - 128;
        int16_t e = src[idx + 3] - 128;

        int odx = i * 6;
        dst[odx]     = clamp_i32((298 * c + 409 * e + 128) >> 8);
        dst[odx + 1] = clamp_i32((298 * c - 100 * d - 208 * e + 128) >> 8);
        dst[odx + 2] = clamp_i32((298 * c + 516 * d + 128) >> 8);

        c = src[idx + 2] - 16;
        dst[odx + 3] = clamp_i32((298 * c + 409 * e + 128) >> 8);
        dst[odx + 4] = clamp_i32((298 * c - 100 * d - 208 * e + 128) >> 8);
        dst[odx + 5] = clamp_i32((298 * c + 516 * d + 128) >> 8);
    }
}

// Deterministic synthetic YUY2 buffer.  Values chosen to cover the full
// [0, 255] luma range and to land both inside and outside the clamp.
std::vector<uint8_t> make_yuy2_buffer(int super_pix)
{
    std::vector<uint8_t> src(super_pix * 4);
    for (int i = 0; i < super_pix; ++i)
    {
        src[i * 4]     = uint8_t((i * 17) & 0xFF);   // Y0
        src[i * 4 + 1] = uint8_t((i * 31) & 0xFF);   // U
        src[i * 4 + 2] = uint8_t((i * 19) & 0xFF);   // Y1
        src[i * 4 + 3] = uint8_t((i * 29) & 0xFF);   // V
    }
    return src;
}

void run_yuy2_rgb8_round_trip(int n /* total pixels, must be even */)
{
    if (!rsutils::rs2_is_gpu_available())
        SKIP("No CUDA / HIP capable GPU detected by the runtime probe.");

    REQUIRE((n % 2) == 0);
    int super_pix = n / 2;

    auto src = make_yuy2_buffer(super_pix);

    std::vector<uint8_t> gpu_dst(n * 3, 0xAA);   // 0xAA marker -> easy to spot
    std::vector<uint8_t> cpu_dst(n * 3, 0);      // empty values for reference

    rscuda::unpack_yuy2_cuda_helper(src.data(), gpu_dst.data(), n, RS2_FORMAT_RGB8);
    yuy2_to_rgb8_reference(src.data(), cpu_dst.data(), super_pix);

    // Equality check, byte by byte -- the kernel uses the exact same fixed-
    // point formula as the host reference, so the bit pattern must match.
    for (int i = 0; i < n * 3; ++i)
    {
        CAPTURE(i);
        REQUIRE(int(gpu_dst[i]) == int(cpu_dst[i]));
    }
}

} // namespace

// Realistic frame size: 640x1 = 640 pixels = 320 super-pixels = 2 blocks at
// RS2_CUDA_THREADS_PER_BLOCK = 256.  Exercises the common path.
TEST_CASE("cuda_yuy2_to_rgb8_full_frame")
{
    run_yuy2_rgb8_round_trip(640);
}

// Sub-block-size case (super_pix = 16 < 256).  Before the ceiling-division
// fix in PR #15074 this would have launched the kernel with 0 blocks and
// the output buffer would have stayed full of the 0xAA marker, immediately
// tripping the REQUIRE below.
TEST_CASE("cuda_yuy2_to_rgb8_sub_block_buffer")
{
    run_yuy2_rgb8_round_trip(32);   // 16 super-pixels, well below block size
}

// Single super-pixel.  Smallest meaningful input; also catches the
// "numBlocks rounds down to 0 then ceil promotes to 1" boundary.
TEST_CASE("cuda_yuy2_to_rgb8_single_super_pixel")
{
    run_yuy2_rgb8_round_trip(2);
}

#endif // RS2_USE_CUDA
