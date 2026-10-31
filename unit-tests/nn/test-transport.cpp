// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:add-file ../../src/nn/nn-protocol.cpp
//#cmake:add-file ../../src/nn/nn-transport.cpp

#include "../catch.h"
#include <src/nn/nn-transport.h>
#include <src/librealsense-exception.h>

#include <cmath>
#include <deque>
#include <map>
#include <string>

using namespace librealsense;
using Catch::Matchers::ContainsSubstring;
using std::chrono::milliseconds;


// A device scripted per test: chunk replies by index, and a queue of NN_STATUS reads whose last entry repeats
class fake_transport : public nn_transport
{
public:
    struct chunk
    {
        uint32_t param4;
        size_t size;
        uint8_t first;
    };

    // The total limit leaves the quick failures plenty of headroom on slow (e.g. instrumented) CI runners
    fake_transport() : nn_transport( { milliseconds( 1 ), milliseconds( 1000 ) } ) {}

    int32_t send_chunk( uint16_t, uint32_t, uint32_t, uint32_t param4, const uint8_t * data, size_t size ) override
    {
        chunks.push_back( { param4, size, data[0] } );
        auto it = chunk_errors.find( chunks.size() - 1 );
        return it == chunk_errors.end() ? 0 : it->second;
    }

    int32_t read_status( nn::status & status ) override
    {
        ++status_reads;
        if( status_error )
            return status_error;
        if( next_status )
            status = next_status();
        else
        {
            status = statuses.front();
            if( statuses.size() > 1 )
                statuses.pop_front();
        }
        return 0;
    }

    std::vector< chunk > chunks;
    std::map< size_t, int32_t > chunk_errors;
    std::deque< nn::status > statuses;
    std::function< nn::status() > next_status;
    int32_t status_error = 0;
    size_t status_reads = 0;
};


static nn::status make_status( nn::upload_state state, uint32_t seq, uint8_t progress = 0, int32_t error = 0 )
{
    nn::status status = {};
    status.version = 0x0100;
    status.size = sizeof( status );
    status.state = state;
    status.progress = progress;
    status.error = error;
    status.upload_seq = seq;
    return status;
}


// Each byte holds its chunk index, so the chunks' order can be checked
static std::vector< uint8_t > make_table( size_t size )
{
    std::vector< uint8_t > table( size );
    for( size_t i = 0; i < size; ++i )
        table[i] = static_cast< uint8_t >( i / nn::CHUNK_SIZE );
    return table;
}


// Uploads a table and returns the exception message, or empty on success
static std::string upload( fake_transport & t, size_t size = 2501, std::vector< float > * progress = nullptr )
{
    try
    {
        t.send_table( nn::PACKAGE_TABLE_ID, 1, 0, make_table( size ),
                      [&]( float p ) { if( progress ) progress->push_back( p ); } );
    }
    catch( librealsense_exception const & e )
    {
        return e.what();
    }
    return {};
}


TEST_CASE( "upload and commit" )
{
    fake_transport t;
    t.statuses = { make_status( nn::upload_state::idle, 7 ),
                   make_status( nn::upload_state::validating, 8, 0 ),
                   make_status( nn::upload_state::writing_flash, 8, 50 ),
                   make_status( nn::upload_state::done, 8, 100 ) };
    std::vector< float > progress;
    CHECK( upload( t, 2501, &progress ) == "" );

    REQUIRE( t.chunks.size() == 3 );
    for( uint8_t i = 0; i < 3; ++i )
    {
        CHECK( t.chunks[i].param4 == ( 0x20000u | i ) );
        CHECK( t.chunks[i].first == i );
    }
    CHECK( t.chunks[0].size == 1000 );
    CHECK( t.chunks[2].size == 501 );

    // 90% for the chunks, the rest from the device's commit progress
    std::vector< float > const expected = { 0.3f, 0.6f, 0.9f, 0.9f, 0.95f, 1.f };
    REQUIRE( progress.size() == expected.size() );
    for( size_t i = 0; i < expected.size(); ++i )
        CHECK( std::abs( progress[i] - expected[i] ) < 1e-5f );
}


TEST_CASE( "refused before the first chunk" )
{
    fake_transport busy;
    busy.statuses = { make_status( nn::upload_state::writing_flash, 3 ) };
    CHECK_THROWS_AS( busy.send_table( nn::PACKAGE_TABLE_ID, 1, 0, make_table( 10 ), {} ), io_exception );
    CHECK_THAT( upload( busy ), ContainsSubstring( "another upload" ) );
    CHECK( busy.chunks.empty() );

    fake_transport unsupported;
    unsupported.status_error = -12;
    CHECK_THROWS_AS( unsupported.send_table( nn::PACKAGE_TABLE_ID, 1, 0, make_table( 10 ), {} ), not_implemented_exception );
    CHECK( unsupported.chunks.empty() );
}


