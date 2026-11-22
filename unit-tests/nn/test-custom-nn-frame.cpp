// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:add-file ../../src/nn/nn-protocol.cpp

#include "../catch.h"
#include <src/nn/nn-protocol.h>
#include <src/librealsense-exception.h>
#include <rsutils/number/crc32.h>

#include <cstring>
#include <string>
#include <vector>

using namespace librealsense;
using Catch::Matchers::ContainsSubstring;


static size_t round64( size_t n ) { return ( n + 63 ) / 64 * 64; }

// A version 2 frame as the camera sends it: every part on a 64-byte boundary, padded to 1024 x H
static std::vector< uint8_t > make_frame( std::vector< std::vector< uint8_t > > const & tensors,
                                          uint8_t status = 0,
                                          uint32_t generation = 0x2472D988,
                                          uint8_t dtype = RS2_NN_DTYPE_U8 )
{
    size_t end = 64;
    for( auto const & t : tensors )
        end += 64 + round64( t.size() );
    std::vector< uint8_t > frame( ( end + 1023 ) / 1024 * 1024 );

    nn::custom_nn_payload_header payload{ 1234.5, 77, 76, uint8_t( tensors.size() ), status };
    size_t offset = 64;
    for( size_t i = 0; i < tensors.size(); ++i )
    {
        nn::custom_nn_tensor_header th{ uint8_t( i ), dtype, uint32_t( tensors[i].size() ) };
        std::memcpy( frame.data() + offset, &th, sizeof( th ) );
        std::memcpy( frame.data() + offset + 64, tensors[i].data(), tensors[i].size() );
        offset += 64 + round64( tensors[i].size() );
    }
    std::memcpy( frame.data() + sizeof( nn::custom_nn_frame_header ), &payload, sizeof( payload ) );

    uint32_t const payload_size = uint32_t( end - sizeof( nn::custom_nn_frame_header ) );
    nn::custom_nn_frame_header header{ nn::CUSTOM_NN_MAGIC, nn::CUSTOM_NN_VERSION, uint8_t( status ? nn::CUSTOM_NN_ERROR_FLAG : 0 ),
                                       payload_size, generation,
                                       rsutils::number::calc_crc32( frame.data() + sizeof( header ), payload_size ) };
    std::memcpy( frame.data(), &header, sizeof( header ) );
    return frame;
}

static std::vector< uint8_t > bytes( size_t n, uint8_t first = 1 )
{
    std::vector< uint8_t > v( n );
    for( size_t i = 0; i < n; ++i )
        v[i] = uint8_t( first + i );
    return v;
}

static nn::custom_nn_frame_layout parse( std::vector< uint8_t > const & frame )
{
    return nn::parse_custom_nn_frame( frame.data(), frame.size() );
}

// The parse error, or empty when the frame parses
static std::string error_of( std::vector< uint8_t > const & frame, size_t size = 0 )
{
    try
    {
        nn::parse_custom_nn_frame( frame.data(), size ? size : frame.size() );
    }
    catch( librealsense_exception const & e )
    {
        return e.what();
    }
    return {};
}

// Rewrites the CRC after a change to the payload, so only the change itself is tested
static void fix_crc( std::vector< uint8_t > & frame )
{
    nn::custom_nn_frame_header header;
    std::memcpy( &header, frame.data(), sizeof( header ) );
    header.crc32 = rsutils::number::calc_crc32( frame.data() + sizeof( header ), header.payload_size );
    std::memcpy( frame.data(), &header, sizeof( header ) );
}


TEST_CASE( "custom nn frame layout" )
{
    CHECK( offsetof( nn::custom_nn_frame_header, payload_size ) == 7 );
    CHECK( offsetof( nn::custom_nn_frame_header, generation ) == 11 );
    CHECK( offsetof( nn::custom_nn_frame_header, crc32 ) == 15 );
    CHECK( offsetof( nn::custom_nn_payload_header, frame_id ) == 8 );
    CHECK( offsetof( nn::custom_nn_payload_header, source_frame_id ) == 16 );
    CHECK( offsetof( nn::custom_nn_payload_header, tensor_count ) == 20 );
    CHECK( offsetof( nn::custom_nn_payload_header, status ) == 21 );
    CHECK( offsetof( nn::custom_nn_tensor_header, byte_size ) == 2 );

    CHECK( nn::dtype_size( RS2_NN_DTYPE_S16 ) == 2 );
    CHECK( nn::dtype_size( RS2_NN_DTYPE_U8 ) == 1 );
    CHECK( nn::dtype_size( RS2_NN_DTYPE_F32 ) == 4 );
    CHECK( nn::dtype_size( RS2_NN_DTYPE_COUNT ) == 0 );
}


