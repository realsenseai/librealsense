// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:add-file ../../src/nn/nn-protocol.cpp
//#cmake:add-file ../../src/nn/nn-loader.cpp
//#cmake:add-file ../../src/nn/nn-info.cpp

#include "../catch.h"
#include <src/nn/nn-protocol.h>
#include <src/nn/nn-loader.h>
#include <src/nn/nn-info.h>
#include <rsutils/number/crc32.h>
#include <rsutils/json.h>
#include <src/librealsense-exception.h>

#include <cstddef>
#include <cstring>
#include <string>

using namespace librealsense;
using Catch::Matchers::ContainsSubstring;


TEST_CASE( "chunk numbering" )
{
    CHECK( nn::chunk_count( 0 ) == 0 );
    CHECK( nn::chunk_count( 1 ) == 1 );
    CHECK( nn::chunk_count( 1000 ) == 1 );
    CHECK( nn::chunk_count( 1001 ) == 2 );

    CHECK( nn::chunk_param( 1, 0 ) == 0 );
    CHECK( nn::chunk_param( 2, 1 ) == 0x00010001 );
    CHECK( nn::chunk_param( 3, 1 ) == 0x00020001 );

    // The largest file fills exactly 65,536 chunks
    CHECK( nn::chunk_count( nn::MAX_FILE_SIZE + nn::TABLE_HEADER_SIZE ) == nn::MAX_CHUNKS );
    CHECK( nn::chunk_count( nn::MAX_FILE_SIZE + 1 + nn::TABLE_HEADER_SIZE ) == nn::MAX_CHUNKS + 1 );
    CHECK( nn::chunk_param( nn::MAX_CHUNKS, 0 ) == 0xFFFF0000 );
    CHECK( nn::chunk_param( nn::MAX_CHUNKS, nn::MAX_CHUNKS - 1 ) == 0xFFFFFFFF );

    CHECK( nn::target_param( RS2_NN_TARGET_FLASH ) == 1 );
    CHECK( nn::target_param( RS2_NN_TARGET_DDR ) == 2 );
    CHECK( nn::slot_param( 0, true ) == 0x10000 );
    CHECK( nn::slot_param( 3, false ) == 3 );
}


TEST_CASE( "table header" )
{
    std::string const file = "123456789";
    auto table = nn::build_table( nn::PACKAGE_TABLE_ID, reinterpret_cast< const uint8_t * >( file.data() ), file.size() );
    REQUIRE( table.size() == nn::TABLE_HEADER_SIZE + file.size() );

    auto u16 = [&]( size_t offset ) { uint16_t v; std::memcpy( &v, table.data() + offset, sizeof( v ) ); return v; };
    auto u32 = [&]( size_t offset ) { uint32_t v; std::memcpy( &v, table.data() + offset, sizeof( v ) ); return v; };
    // Version 0x0100 as bytes 00 01, as FW sends it in NN_STATUS
    CHECK( table[0] == 0x00 );
    CHECK( table[1] == 0x01 );
    CHECK( u16( 2 ) == nn::PACKAGE_TABLE_ID );
    CHECK( u32( 4 ) == file.size() );
    CHECK( u32( 8 ) == 0 );
    CHECK( u32( 12 ) == 0xCBF43926 );  // CRC-32/ISO-HDLC check value
    CHECK( std::string( table.begin() + nn::TABLE_HEADER_SIZE, table.end() ) == file );
}


TEST_CASE( "upload arguments" )
{
    CHECK_NOTHROW( validate_nn_upload_args( 1, RS2_NN_TARGET_FLASH, 0, true ) );
    CHECK_NOTHROW( validate_nn_upload_args( nn::MAX_FILE_SIZE, RS2_NN_TARGET_FLASH, 0, true ) );

    CHECK_THROWS_AS( validate_nn_upload_args( 0, RS2_NN_TARGET_FLASH, 0, true ), invalid_value_exception );
    CHECK_THROWS_AS( validate_nn_upload_args( nn::MAX_FILE_SIZE + 1, RS2_NN_TARGET_FLASH, 0, true ), invalid_value_exception );
    CHECK_THROWS_AS( validate_nn_upload_args( 1, RS2_NN_TARGET_DDR, 0, true ), not_implemented_exception );
    CHECK_THROWS_AS( validate_nn_upload_args( 1, RS2_NN_TARGET_FLASH, 1, true ), not_implemented_exception );
    CHECK_THROWS_AS( validate_nn_upload_args( 1, RS2_NN_TARGET_FLASH, -1, true ), not_implemented_exception );
    CHECK_THROWS_AS( validate_nn_upload_args( 1, RS2_NN_TARGET_FLASH, 0, false ), not_implemented_exception );
}


