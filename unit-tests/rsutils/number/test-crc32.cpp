// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:dependencies rsutils

#include <unit-tests/catch.h>
#include <rsutils/number/crc32.h>

#include <cstring>
#include <random>
#include <vector>


namespace {

// Byte-at-a-time reference: the classic table-free reflected CRC-32 (polynomial 0xedb88320)
uint32_t reference_crc32( const uint8_t * buf, size_t size )
{
    uint32_t crc = 0xFFFFFFFF;
    for( size_t i = 0; i < size; ++i )
    {
        crc ^= buf[i];
        for( int k = 0; k < 8; ++k )
            crc = ( crc & 1 ) ? ( 0xedb88320U ^ ( crc >> 1 ) ) : ( crc >> 1 );
    }
    return ~crc;
}

}  // namespace


TEST_CASE( "crc32 standard check value" )
{
    const char * check = "123456789";
    CHECK( rsutils::number::calc_crc32( reinterpret_cast< const uint8_t * >( check ), std::strlen( check ) )
           == 0xCBF43926U );
    CHECK( rsutils::number::calc_crc32( nullptr, 0 ) == 0U );
}

TEST_CASE( "crc32 matches the byte-wise reference for any length and alignment" )
{
    std::mt19937 rng( 1234 );
    std::vector< uint8_t > data( 4096 + 16 );
    for( auto & b : data )
        b = static_cast< uint8_t >( rng() );

    for( size_t offset = 0; offset < 8; ++offset )
        for( size_t size : { 0, 1, 7, 8, 9, 15, 16, 17, 63, 64, 65, 1000, 4096 } )
        {
            CAPTURE( offset, size );
            CHECK( rsutils::number::calc_crc32( data.data() + offset, size )
                   == reference_crc32( data.data() + offset, size ) );
        }
}