TEST_CASE( "device errors" )
{
    SECTION( "on a chunk: the upload stops there" )
    {
        fake_transport t;
        t.statuses = { make_status( nn::upload_state::idle, 7 ) };
        t.chunk_errors[1] = -6;
        CHECK_THROWS_AS( t.send_table( nn::PACKAGE_TABLE_ID, 1, 0, make_table( 2501 ), {} ), invalid_value_exception );
        CHECK( t.chunks.size() == 2 );
        CHECK( t.status_reads == 1 );
    }
    SECTION( "in the commit" )
    {
        fake_transport t;
        t.statuses = { make_status( nn::upload_state::idle, 7 ), make_status( nn::upload_state::error, 8, 40, -15 ) };
        CHECK_THAT( upload( t ), ContainsSubstring( "writing the model to the camera failed" ) && ContainsSubstring( "(-15)" ) );
    }
    SECTION( "after the commit progress reached 100: never reported as complete" )
    {
        fake_transport t;
        t.statuses = { make_status( nn::upload_state::idle, 7 ), make_status( nn::upload_state::activating, 8, 100 ),
                       make_status( nn::upload_state::error, 8, 100, -5 ) };
        std::vector< float > progress;
        CHECK_THAT( upload( t, 2501, &progress ), ContainsSubstring( "(-5)" ) );
        REQUIRE( ! progress.empty() );
        CHECK( progress.back() < 1.f );
    }
}


TEST_CASE( "upload sequence number" )
{
    SECTION( "a leftover result from the previous upload is ignored" )
    {
        fake_transport t;
        t.statuses = { make_status( nn::upload_state::done, 7 ), make_status( nn::upload_state::done, 7 ),
                       make_status( nn::upload_state::error, 8, 0, -6 ) };
        CHECK_THAT( upload( t ), ContainsSubstring( "CRC mismatch (-6)" ) );
        CHECK( t.status_reads == 3 );
    }
    SECTION( "status of another upload is ignored" )
    {
        fake_transport t;
        t.statuses = { make_status( nn::upload_state::done, 7 ), make_status( nn::upload_state::error, 3, 0, -5 ),
                       make_status( nn::upload_state::done, 8 ) };
        CHECK( upload( t ) == "" );
        CHECK( t.status_reads == 3 );
    }
    SECTION( "wraps" )
    {
        fake_transport t;
        t.statuses = { make_status( nn::upload_state::idle, 0xFFFFFFFF ), make_status( nn::upload_state::done, 0 ) };
        CHECK( upload( t ) == "" );
    }
}


TEST_CASE( "long commit and lost device" )
{
    fake_transport t;
    auto const start = std::chrono::steady_clock::now();
    auto elapsed = [&] { return std::chrono::steady_clock::now() - start; };

    SECTION( "progress stays 0 for the whole flash write" )
    {
        t.next_status = [&]
        {
            return t.status_reads == 1  ? make_status( nn::upload_state::idle, 7 )
                 : t.status_reads < 30  ? make_status( nn::upload_state::writing_flash, 8 )
                                        : make_status( nn::upload_state::done, 8, 100 );
        };
        CHECK( upload( t ) == "" );
        CHECK( t.status_reads == 30 );
    }
    SECTION( "never completes" )
    {
        t.statuses = { make_status( nn::upload_state::idle, 7 ), make_status( nn::upload_state::writing_flash, 8 ) };
        CHECK_THAT( upload( t ), ContainsSubstring( "did not complete" ) );
        CHECK( elapsed() >= milliseconds( 1000 ) );
    }
    SECTION( "device restarted: its status no longer matches the upload" )
    {
        t.statuses = { make_status( nn::upload_state::idle, 7 ), make_status( nn::upload_state::idle, 0 ) };
        CHECK_THAT( upload( t ), ContainsSubstring( "did not complete" ) );
        CHECK( elapsed() >= milliseconds( 1000 ) );
    }
    SECTION( "command timeouts while writing: still busy" )
    {
        t.next_status = [&]
        {
            if( t.status_reads > 2 && t.status_reads < 6 )
                throw io_exception( "timed out" );
            return t.status_reads == 1  ? make_status( nn::upload_state::idle, 7 )
                 : t.status_reads < 8   ? make_status( nn::upload_state::writing_flash, 8 )
                                        : make_status( nn::upload_state::done, 8, 100 );
        };
        CHECK( upload( t ) == "" );
        CHECK( elapsed() < milliseconds( 1000 ) );
    }
    SECTION( "device lost while polling" )
    {
        t.next_status = [&]
        {
            if( t.status_reads > 2 )
                throw io_exception( "device disconnected" );
            return make_status( t.status_reads == 1 ? nn::upload_state::idle : nn::upload_state::writing_flash,
                                t.status_reads == 1 ? 7 : 8 );
        };
        CHECK_THAT( upload( t ), ContainsSubstring( "no detection model" ) && ContainsSubstring( "device disconnected" ) );
        CHECK( elapsed() >= milliseconds( 1000 ) );
    }
    SECTION( "status read refused while polling" )
    {
        t.next_status = [&]
        {
            t.status_error = -21;  // from the next read on
            return make_status( nn::upload_state::idle, 7 );
        };
        CHECK_THAT( upload( t ), ContainsSubstring( "did not complete" ) && ContainsSubstring( "allocate resources" ) );
        CHECK( t.status_reads == 2 );
    }
}
