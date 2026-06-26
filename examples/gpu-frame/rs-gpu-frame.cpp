// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

// rs-gpu-frame
// ------------
// Demonstrates the rs2::gpu_frame extension — the zero-copy GPU pointer API — and prints a clear
// per-frame report so you can see, on YOUR build + hardware, whether a frame is delivered in
// GPU-accessible memory.
//
// The model (mirrors rs2::gl::gpu_frame):
//   * frame::get_data()            -> HOST pointer. Always valid. Unchanged decade-old contract.
//   * frame::as<rs2::gpu_frame>()  -> non-null ONLY when the frame's pixels are GPU-resident
//                                     (built with BUILD_WITH_CUDA_ZEROCOPY *and* on an integrated
//                                     GPU / Jetson). gpu_frame::get_gpu_data() then returns a CUDA
//                                     device pointer aliasing the frame — feed it straight to a
//                                     kernel / TensorRT, no host->device copy.
//   * frame::get_gpu_data_or_upload(&copied) -> the "always works on a CUDA build" path: returns a
//                                     device pointer regardless, setting copied=false for true
//                                     zero-copy or copied=true when the SDK had to upload. Returns
//                                     null only on a non-CUDA build.
//
// IMPORTANT: the GPU pointer aliases the SHARED frame buffer. Treat it as READ-ONLY (other
// consumers — the viewer, recording, pyrealsense2 — read the same memory), and keep the frame
// alive (hold it) until your GPU work completes. Copy first if you need to mutate.
//
// This example needs no CUDA toolchain to build: it only uses the public API.

#include <librealsense2/rs.hpp>
#include <iostream>

int main()
try
{
    // Keep the console readable: show only warnings/errors from the SDK, not the per-frame DEBUG
    // spam. (Comment this out, or raise to RS2_LOG_SEVERITY_DEBUG, to see the SDK internals.)
    rs2::log_to_console( RS2_LOG_SEVERITY_WARN );

    rs2::pipeline pipe;
    rs2::config cfg;
    cfg.enable_stream( RS2_STREAM_COLOR );  // RGB is the interesting case for GPU / NN consumers
    pipe.start( cfg );

    // Warm up so streaming and frame-pool allocation have settled.
    for( int i = 0; i < 30; ++i )
        pipe.wait_for_frames();

    std::cout << "\n=== rs2::gpu_frame / zero-copy GPU pointer demo ===\n"
                 "  host    : CPU pointer from get_data() (always valid)\n"
                 "  gpu_frame: did frame.as<rs2::gpu_frame>() succeed? (true => GPU-resident)\n"
                 "  device  : pointer from get_gpu_data_or_upload() (always usable on a CUDA build)\n"
                 "  path    : ZERO-COPY (no copy) vs UPLOAD (SDK host->device copy) vs NONE (no CUDA)\n\n";

    bool any_zero_copy = false;
    bool any_device    = false;

    for( int i = 0; i < 5; ++i )
    {
        auto color = pipe.wait_for_frames().get_color_frame();
        if( ! color )
            continue;

        const void * host = color.get_data();

        // Is this frame GPU-resident? The extension cast is the public "is it on the GPU" check.
        auto         gf     = color.as< rs2::gpu_frame >();
        const void * zc_ptr = gf ? gf.get_gpu_data() : nullptr;

        // Robust path: a device pointer no matter what. `copied` tells you which path you got.
        bool         copied = false;
        const void * dev    = color.get_gpu_data_or_upload( &copied );

        const char * path = ! dev ? "NONE (non-CUDA build)"
                                   : ( copied ? "UPLOAD (host->device copy)" : "ZERO-COPY (no copy)" );

        std::cout << "frame " << color.get_frame_number()
                  << "  " << color.get_width() << "x" << color.get_height()
                  << "  host="      << host
                  << "  gpu_frame=" << ( gf ? "yes" : "no " )
                  << "  device="    << dev
                  << "  path="      << path << "\n";

        any_zero_copy |= static_cast< bool >( gf );
        any_device    |= ( dev != nullptr );
    }

    std::cout << "\n";
    if( any_zero_copy )
    {
        std::cout << "ZERO-COPY ACTIVE — frame.as<rs2::gpu_frame>() returns a CUDA device pointer\n"
                     "that aliases the frame in place. Feed it directly to your GPU consumer:\n"
                     "    auto gf = color.as<rs2::gpu_frame>();\n"
                     "    if( gf ) my_kernel<<<grid,block>>>( (const uint8_t*)gf.get_gpu_data(), w, h );\n"
                     "Read-only; hold the frame until the GPU work finishes.\n";
    }
    else if( any_device )
    {
        std::cout << "CUDA build, but frames are NOT zero-copy-resident (discrete GPU, or zero-copy\n"
                     "not built). frame.as<rs2::gpu_frame>() is null; get_gpu_data_or_upload() still\n"
                     "returns a device pointer via an SDK-managed upload (path=UPLOAD above).\n"
                     "Build with BUILD_WITH_CUDA_ZEROCOPY and run on an integrated GPU (Jetson) to\n"
                     "get the zero-copy path.\n";
    }
    else
    {
        std::cout << "Non-CUDA build — no GPU pointer available. Use get_data() and upload it\n"
                     "yourself (e.g. cudaMemcpy H2D). Rebuild with BUILD_WITH_CUDA[_ZEROCOPY] for\n"
                     "the GPU paths.\n";
    }

    pipe.stop();
    return EXIT_SUCCESS;
}
catch( const rs2::error & e )
{
    std::cerr << "RealSense error calling " << e.get_failed_function()
              << "(" << e.get_failed_args() << "):\n    " << e.what() << "\n";
    return EXIT_FAILURE;
}
catch( const std::exception & e )
{
    std::cerr << e.what() << "\n";
    return EXIT_FAILURE;
}
