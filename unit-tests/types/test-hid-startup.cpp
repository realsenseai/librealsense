// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include <unit-tests/catch.h>
#include <src/mf/hid-startup.h>
#include <future>

using librealsense::platform::hid_startup;
using namespace std::chrono;

TEST_CASE( "HID startup verification is limited to D585 USB products", "[hid][startup]" )
{
    for( auto const & pid : {"c04", "c05", "c07", "c08"} )
    {
        CHECK( hid_startup::is_required( "38e5", pid ) );
        CHECK_FALSE( hid_startup::is_required( "8086", pid ) );
    }
    CHECK_FALSE( hid_startup::is_required( "8086", "b5c" ) );  // D455
    CHECK_FALSE( hid_startup::is_required( "8086", "b6b" ) );  // D585S
    CHECK_FALSE( hid_startup::is_required( "38e5", "c09" ) );  // no automatic opt-in for new products
}

TEST_CASE( "Unverified HID startup completes without any sensor reports", "[hid][startup]" )
{
    hid_startup startup( {}, 100 );
    // Keep callbacks out of a partially registered capture. With no sensors
    // to verify, finishing registration needs no data or timeout allowance.
    CHECK_FALSE( startup.on_report( "Gyro", 99 ) );
    REQUIRE_NOTHROW( startup.wait_for( milliseconds(0) ) );
    CHECK( startup.on_report( "Gyro", 99 ) );
    CHECK( startup.on_report( "Accel", 101 ) );
}

TEST_CASE( "HID startup requires every requested sensor", "[hid][startup]" )
{
    hid_startup startup( {"Accel", "Gyro"}, 100 );
    CHECK_FALSE( startup.on_report( "Accel", 101 ) );
    CHECK_THROWS_MATCHES( startup.wait_for( milliseconds(1) ), std::runtime_error,
                         Catch::Matchers::Message( "Windows HID start timed out waiting for fresh data from: Gyro" ) );
    CHECK_FALSE( startup.on_report( "Gyro", 102 ) );
    REQUIRE_NOTHROW( startup.wait_for( milliseconds(1) ) );
    CHECK( startup.on_report( "Accel", 103 ) );
    CHECK( startup.on_report( "Gyro", 104 ) );
}

TEST_CASE( "Cached HID reports cannot admit a new capture", "[hid][startup]" )
{
    hid_startup startup( {"Accel", "Gyro"}, 100 );
    CHECK_FALSE( startup.on_report( "Accel", 99 ) );
    CHECK_FALSE( startup.on_report( "Gyro", 101 ) );
    CHECK_THROWS_MATCHES( startup.wait_for( milliseconds(1) ), std::runtime_error,
                         Catch::Matchers::Message( "Windows HID start timed out waiting for fresh data from: Accel" ) );
    CHECK_FALSE( startup.on_report( "Accel", 100 ) );
    REQUIRE_NOTHROW( startup.wait_for( milliseconds(1) ) );
}

TEST_CASE( "HID startup does not forward warm-up callbacks", "[hid][startup]" )
{
    hid_startup startup( {"Gyro"}, 100 );
    CHECK_FALSE( startup.on_report( "Gyro", 101 ) );
    CHECK_FALSE( startup.on_report( "Gyro", 102 ) );
    REQUIRE_NOTHROW( startup.wait_for( milliseconds(1) ) );
    CHECK( startup.on_report( "Gyro", 103 ) );
}

TEST_CASE( "HID startup receives concurrent sensor notifications", "[hid][startup]" )
{
    hid_startup startup( {"Accel", "Gyro"}, 100 );
    auto accel = std::async( std::launch::async, [&] { return startup.on_report( "Accel", 101 ); } );
    auto gyro = std::async( std::launch::async, [&] { return startup.on_report( "Gyro", 102 ); } );
    REQUIRE_NOTHROW( startup.wait_for( seconds(5) ) );
    CHECK_FALSE( accel.get() );
    CHECK_FALSE( gyro.get() );
    CHECK( startup.on_report( "Accel", 103 ) );
}

TEST_CASE( "A new HID attempt starts with its own readiness state", "[hid][startup]" )
{
    hid_startup failed( {"Gyro"}, 100 );
    CHECK_THROWS_AS( failed.wait_for( milliseconds(1) ), std::runtime_error );
    hid_startup retry( {"Gyro"}, 200 );
    CHECK_FALSE( retry.on_report( "Gyro", 199 ) );
    CHECK_THROWS_AS( retry.wait_for( milliseconds(1) ), std::runtime_error );
    CHECK_FALSE( retry.on_report( "Gyro", 201 ) );
    REQUIRE_NOTHROW( retry.wait_for( milliseconds(1) ) );
}