static std::string thrown_message( int32_t code )
{
    try
    {
        nn::throw_error( code );
    }
    catch( librealsense_exception const & e )
    {
        return e.what();
    }
    return {};
}


TEST_CASE( "error mapping" )
{
    CHECK_THROWS_AS( nn::throw_error( -1 ), not_implemented_exception );
    CHECK_THROWS_AS( nn::throw_error( -4 ), not_implemented_exception );
    CHECK_THAT( thrown_message( -4 ), ContainsSubstring( "locked" ) );
    CHECK_THROWS_AS( nn::throw_error( -12 ), not_implemented_exception );
    CHECK_THROWS_AS( nn::throw_error( -19 ), not_implemented_exception );
    CHECK_THROWS_AS( nn::throw_error( -10 ), invalid_value_exception );
    CHECK_THAT( thrown_message( -10 ), ContainsSubstring( "too large for this camera's model slot" ) && ContainsSubstring( "(-10)" ) );
    CHECK_THROWS_AS( nn::throw_error( -34 ), not_implemented_exception );
    CHECK_THAT( thrown_message( -34 ), ContainsSubstring( "locked" ) );
    CHECK_THAT( nn::error_message( -1 ), ContainsSubstring( "invalid command" ) );
    CHECK_THAT( nn::error_message( -5 ), ContainsSubstring( "not valid for this camera" ) && ContainsSubstring( "converted for" ) );
    CHECK_THAT( nn::error_message( -15 ), ContainsSubstring( "no detection model" ) );
    CHECK_THAT( nn::error_message( -19 ), ContainsSubstring( "package is not supported" ) );
    CHECK_THAT( nn::error_message( -21 ), ContainsSubstring( "allocate resources" ) );
    CHECK_THAT( nn::error_message( -41 ), ContainsSubstring( "busy" ) );
    CHECK( nn::error_message( -99 ) == "device error -99" );
}


TEST_CASE( "status layout" )
{
    CHECK( offsetof( nn::status, state ) == 4 );
    CHECK( offsetof( nn::status, progress ) == 6 );
    CHECK( offsetof( nn::status, error ) == 8 );
    CHECK( offsetof( nn::status, upload_seq ) == 12 );
    CHECK( offsetof( nn::status, elapsed_ms ) == 24 );

    CHECK_FALSE( nn::is_busy( nn::upload_state::idle ) );
    CHECK( nn::is_busy( nn::upload_state::receiving ) );
    CHECK( nn::is_busy( nn::upload_state::activating ) );
    CHECK_FALSE( nn::is_busy( nn::upload_state::done ) );
    CHECK_FALSE( nn::is_busy( nn::upload_state::error ) );
}


