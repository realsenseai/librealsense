// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// Adapted from https://github.com/IntelRealSense/librealsense/pull/7923 by thomas-slamcore,
// which first demonstrated with a test that the 32-bit hardware-timestamp wrap-around breaks
// the global-time linear fit; extended with an interleaved-stream wrap case.

//#cmake: static!

#include "../catch.h"
#include <src/global_timestamp_reader.h>
#include <vector>

using namespace librealsense;

constexpr uint64_t PERIOD_USEC = 20000;      // 50 Hz
constexpr uint64_t WRAP_USEC = 1ULL << 32;   // HW timestamps wrap every 2^32 microseconds
constexpr double USEC_TO_MSEC = 0.001;

static double to_hw_ms( uint64_t ts_usec )
{
    return ( ts_usec % WRAP_USEC ) * USEC_TO_MSEC;
}

static double to_sys_ms( uint64_t ts_usec )
{
    return ts_usec * USEC_TO_MSEC;
}

// Feed a clock-sync sample the way time_diff_keeper::update_diff_time does: only the
// sample-admission path may re-base the fit onto the current wrap epoch.
static void add_sample( CLinearCoefficients & coefs, uint64_t ts_usec, bool is_ready )
{
    if( is_ready )
        coefs.update_samples_base( to_hw_ms( ts_usec ) );
    coefs.add_value( CSample( to_hw_ms( ts_usec ), to_sys_ms( ts_usec ) ) );
}

// Convert a frame's HW time the way time_diff_keeper::get_system_hw_time does: read-only
// wrap alignment, never mutating the shared fit.
static double query( CLinearCoefficients & coefs, uint64_t ts_usec )
{
    double x = coefs.to_fit_domain( to_hw_ms( ts_usec ) );
    coefs.update_last_sample_time( x );
    return coefs.calc_value( x );
}

// With zero-latency samples (hardware and system clocks advancing identically) the fit is
// the identity mapping, so querying any fed timestamp must reproduce its system time -
// including when the 32-bit hardware counter wraps mid-sequence.
static void test_linear_coefficients( const uint64_t ts_offset )
{
    CLinearCoefficients coefs( 15 );

    std::vector< uint64_t > timestamps;
    for( size_t i = 0; i < 10; ++i )
        timestamps.push_back( ts_offset + i * PERIOD_USEC );

    bool is_ready = false;
    for( auto ts : timestamps )
    {
        add_sample( coefs, ts, is_ready );
        is_ready = true;
    }

    // It should be possible to query timestamps in the past - frames are converted some
    // time after they were captured
    for( auto ts : timestamps )
        CHECK( query( coefs, ts ) == Catch::Approx( to_sys_ms( ts ) ).margin( 0.001 ) );
}

TEST_CASE( "linear_coefficients_simple", "[global-timestamp]" )
{
    test_linear_coefficients( 0 );
}

TEST_CASE( "linear_coefficients_timewrap", "[global-timestamp]" )
{
    // Start a little bit before the hardware wrap-around time
    test_linear_coefficients( ( ( WRAP_USEC - 5 * PERIOD_USEC ) / PERIOD_USEC ) * PERIOD_USEC );
}