TEST_CASE( "custom nn frame tensors" )
{
    // Sizes around each 64-byte boundary, and six outputs as DAMO-YOLO
    std::vector< std::vector< uint8_t > > tensors = { bytes( 1 ), bytes( 63, 10 ), bytes( 64, 20 ),
                                                      bytes( 65, 30 ), bytes( 128, 40 ), bytes( 338, 50 ) };
    auto frame = make_frame( tensors );
    REQUIRE( frame.size() == 2048 );

    auto layout = parse( frame );
    CHECK( layout.header.generation == 0x2472D988 );
    CHECK( layout.payload.timestamp_ms == 1234.5 );
    CHECK( layout.payload.frame_id == 77 );
    CHECK( layout.payload.source_frame_id == 76 );
    CHECK( layout.payload.status == 0 );
    REQUIRE( layout.tensors.size() == tensors.size() );

    size_t offset = 128;
    for( size_t i = 0; i < tensors.size(); ++i )
    {
        auto const & t = layout.tensors[i];
        CHECK( t.output_index == i );
        CHECK( t.dtype == RS2_NN_DTYPE_U8 );
        CHECK( t.offset == offset );
        CHECK( t.size == tensors[i].size() );
        CHECK( std::memcmp( frame.data() + t.offset, tensors[i].data(), t.size ) == 0 );
        offset += round64( t.size ) + 64;
    }
}


TEST_CASE( "custom nn frame at the size cap" )
{
    // 1024 x 3072: one s16 tensor filling it
    auto frame = make_frame( { bytes( 1024 * 3072 - 128 ) }, 0, 1, RS2_NN_DTYPE_S16 );
    REQUIRE( frame.size() == 1024 * 3072 );
    auto layout = parse( frame );
    REQUIRE( layout.tensors.size() == 1 );
    CHECK( layout.tensors[0].dtype == RS2_NN_DTYPE_S16 );
    CHECK( layout.tensors[0].size == 1024 * 3072 - 128 );
}


TEST_CASE( "custom nn error frames" )
{
    auto load_failed_new = parse( make_frame( {}, 4, 0 ) );
    CHECK( load_failed_new.tensors.empty() );
    CHECK( load_failed_new.header.flags == nn::CUSTOM_NN_ERROR_FLAG );
    CHECK_THAT( nn::custom_nn_status_message( 4, 0 ), ContainsSubstring( "reset the camera" ) );
    CHECK_THAT( nn::custom_nn_status_message( 4, 0x56ECB061 ), ContainsSubstring( "Upload it again" ) );
    CHECK_THAT( nn::custom_nn_status_message( 5, 1 ), ContainsSubstring( "Inference failed" ) );
    CHECK( std::string( nn::custom_nn_status_message( 0, 1 ) ).empty() );
    CHECK_FALSE( std::string( nn::custom_nn_status_message( 2, 1 ) ).empty() );

    // An error frame's tensors are never read, whatever its count says
    auto frame = make_frame( { bytes( 64 ) }, 5 );
    CHECK( parse( frame ).tensors.empty() );
}


TEST_CASE( "custom nn invalid frames" )
{
    auto const good = make_frame( { bytes( 100 ), bytes( 6 ) } );
    REQUIRE( error_of( good ).empty() );

    auto frame = good;
    frame[0] ^= 1;
    CHECK_THAT( error_of( frame ), ContainsSubstring( "magic" ) );

    frame = good;
    frame[5] = 0x01;  // version 0x0100
    CHECK_THAT( error_of( frame ), ContainsSubstring( "version" ) );

    frame = good;
    frame[200] ^= 0xFF;
    CHECK_THAT( error_of( frame ), ContainsSubstring( "CRC" ) );

    CHECK_THAT( error_of( good, 63 ), ContainsSubstring( "63 bytes" ) );
    // payload_size counts bytes past the end of the buffer
    CHECK_THAT( error_of( good, 256 ), ContainsSubstring( "payload size" ) );

    // A tensor larger than the payload
    frame = good;
    frame[64 + 2] = 0xFF;
    frame[64 + 3] = 0xFF;
    fix_crc( frame );
    CHECK_THAT( error_of( frame ), ContainsSubstring( "past the payload" ) );

    // More tensors than the payload holds
    frame = good;
    frame[19 + 20] = 3;
    fix_crc( frame );
    CHECK_THAT( error_of( frame ), ContainsSubstring( "tensor 2 header" ) );

    // Unknown dtype
    frame = good;
    frame[64 + 1] = RS2_NN_DTYPE_COUNT;
    fix_crc( frame );
    CHECK_THAT( error_of( frame ), ContainsSubstring( "dtype" ) );

    // An s16 tensor of an odd number of bytes
    frame = make_frame( { bytes( 5 ) }, 0, 1, RS2_NN_DTYPE_S16 );
    CHECK_THAT( error_of( frame ), ContainsSubstring( "dtype" ) );
}
