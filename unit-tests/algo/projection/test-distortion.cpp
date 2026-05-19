// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

//#cmake: static!
//#cmake:add-file ../../../src/proc/sse/sse-pointcloud.cpp

#include "../algo-common.h"
#include <librealsense2/rsutil.h>
#include <src/proc/sse/sse-pointcloud.h>
#include <src/cuda/cuda-pointcloud.cuh>
#include <src/types.h>

// Runtime GPU probe -- this header is in third-party/rsutils and does NOT
// link-depend on libcuda / libamdhip64, so it is always safe to include.
// We use it below to skip the *_cuda_deproject test cases on CI agents
// that have no CUDA driver and no AMD HIP runtime present.
#ifdef RS2_USE_CUDA
#include <rsutils/accelerators/gpu.h>
#endif

rs2_intrinsics intrin
= { 1280,
    720,
    643.720581f,
    357.821259f,
    904.170471f,
    905.155090f,
    RS2_DISTORTION_INVERSE_BROWN_CONRADY,
    { 0.180086836f, -0.534179211f, -0.00139013783f, 0.000118769123f, 0.470662683f } };

void compare(librealsense::float2 pixel1, librealsense::float2 pixel2)
{
    for (auto i = 0; i < 2; i++)
    {
        CAPTURE(i);
        REQUIRE(std::abs(pixel1[i] - pixel2[i]) <= 0.001);
    }
}

TEST_CASE( "inverse_brown_conrady_deproject" )
{
    float point[3] = { 0 };
    librealsense::float2 pixel1 = { 1, 1 };
    librealsense::float2 pixel2 = { 0, 0 };
    float depth = 10.5;
    rs2_deproject_pixel_to_point( point, &intrin, (float*)&pixel1, depth );
    rs2_project_point_to_pixel((float*)&pixel2, &intrin, point );

    compare(pixel1, pixel2);
}

TEST_CASE( "brown_conrady_deproject" )
{
    float point[3] = { 0 };

    librealsense::float2 pixel1 = { 1, 1 };
    librealsense::float2 pixel2 = { 0, 0 };
    float depth = 10.5;
    rs2_deproject_pixel_to_point( point, &intrin, (float*)&pixel1, depth );
    rs2_project_point_to_pixel((float*)&pixel2, &intrin, point );

    compare(pixel1, pixel2);
}

#if 0 //TODO: check why sse tests fails on LibCi
TEST_CASE("inverse_brown_conrady_sse_deproject")
{
    std::shared_ptr<librealsense::pointcloud_sse> pc_sse = std::make_shared<librealsense::pointcloud_sse >();

    librealsense::float2 pixel[4] = { {1, 1}, {0,2},{1,3},{1,4} };
    float depth = 10.5;
    librealsense::float3 points[4] = {};

    // deproject with native code because sse deprojection doesn't implement distortion
    for (auto i = 0; i < 4; i++)
    {
        rs2_deproject_pixel_to_point((float*)&points[i], &intrin, (float*)&pixel[i], depth);
    }
   
    std::vector<librealsense::float2> res(4, { 0,0 });
    std::vector<librealsense::float2> unnormalized_res(4, { 0,0 });
    rs2_extrinsics extrin = { {1,0,0,
        0,1,0,
        0,0,1},{0,0,0} };

    pc_sse->get_texture_map_sse((librealsense::float2*)res.data(), points, 4, 1, intrin, extrin, (librealsense::float2*)unnormalized_res.data());

    for (auto i = 0; i < 4; i++)
    {
        compare(unnormalized_res[i], pixel[i]);
    }
}

