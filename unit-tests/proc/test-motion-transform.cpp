// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// The FW accel unit depends on the FW version: legacy FW sends integer milli-g (0.001 g/count),
// newer FW sends 10 micro-g (0.00001 g/count). The scale reaches acceleration_transform from
// d400_motion_base::get_accel_default_scale(); pairing it with the wrong FW is a silent 100x error.

//#cmake: static!

#include "../catch.h"
#include "../approx.h"
#include <src/float3.h>  // motion-transform.h expects float3/float3x3 already in scope
#include <src/proc/motion-transform.h>
#include <src/platform/hid-data.h>

using namespace librealsense;

static constexpr double MILLI_G = 0.001;        // legacy FW: 1 count = 1 mg
static constexpr double TEN_MICRO_G = 0.00001;  // newer FW: 1 count = 10 ug
static constexpr float GRAVITY = 9.80665f;

namespace {

// process_function() is protected; expose it so a sample can be converted without a frame pipeline
struct accel_xform : acceleration_transform
{
    accel_xform( double scale, bool high_accuracy )
        : acceleration_transform( nullptr, nullptr, scale, high_accuracy )
    {
    }
    using acceleration_transform::process_function;
};

float3 convert( double scale, bool high_accuracy, int32_t x, int32_t y, int32_t z )
{
    hid_data source{ x, y, z };
    float3 result{};
    uint8_t * const dest[] = { reinterpret_cast< uint8_t * >( &result ) };
    accel_xform( scale, high_accuracy )
        .process_function( dest, reinterpret_cast< const uint8_t * >( &source ), 0, 0, 0, 0 );
    return result;
}

}  // namespace


TEST_CASE( "accel counts convert to m/s^2 per FW unit", "[motion-transform]" )
{
    auto legacy = convert( MILLI_G, true, 1000, -1000, 0 );  // 1 g in milli-g
    CHECK( legacy.x == approx( GRAVITY ) );
    CHECK( legacy.y == approx( -GRAVITY ) );
    CHECK( legacy.z == approx( 0.f ) );

    auto current = convert( TEN_MICRO_G, true, 100000, -100000, 0 );  // 1 g in 10 micro-g
    CHECK( current.x == approx( GRAVITY ) );
    CHECK( current.y == approx( -GRAVITY ) );
    CHECK( current.z == approx( 0.f ) );
}

TEST_CASE( "both FW units agree on the same acceleration", "[motion-transform]" )
{
    // Reading one FW generation's counts with the other's scale is off by exactly 100x
    for( int32_t mg : { -4000, -250, -1, 0, 3, 2000 } )
    {
        auto legacy = convert( MILLI_G, true, mg, mg, mg );
        auto current = convert( TEN_MICRO_G, true, mg * 100, mg * 100, mg * 100 );
        CHECK( current.x == approx( legacy.x ) );
        CHECK( current.y == approx( legacy.y ) );
        CHECK( current.z == approx( legacy.z ) );
    }
}

TEST_CASE( "16-bit converter keeps only the low word", "[motion-transform]" )
{
    // Pre-5.16 FW packs int16 samples into the 32-bit fields, so the sign comes from the low word
    CHECK( convert( MILLI_G, false, 0xFFFF, 0, 0 ).x == approx( float( -MILLI_G * GRAVITY ) ) );
    CHECK( convert( MILLI_G, true, 0xFFFF, 0, 0 ).x == approx( float( 0xFFFF * MILLI_G * GRAVITY ) ) );
}
