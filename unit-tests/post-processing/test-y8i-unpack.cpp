// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake: static!

#include <unit-tests/test.h>
#include <src/proc/y8i-to-y8y8.h>
#include <vector>

namespace {
struct test_splitter : librealsense::y8i_to_y8y8
{
    using y8i_to_y8y8::process_function;
};
}

TEST_CASE( "Y8I preserves both infrared planes at SIMD and scalar boundaries" )
{
    test_splitter split;
    for( int count : { 1, 15, 16, 17, 31, 32, 33, 424 } )
    {
        CAPTURE( count );
        std::vector< uint8_t > input( 2 * count + 1 );
        std::vector< uint8_t > left( count + 2, 0xaa ), right( count + 2, 0x55 );
        auto expected_left = left;
        auto expected_right = right;
        for( int i = 0; i < count; ++i )
        {
            expected_left[1 + i] = input[1 + 2 * i] = uint8_t( i * 37 + 17 );
            expected_right[1 + i] = input[2 + 2 * i] = uint8_t( i * 13 + 201 );
        }
        uint8_t * planes[] = { left.data() + 1, right.data() + 1 };
        split.process_function( planes, input.data() + 1, count, 1, 2 * count, 2 * count );
        REQUIRE( left == expected_left );
        REQUIRE( right == expected_right );
    }
}
