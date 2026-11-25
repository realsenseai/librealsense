// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:add-file ../../src/nn/nn-protocol.cpp

#include "../catch.h"
#include <src/nn/nn-protocol.h>
#include <src/librealsense-exception.h>
#include <rsutils/number/crc32.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace librealsense;
using Catch::Matchers::ContainsSubstring;


static size_t round64( size_t n ) { return ( n + 63 ) / 64 * 64; }

// A version 3 frame as the D585 sends it: every part on a 64-byte boundary, padded to 1024 x H
static std::vector< uint8_t > make_frame( std::vector< std::vector< uint8_t > > const & tensors,
                                          uint8_t status = 0,
                                          uint32_t generation = 0x2472D988,
                                          uint8_t dtype = RS2_NN_DTYPE_U8,
                                          uint32_t model_id = 0x1234ABCD )
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
                                       payload_size, model_id, generation,
                                       rsutils::number::calc_crc32( frame.data() + sizeof( header ), payload_size ) };
    std::memcpy( frame.data(), &header, sizeof( header ) );
    return frame;
}

// A version 1 frame, as the D555E test FW sends it: every part packed right after the one before, padded to 1024 x H
static std::vector< uint8_t > make_frame_v1( std::vector< std::vector< uint8_t > > const & tensors )
{
    std::vector< uint8_t > body;
    nn::custom_nn_payload_header payload{ 1234.5, 77, 76, uint8_t( tensors.size() ), 0 };
    body.insert( body.end(), (const uint8_t *)&payload, (const uint8_t *)&payload + sizeof( payload ) );
    for( size_t i = 0; i < tensors.size(); ++i )
    {
        nn::custom_nn_tensor_header th{ uint8_t( i ), RS2_NN_DTYPE_U8, uint32_t( tensors[i].size() ) };
        body.insert( body.end(), (const uint8_t *)&th, (const uint8_t *)&th + sizeof( th ) );
        body.insert( body.end(), tensors[i].begin(), tensors[i].end() );
    }
    nn::custom_nn_frame_header_v1 header{ nn::CUSTOM_NN_MAGIC, nn::CUSTOM_NN_VERSION_PACKED, 0, uint32_t( body.size() ),
                                          0x2472D988, rsutils::number::calc_crc32( body.data(), body.size() ) };
    std::vector< uint8_t > frame( (const uint8_t *)&header, (const uint8_t *)&header + sizeof( header ) );
    frame.insert( frame.end(), body.begin(), body.end() );
    frame.resize( ( frame.size() + 1023 ) / 1024 * 1024 );
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
    CHECK( offsetof( nn::custom_nn_frame_header, model_id ) == 11 );
    CHECK( offsetof( nn::custom_nn_frame_header, generation ) == 15 );
    CHECK( offsetof( nn::custom_nn_frame_header, crc32 ) == 19 );
    CHECK( offsetof( nn::custom_nn_frame_header_v1, generation ) == 11 );
    CHECK( offsetof( nn::custom_nn_frame_header_v1, crc32 ) == 15 );
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
    CHECK( layout.header.model_id == 0x1234ABCD );
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

    // A package without a model id
    CHECK( parse( make_frame( tensors, 0, 1, RS2_NN_DTYPE_U8, 0 ) ).header.model_id == 0 );
}


