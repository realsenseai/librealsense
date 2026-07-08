// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:dependencies rsutils

#include <unit-tests/test.h>
#include <rsutils/json.h>

TEST_CASE( "JSON round-trip - scalar types", "[json][system-json]" )
{
    rsutils::json j;
    j["int_val"]    = 42;
    j["float_val"]  = 3.14f;
    j["string_val"] = "realsense";
    j["bool_val"]   = true;
    auto parsed = rsutils::json::parse( j.dump() );
    REQUIRE( parsed["int_val"].get<int>()           == 42 );
    REQUIRE( parsed["float_val"].get<float>()       == Catch::Approx( 3.14f ) );
    REQUIRE( parsed["string_val"].get<std::string>() == "realsense" );
    REQUIRE( parsed["bool_val"].get<bool>()         == true );
}

TEST_CASE( "JSON round-trip - nested and array types", "[json][system-json]" )
{
    rsutils::json j;
    j["nested"]["key"] = "value";
    j["array"]         = rsutils::json::array( { 1, 2, 3 } );
    auto parsed = rsutils::json::parse( j.dump() );
    REQUIRE( parsed["nested"]["key"].get<std::string>() == "value" );
    REQUIRE( parsed["array"][1].get<int>() == 2 );
}