// Around the wrap instant, in-flight frames from different streams of one device straddle
// the boundary (one stream still converting HW times near 2^32 us while another already
// converts times near 0) and are converted interleaved through the shared fit. Per-frame
// conversion used to re-base the fit on every such query, yanking it back and forth by a
// full wrap period; it must be read-only and correct for both epochs.
TEST_CASE( "linear_coefficients_interleaved_wrap", "[global-timestamp]" )
{
    CLinearCoefficients coefs( 15 );

    // Bring the fit to steady state just before the wrap
    uint64_t ts = WRAP_USEC - 8 * PERIOD_USEC;
    bool is_ready = false;
    for( int i = 0; i < 6; ++i, ts += PERIOD_USEC )
    {
        add_sample( coefs, ts, is_ready );
        is_ready = true;
    }

    // Interleaved per-frame conversions from both sides of the wrap
    const uint64_t pre_wrap = WRAP_USEC - PERIOD_USEC;
    const uint64_t post_wrap = WRAP_USEC + PERIOD_USEC;
    for( int i = 0; i < 10; ++i )
    {
        CHECK( query( coefs, pre_wrap ) == Catch::Approx( to_sys_ms( pre_wrap ) ).margin( 0.001 ) );
        CHECK( query( coefs, post_wrap ) == Catch::Approx( to_sys_ms( post_wrap ) ).margin( 0.001 ) );
    }

    // The clock-sync loop keeps sampling across the wrap; the fit must still be intact
    for( int i = 0; i < 8; ++i, ts += PERIOD_USEC )
        add_sample( coefs, ts, is_ready );

    for( uint64_t t = WRAP_USEC - 8 * PERIOD_USEC; t < ts; t += PERIOD_USEC )
        CHECK( query( coefs, t ) == Catch::Approx( to_sys_ms( t ) ).margin( 0.001 ) );
}

TEST_CASE( "align_to_epoch", "[global-timestamp]" )
{
    const double wrap_ms = WRAP_USEC * USEC_TO_MSEC;
    // Same epoch: unchanged
    CHECK( CLinearCoefficients::align_to_epoch( 5000., 6000. ) == Catch::Approx( 5000. ).margin( 0.001 ) );
    // Value just after the wrap, anchor just before it: shifted up one period
    CHECK( CLinearCoefficients::align_to_epoch( 20., wrap_ms - 20. ) == Catch::Approx( wrap_ms + 20. ).margin( 0.001 ) );
    // Value just before the wrap, anchor just after it: shifted down one period
    CHECK( CLinearCoefficients::align_to_epoch( wrap_ms - 20., 20. ) == Catch::Approx( -20. ).margin( 0.001 ) );
}

// A long innovation-rejection streak ends with the fit rebuilt from the rejected-sample window
// (refit_from_samples). The window is collected on a single wrap epoch (see update_diff_time),
// so a streak that spans the wrap instant must still rebuild a valid fit for both epochs.
TEST_CASE( "refit_from_samples_across_wrap", "[global-timestamp]" )
{
    CLinearCoefficients coefs( 15 );

    // Pre-existing fit state from well before the wrap
    uint64_t ts = WRAP_USEC - 100 * PERIOD_USEC;
    bool is_ready = false;
    for( int i = 0; i < 6; ++i, ts += PERIOD_USEC )
    {
        add_sample( coefs, ts, is_ready );
        is_ready = true;
    }

    // Rejection window straddling the wrap, collected the way update_diff_time collects it:
    // each sample epoch-aligned onto the window before being stored (newest at front)
    std::deque< CSample > window;
    for( uint64_t t = WRAP_USEC - 7 * PERIOD_USEC; t <= WRAP_USEC + 7 * PERIOD_USEC; t += PERIOD_USEC )
    {
        double x = to_hw_ms( t );
        if( ! window.empty() )
            x = CLinearCoefficients::align_to_epoch( x, window.front()._x );
        window.push_front( CSample( x, to_sys_ms( t ) ) );
    }

    coefs.refit_from_samples( window );

    // The rebuilt fit must be valid on both sides of the wrap
    for( uint64_t t = WRAP_USEC - 7 * PERIOD_USEC; t <= WRAP_USEC + 7 * PERIOD_USEC; t += PERIOD_USEC )
        CHECK( query( coefs, t ) == Catch::Approx( to_sys_ms( t ) ).margin( 0.001 ) );

    // And the clock-sync loop must be able to keep extending it
    for( uint64_t t = WRAP_USEC + 8 * PERIOD_USEC; t <= WRAP_USEC + 12 * PERIOD_USEC; t += PERIOD_USEC )
    {
        add_sample( coefs, t, true );
        CHECK( query( coefs, t ) == Catch::Approx( to_sys_ms( t ) ).margin( 0.001 ) );
    }
}
