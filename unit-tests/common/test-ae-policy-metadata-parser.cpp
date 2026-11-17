// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:dependencies realsense2

#include <unit-tests/catch.h>

#include <src/metadata-parser.h>

#include <cstring>

using namespace librealsense;

namespace {

constexpr unsigned long long DEPTH_OFFSET = 236;
constexpr unsigned long long RGB_OFFSET = metadata_raw_mode_offset + sizeof( md_rgb_normal_mode );

// Same offsets as the parsers registered by d500_dual_color
md_trailing_byte_parser depth_parser()
{
    auto const control = metadata_raw_mode_offset + offsetof( md_depth_mode, depth_y_mode ) + offsetof( md_depth_y_normal_mode, intel_depth_control );
    return { control, md_type_trait< md_depth_control >::type, DEPTH_OFFSET, 1, 3 };
}

md_trailing_byte_parser rgb_parser()
{
    auto const control = metadata_raw_mode_offset + offsetof( md_rgb_mode, rgb_mode ) + offsetof( md_rgb_normal_mode, intel_rgb_control );
    return { control, md_type_trait< md_rgb_control >::type, RGB_OFFSET, 1, 3 };
}

void set_layout( librealsense::frame & f, unsigned long long control_offset, md_type type, unsigned long long byte_offset, uint8_t value )
{
    md_header header = { type, 0 };
    std::memcpy( f.additional_data.metadata_blob.data() + control_offset, &header, sizeof( header ) );
    f.additional_data.metadata_blob[byte_offset] = value;
    f.additional_data.metadata_size = static_cast< uint32_t >( byte_offset + 1 );
}

}  // namespace

TEST_CASE( "ae policy metadata parser" )
{
    auto const depth_control = metadata_raw_mode_offset + offsetof( md_depth_mode, depth_y_mode ) + offsetof( md_depth_y_normal_mode, intel_depth_control );
    librealsense::frame f;
    rs2_metadata_type value = 0;

    SECTION( "depth layout: values 1-3 reported" )
    {
        for( uint8_t policy : { 1, 2, 3 } )
        {
            set_layout( f, depth_control, md_type::META_DATA_INTEL_DEPTH_CONTROL_ID, DEPTH_OFFSET, policy );
            REQUIRE( depth_parser().find( f, &value ) );
            CHECK( value == policy );
        }
    }

    SECTION( "depth layout: 0 and out-of-range values not supported" )
    {
        for( uint8_t policy : { 0, 4, 255 } )
        {
            set_layout( f, depth_control, md_type::META_DATA_INTEL_DEPTH_CONTROL_ID, DEPTH_OFFSET, policy );
            CHECK_FALSE( depth_parser().find( f, &value ) );
        }
    }

    SECTION( "payload too short to hold the byte: not supported" )
    {
        set_layout( f, depth_control, md_type::META_DATA_INTEL_DEPTH_CONTROL_ID, DEPTH_OFFSET, 1 );
        f.additional_data.metadata_size = DEPTH_OFFSET;
        CHECK_FALSE( depth_parser().find( f, &value ) );
    }

    SECTION( "RGB layout: matched by the RGB parser only" )
    {
        // Both layouts keep their control struct at the same offset, only the md_type tells them apart
        set_layout( f, depth_control, md_type::META_DATA_INTEL_RGB_CONTROL_ID, RGB_OFFSET, 2 );
        REQUIRE( rgb_parser().find( f, &value ) );
        CHECK( value == 2 );
        CHECK_FALSE( depth_parser().find( f, &value ) );
    }

    SECTION( "depth layout is not matched by the RGB parser" )
    {
        set_layout( f, depth_control, md_type::META_DATA_INTEL_DEPTH_CONTROL_ID, DEPTH_OFFSET, 1 );
        CHECK_FALSE( rgb_parser().find( f, &value ) );
    }
}