TEST_CASE( "custom nn version 1 frames" )
{
    std::vector< std::vector< uint8_t > > tensors = { bytes( 1 ), bytes( 63, 10 ), bytes( 338, 50 ) };
    auto frame = make_frame_v1( tensors );
    auto layout = parse( frame );
    CHECK( layout.header.version == nn::CUSTOM_NN_VERSION_PACKED );
    CHECK( layout.header.model_id == 0 );
    CHECK( layout.header.generation == 0x2472D988 );
    CHECK( layout.payload.frame_id == 77 );
    REQUIRE( layout.tensors.size() == tensors.size() );
    size_t offset = 19 + 22;
    for( size_t i = 0; i < tensors.size(); ++i )
    {
        offset += 6;
        CHECK( layout.tensors[i].offset == offset );
        CHECK( std::memcmp( frame.data() + offset, tensors[i].data(), tensors[i].size() ) == 0 );
        offset += tensors[i].size();
    }

    // Measured on a D555E: MobileNetV2's 2,048 B output makes a 2,095 B frame
    auto const mobilenet = parse( make_frame_v1( { bytes( 2048 ) } ) ).tensors.at( 0 );
    CHECK( mobilenet.offset + mobilenet.size == 2095 );
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
    CHECK_THAT( nn::custom_nn_status_message( 6, 1 ), ContainsSubstring( "stopped the camera's DSP" ) );
    auto dsp_fault = parse( make_frame( {}, 6 ) );
    CHECK( dsp_fault.payload.status == 6 );
    CHECK( dsp_fault.tensors.empty() );
    CHECK( std::string( nn::custom_nn_status_message( 0, 1 ) ).empty() );
    CHECK_THAT( nn::custom_nn_status_message( 1, 1 ), ContainsSubstring( "no Custom NN model description" ) );
    CHECK_THAT( nn::custom_nn_status_message( 2, 1 ), ContainsSubstring( "didn't fit" ) );
    CHECK_THAT( nn::custom_nn_status_message( 3, 1 ), ContainsSubstring( "invalid" ) );
    CHECK_THAT( nn::custom_nn_status_message( 9, 1 ), ContainsSubstring( "couldn't run" ) );

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

    // Version 2, sent only by earlier D585 test images, is refused
    frame = good;
    frame[5] = 0x02;
    CHECK_THAT( error_of( frame ), ContainsSubstring( "unsupported version 0x200" ) );

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
    frame[23 + 20] = 3;
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



// Frames in custom-nn-v3-frames (see its README.md), next to this file
static std::vector< uint8_t > frame_file( std::string const & name )
{
    std::string path = __FILE__;
    path = path.substr( 0, path.find_last_of( "/\\" ) + 1 ) + "custom-nn-v3-frames/" + name;
    std::ifstream file( path, std::ios::binary );
    REQUIRE( file );
    return std::vector< uint8_t >( std::istreambuf_iterator< char >( file ), {} );
}

// Byte k of output i's data is ( i * 37 + k ) mod 256
static bool has_pattern( std::vector< uint8_t > const & frame, nn::custom_nn_tensor const & t )
{
    for( size_t k = 0; k < t.size; ++k )
        if( frame[t.offset + k] != uint8_t( t.output_index * 37 + k ) )
            return false;
    return true;
}


TEST_CASE( "custom nn host-built frames" )
{
    auto const frame = frame_file( "v3_01_ok_model_id_42_1024x313.bin" );
    REQUIRE( frame.size() == 1024 * 313 );
    auto layout = parse( frame );
    CHECK( layout.header.version == nn::CUSTOM_NN_VERSION );
    CHECK( layout.header.flags == 0 );
    CHECK( 23 + layout.header.payload_size == 320064 );
    CHECK( layout.header.model_id == 42 );
    CHECK( layout.header.generation == 0x1234ABCD );
    CHECK( layout.payload.timestamp_ms == 123456.789 );
    CHECK( layout.payload.source_frame_id == 136 + layout.payload.frame_id );
    // The host writer pads each size to 64 bytes; a camera writes the real size (see the recorded frame below)
    std::vector< size_t > const offsets = { 128, 15104, 15552, 75136, 76608, 314624 };
    REQUIRE( layout.tensors.size() == 6 );
    for( size_t i = 0; i < 6; ++i )
    {
        auto const & t = layout.tensors[i];
        CHECK( t.output_index == i );
        CHECK( t.dtype == RS2_NN_DTYPE_S16 );
        CHECK( t.offset == offsets[i] );
        CHECK( has_pattern( frame, t ) );
    }

    CHECK( parse( frame_file( "v3_02_ok_model_id_0_1024x313.bin" ) ).header.model_id == 0 );
    CHECK( parse( frame_file( "v3_03_ok_model_id_max_1024x313.bin" ) ).header.model_id == 0xFFFFFFFF );

    // Tensors in DSP order, their data not a multiple of 64 bytes
    auto const small_frame = frame_file( "v3_04_ok_small_unaligned_1024x1.bin" );
    layout = parse( small_frame );
    CHECK( layout.header.model_id == 7 );
    CHECK( 23 + layout.header.payload_size == 576 );
    REQUIRE( layout.tensors.size() == 3 );
    std::vector< std::vector< size_t > > const small_tensors = { { 2, 128, 6 }, { 0, 256, 130 }, { 1, 512, 64 } };
    for( size_t i = 0; i < 3; ++i )
    {
        auto const & t = layout.tensors[i];
        CHECK( t.output_index == small_tensors[i][0] );
        CHECK( t.offset == small_tensors[i][1] );
        CHECK( t.size == small_tensors[i][2] );
        CHECK( has_pattern( small_frame, t ) );
    }

    // Error frames: 64 bytes, error flag, no tensors
    std::vector< std::pair< std::string, int > > const errors = { { "v3_05_load_failed_gen0", 4 },
                                                                  { "v3_06_load_failed_model", 4 },
                                                                  { "v3_07_dsp_error", 5 },
                                                                  { "v3_08_no_metadata", 1 },
                                                                  { "v3_09_too_large", 2 },
                                                                  { "v3_10_tensor_invalid", 3 } };
    for( auto const & e : errors )
    {
        CAPTURE( e.first );
        layout = parse( frame_file( e.first + "_1024x313.bin" ) );
        CHECK( layout.payload.status == e.second );
        CHECK( layout.header.flags == nn::CUSTOM_NN_ERROR_FLAG );
        CHECK( layout.header.payload_size == 41 );
        CHECK( layout.tensors.empty() );
    }
    // After an install, before the reset: no model id and no generation, and the reset message
    layout = parse( frame_file( "v3_05_load_failed_gen0_1024x313.bin" ) );
    CHECK( layout.header.generation == 0 );
    CHECK( layout.header.model_id == 0 );
    CHECK_THAT( nn::custom_nn_status_message( layout.payload.status, layout.header.generation ), ContainsSubstring( "reset the camera" ) );
    layout = parse( frame_file( "v3_06_load_failed_model_1024x313.bin" ) );
    CHECK( layout.header.model_id == 42 );
    CHECK_THAT( nn::custom_nn_status_message( layout.payload.status, layout.header.generation ), ContainsSubstring( "Upload it again" ) );

    CHECK_THAT( error_of( frame_file( "v2_90_refused_ok_1024x313.bin" ) ), ContainsSubstring( "unsupported version 0x200" ) );
    CHECK_THAT( error_of( frame_file( "v3_91_bad_crc_1024x313.bin" ) ), ContainsSubstring( "CRC" ) );
    CHECK_THAT( error_of( frame_file( "v3_92_bad_size_1024x313.bin" ) ), ContainsSubstring( "payload size" ) );
    CHECK_THAT( error_of( frame_file( "v3_93_unknown_version_1024x313.bin" ) ), ContainsSubstring( "unsupported version 0x400" ) );
}


TEST_CASE( "custom nn frame recorded on a D585" )
{
    // DAMO-YOLO in slot 2, the second frame of a stream; a package without model_id gives 0
    auto const frame = frame_file( "d585_damo_yolo_1024x313.bin" );
    REQUIRE( frame.size() == 1024 * 313 );
    auto const layout = parse( frame );
    CHECK( layout.header.version == nn::CUSTOM_NN_VERSION );
    CHECK( layout.header.flags == 0 );
    CHECK( 23 + layout.header.payload_size == 320064 );
    CHECK( layout.header.model_id == 0 );
    CHECK( layout.header.generation == 0xD3B4048D );
    CHECK( layout.payload.status == 0 );
    CHECK( layout.payload.frame_id == 1 );
    CHECK( layout.payload.source_frame_id == 6 );
    // Each size is the tensor's element count x 2, the data padded with zeros to the next 64 bytes
    std::vector< size_t > const offsets = { 128, 15104, 15552, 75136, 76608, 314624 };
    std::vector< uint32_t > const sizes = { 14872, 338, 59488, 1352, 237952, 5408 };
    REQUIRE( layout.tensors.size() == 6 );
    for( size_t i = 0; i < 6; ++i )
    {
        auto const & t = layout.tensors[i];
        CHECK( t.output_index == i );
        CHECK( t.dtype == RS2_NN_DTYPE_S16 );
        CHECK( t.offset == offsets[i] );
        CHECK( t.size == sizes[i] );
        CHECK( std::all_of( frame.begin() + t.offset + t.size, frame.begin() + round64( t.offset + t.size ), []( uint8_t b ) { return b == 0; } ) );
    }
}
