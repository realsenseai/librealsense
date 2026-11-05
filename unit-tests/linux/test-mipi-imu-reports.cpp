// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#test:donotrun:!linux

#include <unit-tests/catch.h>

#ifdef RS2_USE_V4L2_BACKEND
#include <src/linux/v4l-mipi-logic.h>
#include <src/platform/hid-data.h>
#include <cstring>
#include <vector>

using namespace librealsense;
using namespace librealsense::platform::v4l_mipi_logic;

namespace
{
constexpr size_t report_size = sizeof( hid_mipi_data_32 );  // 38
constexpr uint8_t accel = hid_mipi_accel, gyro = hid_mipi_gyro, state_valid = 2;

void write_report( std::vector< uint8_t > & buffer, size_t offset, uint8_t sensor, uint64_t timestamp, uint8_t state = state_valid )
{
    hid_mipi_data_32 report{};
    report.typeID = sensor;
    report.skip1 = state;
    report.hwTs = timestamp;
    std::memcpy( buffer.data() + offset, &report, sizeof( report ) );
}

std::vector< uint8_t * > split( std::vector< uint8_t > & buffer )
{
    return get_imu_reports( buffer.data(), buffer.size() );
}
}

TEST_CASE( "GMSL IMU buffer split", "[v4l2][imu]" )
{
    std::vector< uint8_t > buffer( 256, 0 );
    write_report( buffer, 0, accel, 100 );

    SECTION( "single report" )
    {
        auto reports = split( buffer );
        REQUIRE( reports.size() == 1 );
        CHECK( reports[0] == buffer.data() );
    }
    SECTION( "accel + gyro" )
    {
        write_report( buffer, report_size, gyro, 105 );
        auto reports = split( buffer );
        REQUIRE( reports.size() == 2 );
        CHECK( reports[0] == buffer.data() );
        CHECK( reports[1] == buffer.data() + report_size );
    }
    SECTION( "older gyro first" )
    {
        write_report( buffer, report_size, gyro, 50 );
        auto reports = split( buffer );
        REQUIRE( reports.size() == 2 );
        CHECK( reports[0] == buffer.data() + report_size );
        CHECK( reports[1] == buffer.data() );
    }
    SECTION( "equal timestamps: accel first" )
    {
        write_report( buffer, 0, gyro, 100 );
        write_report( buffer, report_size, accel, 100 );
        auto reports = split( buffer );
        REQUIRE( reports.size() == 2 );
        CHECK( reports[0] == buffer.data() + report_size );
        CHECK( reports[1] == buffer.data() );
    }
    SECTION( "malformed: whole buffer dropped" )
    {
        write_report( buffer, report_size, accel, 105 );  // repeated sensor
        CHECK( split( buffer ).size() == 0 );
        write_report( buffer, report_size, 7, 105 );  // unknown sensor
        CHECK( split( buffer ).size() == 0 );
        write_report( buffer, report_size, gyro, 105, 0 );  // invalid state
        CHECK( split( buffer ).size() == 0 );
        write_report( buffer, report_size, gyro, 105 );
        buffer[2 * report_size] = 1;  // extra record after the pair
        CHECK( split( buffer ).size() == 0 );

        std::vector< uint8_t > single( 256, 0 );
        write_report( single, 0, gyro, 100 );
        single[255] = 0xff;  // non-zero padding after a single report
        CHECK( split( single ).size() == 0 );
        write_report( single, 0, 9, 100 );  // invalid first report
        single[255] = 0;
        CHECK( split( single ).size() == 0 );
    }
    SECTION( "D457 64-byte frame is never split or checked" )
    {
        std::vector< uint8_t > d457_frame( 64, 0xab );
        d457_frame[0] = accel;
        CHECK( split( d457_frame ).size() == 1 );
    }
}
#endif
