// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// Drives the full viewer through 20 depth-only Start/Stop cycles and samples the
// process's memory after each cycle. Exercises the real viewer code path
// (ux-window loop, gc_streams, post-processing filters, syncer, etc.).
//
// Per-platform memory metric (all measure *current* — not peak — process memory,
// so the slope reflects per-cycle accumulation rather than monotonic high-water marks):
//   Windows: PROCESS_MEMORY_COUNTERS_EX.PrivateUsage     (a.k.a. "Private Bytes")
//   Linux:   VmRSS from /proc/self/status                (current resident set)
//   macOS:   task_info(TASK_VM_INFO).phys_footprint      (unique resident memory)
// `getrusage().ru_maxrss` is deliberately NOT used — it's the *peak* RSS,
// monotonically non-decreasing, and useless for slope-based leak detection.
//
// Results are written to viewer_mem_leak_results.csv next to the executable.
// Plot with plot_mem.py from the scratchpad.
//
// Run headlessly:
//   realsense-viewer-tests --auto -r mem_leak_depth_start_stop

#include "viewer-test-helpers.h"

#include "post-processing-filters.h"
#include "viewer.h"

#include <librealsense2/rs.hpp>

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#  pragma comment( lib, "psapi.lib" )
#elif defined( __APPLE__ )
#  include <mach/mach.h>
#endif


namespace {

size_t get_process_memory_bytes()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if( !GetProcessMemoryInfo( GetCurrentProcess(),
                               reinterpret_cast< PROCESS_MEMORY_COUNTERS * >( &pmc ),
                               sizeof( pmc ) ) )
    {
        std::cerr << "[mem-leak] WARNING: GetProcessMemoryInfo failed, GLE="
                  << GetLastError() << std::endl;
        return 0;
    }
    return static_cast< size_t >( pmc.PrivateUsage );

#elif defined( __APPLE__ )
    task_vm_info_data_t    info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if( task_info( mach_task_self(),
                   TASK_VM_INFO,
                   reinterpret_cast< task_info_t >( &info ),
                   &count )
        != KERN_SUCCESS )
    {
        std::cerr << "[mem-leak] WARNING: task_info TASK_VM_INFO failed" << std::endl;
        return 0;
    }
    return static_cast< size_t >( info.phys_footprint );

#else  // Linux
    std::ifstream status( "/proc/self/status" );
    std::string   line;
    while( std::getline( status, line ) )
    {
        if( line.rfind( "VmRSS:", 0 ) == 0 )
        {
            size_t kb = 0;
            if( std::sscanf( line.c_str(), "VmRSS: %zu kB", &kb ) == 1 )
                return kb * 1024;
            break;
        }
    }
    std::cerr << "[mem-leak] WARNING: could not read VmRSS from /proc/self/status"
              << std::endl;
    return 0;
#endif
}

double to_mb( size_t bytes ) { return bytes / ( 1024.0 * 1024.0 ); }

// Returns the subdevice that exposes a DEPTH stream (the Stereo Module on D4xx).
std::shared_ptr< rs2::subdevice_model > find_depth_subdevice( rs2::device_model & dm )
{
    for( auto && sub : dm.subdevices )
        for( auto && p : sub->profiles )
            if( p.stream_type() == RS2_STREAM_DEPTH )
                return sub;
    return nullptr;
}

// Narrow the depth subdevice's selection to depth-only (turn IR streams off).
// stream_enabled is keyed by stream unique-id; flipping it directly is what the
// viewer's own checkboxes do (subdevice-model.cpp:782, 1016).
void select_depth_only( std::shared_ptr< rs2::subdevice_model > sub )
{
    for( auto && p : sub->profiles )
        sub->stream_enabled[ p.unique_id() ] = ( p.stream_type() == RS2_STREAM_DEPTH );
}

}  // namespace