TEST_CASE( "status reply" )
{
    nn::status sent = {};
    sent.state = nn::upload_state::writing_flash;
    sent.progress = 40;
    sent.upload_seq = 9;
    sent.model_state = 2;  // not used yet: any value parses
    auto const bytes = reinterpret_cast< const uint8_t * >( &sent );
    auto reply = nn::build_table( nn::STATUS_TABLE_ID, bytes, sizeof( sent ) );
    REQUIRE( reply.size() == 44 );

    auto status = nn::parse_status( reply );
    CHECK( status.state == nn::upload_state::writing_flash );
    // The header version is not checked: the other byte order parses too
    auto other_version = reply;
    other_version[0] = 0x01;
    other_version[1] = 0x00;
    CHECK( nn::parse_status( other_version ).upload_seq == 9 );
    CHECK( status.progress == 40 );
    CHECK( status.upload_seq == 9 );
    CHECK( status.model_state == 2 );

    // A longer status (a later version) is accepted
    std::vector< uint8_t > longer( bytes, bytes + sizeof( sent ) );
    longer.resize( sizeof( sent ) + 4 );
    CHECK( nn::parse_status( nn::build_table( nn::STATUS_TABLE_ID, longer.data(), longer.size() ) ).upload_seq == 9 );

    auto parse_error = []( std::vector< uint8_t > const & reply ) -> std::string
    {
        try
        {
            nn::parse_status( reply );
        }
        catch( librealsense_exception const & e )
        {
            return e.what();
        }
        return {};
    };
    
    auto corrupt = reply;
    // Intentionally corrupt the last byte to test error handling
    corrupt.back() ^= 1;
    CHECK_THAT( parse_error( corrupt ), ContainsSubstring( "invalid NN status" ) );
    CHECK_THAT( parse_error( nn::build_table( nn::PACKAGE_TABLE_ID, bytes, sizeof( sent ) ) ),
                ContainsSubstring( "invalid NN status" ) );
    CHECK_THAT( parse_error( nn::build_table( nn::STATUS_TABLE_ID, bytes, sizeof( sent ) - 1 ) ),
                ContainsSubstring( "too short" ) );
    auto truncated = nn::build_table( nn::STATUS_TABLE_ID, longer.data(), longer.size() );
    truncated.pop_back();
    CHECK_THAT( parse_error( truncated ), ContainsSubstring( "invalid NN status" ) );
    // The status alone, without the header, as before FW added it
    CHECK_THAT( parse_error( std::vector< uint8_t >( bytes, bytes + sizeof( sent ) ) ), ContainsSubstring( "too short" ) );
}

// A model's metadata as the conversion service writes it (YOLOv8n, outputs listed out of order)
static const std::string YOLO_METADATA = R"({"format": "custom_nn_v1", "model": "CustomNN", "model_id": null,
  "inputs": [{"buffer_size": 248832, "dequant_scale": 1.0, "dtype": "u8", "format": "NHWC", "index": 0, "name": "images",
              "sizes": [288, 288, 3], "strides": [864, 3, 1]}],
  "outputs": [{"buffer_size": 13632, "dequant_scale": 84.66925, "dtype": "s16", "format": "NHWC", "index": 1,
               "name": "boxes", "quant_scale": 0.0118, "sizes": [1701, 1, 4], "strides": [4, 4, 1], "zero_point": 0},
              {"buffer_size": 272192, "dequant_scale": 32767.0, "dtype": "s16", "format": "NHWC", "index": 0,
               "name": "scores", "quant_scale": 3.05e-05, "sizes": [1701, 1, 80], "strides": [80, 80, 1], "zero_point": 0}]})";


// An NN_INFO reply: the block, then the metadata; extra bytes in the block stand for a later version
static std::vector< uint8_t > make_info( const std::string & metadata, bool present = true, size_t extra = 0,
                                         uint32_t generation = 0 )
{
    nn::info block = {};
    block.version = 0x0100;
    block.size = uint16_t( sizeof( block ) + extra );
    block.slot = nn::CUSTOM_SLOT;
    block.present = present;
    block.model_state = present ? 1 : 0;
    block.generation = generation ? generation
                                  : rsutils::number::calc_crc32( (const uint8_t *)metadata.data(), metadata.size() );
    block.frame_size = 289344;
    block.frame_height = 283;
    block.metadata_size = uint32_t( metadata.size() );
    std::vector< uint8_t > payload( (const uint8_t *)&block, (const uint8_t *)&block + sizeof( block ) );
    payload.resize( payload.size() + extra, 0xEE );
    payload.insert( payload.end(), metadata.begin(), metadata.end() );
    return nn::build_table( nn::INFO_TABLE_ID, payload.data(), payload.size() );
}


static std::string info_error( std::vector< uint8_t > const & reply )
{
    try
    {
        nn::parse_info( reply );
    }
    catch( librealsense_exception const & e )
    {
        return e.what();
    }
    return {};
}