TEST_CASE("brown_conrady_sse_deproject")
{
    std::shared_ptr<librealsense::pointcloud_sse> pc_sse = std::make_shared<librealsense::pointcloud_sse >();

    librealsense::float2 pixel[4] = { {1, 1}, {0,2},{1,3},{1,4} };
    float depth = 10.5;
    librealsense::float3 points[4] = {};

    // deproject with native code because sse deprojection doesn't implement distortion
    for (auto i = 0; i < 4; i++)
    {
        rs2_deproject_pixel_to_point((float*)&points[i], &intrin, (float*)&pixel[i], depth);
    }

    std::vector<librealsense::float2> res(4, { 0,0 });
    std::vector<librealsense::float2> unnormalized_res(4, { 0,0 });
    rs2_extrinsics extrin = { {1,0,0,
        0,1,0,
        0,0,1},{0,0,0} };

    pc_sse->get_texture_map_sse((librealsense::float2*)res.data(), points, 4, 1, intrin, extrin, (librealsense::float2*)unnormalized_res.data());

    for (auto i = 0; i < 4; i++)
    {
        compare(unnormalized_res[i], pixel[i]);
    }
}
#endif

#ifdef RS2_USE_CUDA

// Helper: build a rs2_intrinsics that matches a `w x h` image and reuses the
// global distortion coefficients.  The principal point is centred and the
// focal length is scaled with the width so projecting the centre pixel still
// round-trips.  Used by the small-resolution test cases below that exercise
// the ceiling-division boundary in numBlocks (count < RS2_CUDA_THREADS_PER_BLOCK)
// without allocating an 1280x720 buffer per assertion.
static rs2_intrinsics make_intrin(int w, int h)
{
    rs2_intrinsics out = intrin;
    out.width  = w;
    out.height = h;
    out.ppx    = (w - 1) * 0.5f;
    out.ppy    = (h - 1) * 0.5f;
    out.fx     = intrin.fx * w / 1280.0f;
    out.fy     = intrin.fy * h / 720.0f;
    return out;
}

// Round-trip every pixel of the given intrinsics through deproject_depth_cuda
// and rs2_project_point_to_pixel.  Skips silently if no GPU is visible to the
// runtime probe -- this lets the test run on CI agents without a CUDA/HIP
// device without false failures.
static void check_cuda_deproject_round_trip(const rs2_intrinsics& in, uint16_t depth_value = 1000)
{
    if (!rsutils::rs2_is_gpu_available())
        SKIP("No CUDA / HIP capable GPU detected by the runtime probe.");

    const int count = in.width * in.height;
    std::vector<float3>   point(count, { 0,0,0 });
    std::vector<uint16_t> depth(count, depth_value);

    rscuda::deproject_depth_cuda((float*)point.data(), in, depth.data(), 1);

    librealsense::float2 pixel = { 0, 0 };
    for (int i = 0; i < in.height; ++i)
    {
        for (int j = 0; j < in.width; ++j)
        {
            CAPTURE(i, j);
            rs2_project_point_to_pixel((float*)&pixel, &in, (float*)&point[i * in.width + j]);
            compare({ (float)j, (float)i }, pixel);
        }
    }
}

TEST_CASE("inverse_brown_conrady_cuda_deproject")
{
    check_cuda_deproject_round_trip(intrin);
}

TEST_CASE("brown_conrady_cuda_deproject")
{
    check_cuda_deproject_round_trip(intrin);
}

// Regression test for the "numBlocks = 0 when count < RS2_CUDA_THREADS_PER_BLOCK"
// bug raised in PR #15074: with a 4x4 image the original integer division
// produced 0 blocks and the kernel was launched with an empty grid, leaving
// the output buffer uninitialised.  With ceiling division the kernel runs
// and the round-trip succeeds.
TEST_CASE("cuda_deproject_small_buffer_ceiling_division")
{
    auto small = make_intrin(4, 4);   // 16 pixels << RS2_CUDA_THREADS_PER_BLOCK (256)
    check_cuda_deproject_round_trip(small);
}

// Sanity test at an exact multiple of the block size, to make sure the
// fix did not regress the canonical "(count % BLOCK) == 0" path.
TEST_CASE("cuda_deproject_block_aligned_buffer")
{
    auto aligned = make_intrin(32, 8);   // 256 pixels, exactly one block
    check_cuda_deproject_round_trip(aligned);
}

#endif