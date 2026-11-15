// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include <rsutils/number/crc32.h>

#include <cstring>


namespace rsutils {
namespace number {


namespace {

// Standard (IEEE 802.3) reflected CRC-32, polynomial 0xedb88320. Slice-by-8 tables: table[0] is
// the classic byte-wise table; table[k] advances a byte through k further zero bytes, so eight
// input bytes are folded per step. Large payloads (device point clouds, several MB per frame)
// are validated on every frame, so the byte-wise loop is too slow for them.
struct crc32_tables
{
    uint32_t table[8][256];

    crc32_tables()
    {
        for( uint32_t i = 0; i < 256; ++i )
        {
            uint32_t c = i;
            for( int k = 0; k < 8; ++k )
                c = ( c & 1 ) ? ( 0xedb88320U ^ ( c >> 1 ) ) : ( c >> 1 );
            table[0][i] = c;
        }
        for( uint32_t i = 0; i < 256; ++i )
            for( int s = 1; s < 8; ++s )
                table[s][i] = ( table[s - 1][i] >> 8 ) ^ table[0][table[s - 1][i] & 0xff];
    }
};

crc32_tables const & tables()
{
    static const crc32_tables t;
    return t;
}

}  // namespace


/// Calculate CRC code for arbitrary characters buffer
uint32_t calc_crc32( const uint8_t * buf, size_t bufsize )
{
    auto const & t = tables().table;
    uint32_t crc = 0xFFFFFFFF;

#if ! defined( __BYTE_ORDER__ ) || ( __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ )
    while( bufsize >= 8 )
    {
        uint32_t lo, hi;
        std::memcpy( &lo, buf, sizeof( lo ) );
        std::memcpy( &hi, buf + 4, sizeof( hi ) );
        lo ^= crc;
        crc = t[7][lo & 0xff] ^ t[6][( lo >> 8 ) & 0xff] ^ t[5][( lo >> 16 ) & 0xff] ^ t[4][lo >> 24]
            ^ t[3][hi & 0xff] ^ t[2][( hi >> 8 ) & 0xff] ^ t[1][( hi >> 16 ) & 0xff] ^ t[0][hi >> 24];
        buf += 8;
        bufsize -= 8;
    }
#endif

    for( ; bufsize; --bufsize, ++buf )
        crc = t[0][( crc ^ *buf ) & 0xff] ^ ( crc >> 8 );
    return ~crc;
}


}  // namespace number
}  // namespace rsutils