TEST_CASE( "info reply" )
{
    CHECK( offsetof( nn::info, generation ) == 8 );
    CHECK( offsetof( nn::info, metadata_size ) == 20 );

    auto stored = nn::parse_info( make_info( YOLO_METADATA ) );
    CHECK( stored.block.present == 1 );
    CHECK( stored.block.frame_height == 283 );
    CHECK( stored.metadata == YOLO_METADATA );

    // A later, longer block: the metadata follows the block's own size
    CHECK( nn::parse_info( make_info( YOLO_METADATA, true, 8 ) ).metadata == YOLO_METADATA );

    // No model: no metadata, generation 0
    auto none = nn::parse_info( make_info( "", false, 0, 0 ) );
    CHECK( none.block.present == 0 );
    CHECK( none.metadata.empty() );

    CHECK_THAT( info_error( make_info( YOLO_METADATA, true, 0, 0x12345678 ) ), ContainsSubstring( "generation" ) );
    auto status_table = nn::build_table( nn::STATUS_TABLE_ID, make_info( "" ).data() + 16, 24 );
    CHECK_THAT( info_error( status_table ), ContainsSubstring( "invalid NN info table" ) );

    // A metadata size past the table
    auto reply = make_info( YOLO_METADATA );
    reply.resize( reply.size() - 10 );
    auto shorter = nn::build_table( nn::INFO_TABLE_ID, reply.data() + 16, reply.size() - 16 );
    CHECK_THAT( info_error( shorter ), ContainsSubstring( "bytes of metadata" ) );
}


static std::string description_error( const std::string & metadata )
{
    try
    {
        nn::parse_model_description( metadata, 1 );
    }
    catch( librealsense_exception const & e )
    {
        return e.what();
    }
    return {};
}


TEST_CASE( "model description" )
{
    auto desc = nn::parse_model_description( YOLO_METADATA, 0x2472D988 );
    CHECK( desc->generation == 0x2472D988 );
    CHECK( desc->model == "CustomNN" );
    REQUIRE( desc->outputs.size() == 2 );
    // Placed by index, as the frame's tensors carry it
    auto const & scores = desc->outputs[0];
    CHECK( scores.name == "scores" );
    CHECK( scores.dtype == RS2_NN_DTYPE_S16 );
    CHECK( scores.sizes == std::vector< int >{ 1701, 1, 80 } );
    CHECK( scores.format == "NHWC" );
    CHECK( scores.strides == std::vector< int >{ 80, 80, 1 } );
    CHECK( scores.buffer_size == 272192 );
    CHECK( scores.dequant_scale == 32767.f );
    CHECK( desc->outputs[1].name == "boxes" );

    auto with = [&]( const std::string & from, const std::string & to )
    {
        auto md = YOLO_METADATA;
        md.replace( md.find( from ), from.size(), to );
        return md;
    };
    CHECK_THAT( description_error( with( "custom_nn_v1", "custom_nn_v9" ) ), ContainsSubstring( "unsupported format" ) );
    CHECK_THAT( description_error( with( "\"index\": 1", "\"index\": 0" ) ), ContainsSubstring( "output index 0" ) );
    CHECK_THAT( description_error( with( "\"dtype\": \"s16\"", "\"dtype\": \"bf16\"" ) ), ContainsSubstring( "bf16" ) );
    CHECK_THAT( description_error( with( "\"name\": \"boxes\", ", "" ) ), ContainsSubstring( "name" ) );
    CHECK_THAT( description_error( "not json" ), ContainsSubstring( "Invalid Custom NN model metadata" ) );

    // Strides: dense (a size-1 dimension's stride is never used), padded, or not given
    CHECK( scores.dense );
    CHECK( desc->outputs[1].dense );
    CHECK( nn::parse_model_description( with( "\"strides\": [80, 80, 1]", "\"strides\": [80, 7, 1]" ), 0 )->outputs[0].dense );
    CHECK_FALSE( nn::parse_model_description( with( "\"strides\": [80, 80, 1]", "\"strides\": [96, 96, 1]" ), 0 )->outputs[0].dense );
    CHECK_FALSE( nn::parse_model_description( with( "\"strides\": [80, 80, 1]", "\"strides\": [80, 1]" ), 0 )->outputs[0].dense );
    CHECK( nn::parse_model_description( with( ", \"strides\": [80, 80, 1]", "" ), 0 )->outputs[0].dense );
}


TEST_CASE( "model info json" )
{
    auto j = rsutils::json::parse( nn::to_json( nn::parse_info( make_info( YOLO_METADATA ) ) ) );
    CHECK( j["present"] == true );
    CHECK( j["frame_height"] == 283 );
    CHECK( j["metadata"]["outputs"].size() == 2 );

    auto none = rsutils::json::parse( nn::to_json( nn::parse_info( make_info( "", false, 0, 0 ) ) ) );
    CHECK( none["present"] == false );
    CHECK_FALSE( none.contains( "metadata" ) );
}