VIEWER_TEST( "streaming", "mem_leak_depth_start_stop" )
{
    constexpr int   ITERATIONS      = 20;
    constexpr float STREAM_DURATION = 10.0f;  // longer cycles amplify any per-cycle leak
    constexpr float IDLE_DURATION   = 5.0f;

    // Leak verdict (linear-regression slope on iters after warmup).
    // First few iterations include one-time allocations and steady-state pool
    // ramp-up that are not part of the per-cycle leak — skip them.
    constexpr int WARMUP_ITERS = 3;

    // Threshold is platform-dependent because the underlying metric is:
    //   Windows: PrivateUsage   — private commit, clean signal, ~0.5 MB/iter post-fix.
    //   macOS:   phys_footprint — similar to PrivateUsage, comparably clean.
    //   Linux:   VmRSS          — includes file-backed mmaps that the kernel can
    //                             evict under memory pressure, and Mesa softpipe
    //                             (under xvfb-run on CI) commits internal buffers
    //                             in 20-50 MB chunks. On x86_64 Linux CI observed
    //                             slopes swing 1-3 MB/iter run-to-run; on Jetson
    //                             (aarch64 + softpipe) they can reach ~10 MB/iter.
    //                             Threshold split by arch: aarch64 loose (Jetson
    //                             noise floor); x86_64 tight (regression guard).
    //                             Windows/macOS: tight regression guard.
#ifdef __linux__
#  ifdef __aarch64__
    constexpr float LEAK_THRESHOLD_MB_PER_ITER = 12.0f;
#  else
    constexpr float LEAK_THRESHOLD_MB_PER_ITER = 5.0f;
#  endif
#else
    constexpr float LEAK_THRESHOLD_MB_PER_ITER = 1.0f;
#endif

    auto & model = test.find_first_device_or_exit();
    auto depth = find_depth_subdevice( model );
    IM_CHECK( depth != nullptr );
    if( ! depth )
        return;

    select_depth_only( depth );

    const std::string out_path = "viewer_mem_leak_results.csv";
    std::ofstream     out( out_path );
    IM_CHECK( out.is_open() );  // a silently-failing ofstream would no-op every write below

    out << "iteration,private_bytes_mb" << std::endl;

    const auto baseline = get_process_memory_bytes();
    IM_CHECK( baseline > 0 );  // 0 means the platform memory sampler failed; would skew the slope
    std::cout << "[mem-leak] Device: " << model.dev.get_info( RS2_CAMERA_INFO_NAME )
              << "  SN " << model.dev.get_info( RS2_CAMERA_INFO_SERIAL_NUMBER ) << std::endl;
    std::cout << "[mem-leak] Sensor: " << depth->s->get_info( RS2_CAMERA_INFO_NAME ) << std::endl;
    std::cout << "[mem-leak] Baseline: " << std::fixed << std::setprecision( 2 )
              << to_mb( baseline ) << " MB" << std::endl;
    out << 0 << "," << std::fixed << std::setprecision( 2 ) << to_mb( baseline ) << std::endl;

    std::vector< double > samples_mb;
    samples_mb.reserve( ITERATIONS );

    for( int i = 1; i <= ITERATIONS; ++i )
    {
        test.click_stream_toggle_on( model, depth );
        test.sleep( STREAM_DURATION );
        test.click_stream_toggle_off( model, depth );
        test.sleep( IDLE_DURATION );

        const auto used = get_process_memory_bytes();
        const auto mb   = to_mb( used );
        samples_mb.push_back( mb );

        // Snapshot viewer state to find accumulating containers.
        size_t streams_size = 0, streams_origin_size = 0, ppf_frames_queue_size = 0;
        {
            std::lock_guard< std::mutex > lock( test.viewer_model.streams_mutex );
            streams_size          = test.viewer_model.streams.size();
            streams_origin_size   = test.viewer_model.streams_origin.size();
            ppf_frames_queue_size = test.viewer_model.ppf.frames_queue.size();
        }

        const double delta = mb - to_mb( baseline );
        std::cout << "[mem-leak] iter " << std::setw( 2 ) << i << " : " << std::fixed
                  << std::setprecision( 2 ) << mb
                  << " MB  (delta vs baseline: " << std::showpos << delta << std::noshowpos
                  << " MB)"
                  << "  | streams=" << streams_size
                  << "  streams_origin=" << streams_origin_size
                  << "  ppf.frames_queue=" << ppf_frames_queue_size
                  << std::endl;
        out << i << "," << std::fixed << std::setprecision( 2 ) << mb << std::endl;
    }

    std::cout << "[mem-leak] Results written to " << out_path << std::endl;
    IM_CHECK( ! model.is_streaming() );

    // ---- Leak verdict --------------------------------------------------
    // Ordinary least-squares slope (MB per iteration) on iters [WARMUP+1 .. ITERATIONS].
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
    int    n = 0;
    for( int i = WARMUP_ITERS; i < ITERATIONS; ++i )  // samples_mb[0] is iter 1
    {
        double x = static_cast< double >( i + 1 );  // 1-based iter index
        double y = samples_mb[ i ];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
        ++n;
    }
    // Guard the OLS denominator. With ITERATIONS=20, WARMUP_ITERS=3 we get n=17 here,
    // but a future tweak to these constants could degenerate the fit — fail loudly
    // rather than producing NaN (which would silently fail the slope IM_CHECK below).
    IM_CHECK( n >= 2 );
    const double denom = n * sum_xx - sum_x * sum_x;
    IM_CHECK( denom > 0.0 );  // distinct integer x's → guaranteed > 0 when n >= 2, but make it explicit
    const double slope = ( n * sum_xy - sum_x * sum_y ) / denom;

    std::cout << "[mem-leak] linear-fit slope (iters " << ( WARMUP_ITERS + 1 ) << ".."
              << ITERATIONS << "): " << std::fixed << std::setprecision( 2 ) << slope
              << " MB/iter  (threshold: " << LEAK_THRESHOLD_MB_PER_ITER << " MB/iter)"
              << std::endl;
    if( slope > LEAK_THRESHOLD_MB_PER_ITER )
        std::cout << "[mem-leak] VERDICT: LEAK DETECTED" << std::endl;
    else
        std::cout << "[mem-leak] VERDICT: no leak" << std::endl;

    IM_CHECK( slope <= LEAK_THRESHOLD_MB_PER_ITER );
}
