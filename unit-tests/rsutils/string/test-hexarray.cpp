// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

//#cmake:dependencies rsutils

#include <unit-tests/test.h>
#include <rsutils/string/hexarray.h>
#include <rsutils/string/hexdump.h>
#include <rsutils/string/from.h>
#include <rsutils/string/slice.h>
#include <rsutils/json.h>
#include <ostream>

using rsutils::string::hexdump;
using rsutils::string::hexarray;
using rsutils::string::from;
using byte = uint8_t;
using bytearray = std::vector< byte >;


namespace {


bool is_little_endian()
{
    // Only since C++20
    //return ( std::endian::native == std::endian::little );

    union
    {
        uint32_t i;
        uint8_t b[4];
    }
    bint = { 0x01020304 };

    return bint.b[0] == 4;
}

std::string to_string( hexdump const & hex )
{
    return from() << hex;
}
std::string to_string( hexdump::_format const & hexf )
{
    return from() << hexf;
}


}  // namespace


TEST_CASE( "hexdump", "[hexarray]" )
{
    SECTION( "buffers get native (little-endian) byte ordering" )
    {
        int i = 0x04030201;
        CHECK( to_string( hexdump( reinterpret_cast< byte const * >( &i ), sizeof( i ) ) ) == "01020304" );  // little-endian
    }
    SECTION( "single values show the way you'd expect to read them, in big-endian" )
    {
        CHECK( to_string( hexdump( 'a' ) ) == "61" );
        CHECK( to_string( hexdump( byte( 0 ) ) ) == "00" );
        CHECK( to_string( hexdump( 0 ) ) == "00000000" );
        CHECK( to_string( hexdump( 0x04030201 ) ) == "04030201" );
        CHECK( to_string( hexdump( (void *) (0x123) ) ) == "0000000000000123" );  // pointers, too
    }
    SECTION( "floating point values aren't readable -- they stay at native endian-ness" )
    {
        union { uint32_t i; float f; byte b[4]; } u = { 0x40b570a4 };  // 5.67
        CHECK( to_string( hexdump( u ) ) == "a470b540" );
        CHECK( ! hexdump( u )._big_endian );
        CHECK( to_string( hexdump( u.b ) ) == "a470b540" );  // array
        CHECK( to_string( hexdump( u.f ) ) == "a470b540" );  // float
        CHECK( to_string( hexdump( u.i ) ) == "40b570a4" );
        CHECK( int( reinterpret_cast< byte const * >( &u )[0] ) == 0xa4 );
    }
    SECTION( "struct" )
    {
        // one extra byte at the end
        struct { uint32_t i; uint16_t w; uint8_t b; } s{ 0x01020304, 0x0506, 0x78 };
        auto str = to_string( hexdump( s ) );
        CHECK( str.length() == sizeof( s ) * 2 );
        CHECK( str.substr( 0, sizeof( s ) * 2 - 2 ) == "04030201060578" );
    }
    SECTION( "struct, packed" )
    {
#pragma pack( push, 1 )
        struct { uint32_t i; uint16_t w; uint8_t b; } s{ 0x01020304, 0x0506, 0x78 };
#pragma pack( pop )
        CHECK( to_string( hexdump( s ) ) == "04030201060578" );
    }
    SECTION( "case" )
    {
        int i = 0x0a0b0c0d;
        CHECK( to_string( hexdump( i ) ) == "0a0b0c0d" );
        CHECK( ( from() << std::uppercase << hexdump( i ) ).str() == "0A0B0C0D" );
    }
}

TEST_CASE( "hexdump of bytearray", "[hexarray]" )
{
    bytearray ba;
    for( byte i = 0; i < 10; ++i )
        ba.push_back( i );
    std::string ba_string = "00010203040506070809";

    SECTION( "vector repr is not the bytearray" )
    {
        CHECK_FALSE( to_string( hexdump( ba ) ) == ba_string );
    }
    SECTION( "have to use buffer syntax" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ) ) == ba_string );
    }
    SECTION( "zero length?" )
    {
        CHECK( to_string( hexdump( ba.data(), 0 ) ) == "" );
    }
}

TEST_CASE( "hexdump ellipsis", "[hexarray]" )
{
    bytearray ba;
    for( byte i = 0; i < 10; ++i )
        ba.push_back( i );

    SECTION( "have to use buffer syntax" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size()-1 ) ) == "000102030405060708" );
    }
    SECTION( "ellipsis only if we ended before end of buffer" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).max_bytes( ba.size() - 1 ) ) == "000102030405060708..." );
    }
    SECTION( "max-bytes zero means no max bytes" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).max_bytes( 0 ) ) == "00010203040506070809" );
    }
    SECTION( "max-bytes greater than length should have no effect" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).max_bytes( ba.size()+1 ) ) == "00010203040506070809" );
    }
}

TEST_CASE( "hexdump gap", "[hexarray]" )
{
    bytearray ba;
    for( byte i = 0; i < 5; ++i )
        ba.push_back( i );

    SECTION( "gap of 0 is no gap" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).gap(0) ) == "0001020304" );
    }
    SECTION( "gap >= length should have no effect" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).gap( ba.size() ) ) == "0001020304" );
    }
    SECTION( "gap of one" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).gap( 1 ) ) == "00 01 02 03 04" );
    }
    SECTION( "gap of two" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).gap( 2, '-' ) ) == "0001-0203-04" );
    }
    SECTION( "gap of three" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).gap( 3 ) ) == "000102 0304" );
    }
    SECTION( "gap of four" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).gap( 4 ) ) == "00010203 04" );
    }
    SECTION( "gap, max-bytes four" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).gap( 4 ).max_bytes( 4 ) ) == "00010203..." );
    }
}

TEST_CASE( "hexdump format", "[hexarray]" )
{
    bytearray ba;
    for( byte i = 0; i < 5; ++i )
        ba.push_back( i );

    SECTION( "no format == no output" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "" ) ) == "" );
    }
    SECTION( "even with max-bytes" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).max_bytes( 1 ).format( "" ) ) == "" );
    }
    SECTION( "output just as many bytes as you ask" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "{2}" ) ) == "0001" );
    }
    SECTION( "but no more than there are" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "{10}" ) ) == "0001020304" );
    }
    SECTION( "even with max-bytes" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).max_bytes(2).format( "{3}" ) ) == "0001" );
    }
    SECTION( "gap doesn't matter, either" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).gap( 1 ).format( "{3}" ) ) == "000102" );
    }
    SECTION( "one following the other" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "{1}{2}" ) ) == "000102" );
    }
    SECTION( "with extra characters" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "[{1} -> {2}]" ) ) == "[00 -> 0102]" );
    }
    SECTION( "invalid directives show {error} and do not throw" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "[{hmm} {} {:}]" ) ) == "[{error} {error} {error}]" );
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "[{ {} }]" ) ) == "[{error}]" );
    }
    SECTION( "skip bytes with {+#}" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "{1}{+2}{5}" ) ) == "000304" );
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "{1}{+5}{2}" ) ) == "00" );
    }
    SECTION( "other syntax: \\{ etc." )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "\\{}" ) ) == "{}" );
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "\\" ) ) == "\\" );
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "{1000000}" ) ) == "0001020304" );
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "{1000000.}" ) ) == "{error}" );
    }
}

TEST_CASE( "hexdump format {i}", "[hexarray]" )
{
    uint32_t i4 = 0x01020304;
    int32_t negi4 = int32_t( ~0x01020304 + 1 );
    auto i8 = 0x0102030405060708ull;

    SECTION( "our platforms should be little endian" )
    {
        CHECK( to_string( hexdump( i4 ) ) == "01020304" );
        CHECK( to_string( hexdump( i4 ).format( "{4}" ) ) == "04030201" );
    }
    SECTION( "{-#} to reverse order (big-endian)" )
    {
        CHECK( to_string( hexdump( i4 ).format( "{-4}" ) ) == "01020304" );
    }
    SECTION( "{0#} implies big-endian" )
    {
        // 0x100 = 00 01 00 00; removing leading 0s doesn't make sense (we get '1') so removing leading 0s should also
        // imply big-endian!
        CHECK( to_string( hexdump( 0x100 ).format( "{4}" ) ) == "00010000" );
        CHECK( to_string( hexdump( 0x100 ).format( "{2}" ) ) == "0001" );
        CHECK( to_string( hexdump( 0x100 ).format( "{02}" ) ) == "100" );
        CHECK( to_string( hexdump( 0x1 ).format( "{01}" ) ) == "1" );

        CHECK( to_string( hexdump( i4 ).format( "{04}" ) ) == "1020304" );
        CHECK( to_string( hexdump( i4 ).format( "{-04}" ) ) == to_string( hexdump( i4 ).format( "{04}" ) ) );
    }
    SECTION( "signed integral value" )
    {
        i4 = 1020304;  // decimal
        i8 = 1020304050607080910;
        CHECK( to_string( hexdump( i4 ).format( "{-4}" ) ) == "000f9190" );
        CHECK( to_string( hexdump( i4 ).format( "{-04}" ) ) == "f9190" );
        CHECK( to_string( hexdump( i4 ).format( "{4i}" ) ) == "1020304" );
        CHECK( to_string( hexdump( i4 ).format( "{3i}" ) ) == "{error:1/2/4/8}" );  // non-integral size
        CHECK( to_string( hexdump( i4 ).format( "{1}" ) ) == "90" );
        CHECK( to_string( hexdump( i4 ).format( "{1u}" ) ) == "144" );  // 9*16
        CHECK( to_string( hexdump( i4 ).format( "{1i}" ) ) == "-112" );
        CHECK( to_string( hexdump( i4 ).format( "{2u}" ) ) == "37264" );
        CHECK( to_string( hexdump( i4 ).format( "{5i}" ) ) == "{error:1/2/4/8}" );
        CHECK( to_string( hexdump( i4 ).format( "{6i}" ) ) == "{error:1/2/4/8}" );
        CHECK( to_string( hexdump( i4 ).format( "{7i}" ) ) == "{error:1/2/4/8}" );
        CHECK( to_string( hexdump( i8 ).format( "{-8}" ) ) == "0e28d920d353c5ce" );
        CHECK( to_string( hexdump( i8 ).format( "{8i}" ) ) == "1020304050607080910" );
    }
    SECTION( "unsigned integral value" )
    {
        CHECK( to_string( hexdump( negi4 ).format( "{4}" ) ) == "fcfcfdfe" );
        CHECK( to_string( hexdump( negi4 ).format( "{4i}" ) ) == "-16909060" );  // or -0x01020304
        CHECK( to_string( hexdump( negi4 ).format( "{4u}" ) ) == "4278058236" );
        CHECK( to_string( hexdump( negi4 ).format( "{2i}" ) ) == "-772" );
        CHECK( to_string( hexdump( negi4 ).format( "{1i}" ) ) == "-4" );
    }
    SECTION( "not enough bytes" )
    {
        CHECK( to_string( hexdump( i4 ).format( "{8i}" ) ) == "{error:not enough bytes}" );
    }
}

TEST_CASE( "hexdump format {f}", "[hexarray]" )
{
    float f = 102.03f;
    double d = 12345678.91011;

    SECTION( "{4f} for floats" )
    {
        CHECK( to_string( hexdump( f ) ) == "5c0fcc42" );
        CHECK( to_string( hexdump( f ).format( "{4f}" ) ) == "102.029999" );
    }
    SECTION( "{8f} for doubles" )
    {
        CHECK( to_string( hexdump( d ) ) == "029f1fdd298c6741" );
        CHECK( to_string( hexdump( d ).format( "{8f}" ) ) == "12345678.910110" );
    }
    SECTION( "invalid size" )
    {
        CHECK( to_string( hexdump( f ).format( "{3f}" ) ) == "{error:4/8}");
    }
}

TEST_CASE( "hexdump format {repeat:}", "[hexarray]" )
{
    bytearray ba;
    for( byte i = 0; i < 10; ++i )
        ba.push_back( i );

    SECTION( "basic repeat" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "[{2}{repeat:} {2}{:}]" ) ) == "[0001 0203 0405 0607 0809]" );
    }
    SECTION( "without {:}, doesn't repeat" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "[{2}{repeat:} {2}]" ) ) == "[0001 0203]" );
    }
    SECTION( "nothing inside repeat will repeat forever unless we add a skip or limit reps" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "[{2}{repeat:}{+2}{:}]" ) ) == "[0001]" );
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "[{2}{repeat:2}{:}]" ) ) == "[0001]" );
    }
    SECTION( "only twice, no ..." )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "[{2}{repeat:2} {2}{:}]" ) ) == "[0001 0203 0405]" );
    }
    SECTION( "want ...? have to say it" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).format( "[{2}{repeat:2} {2}{:?}]" ) ) == "[0001 0203 0405...]" );
    }
    SECTION( "but only if you don't reach the end" )
    {
        CHECK( to_string( hexdump( ba.data(), 6 ).format( "[{2}{repeat:2} {2}{:?}]" ) ) == "[0001 0203 0405]" );
    }
    SECTION( "with max-size" )
    {
        CHECK( to_string( hexdump( ba.data(), ba.size() ).max_bytes(3).format( "[{2}{repeat:2} {2}{:?}]" ) ) == "[0001 02...]" );
        CHECK( to_string( hexdump( ba.data(), ba.size() ).max_bytes(3).format( "[{2}{repeat:2} {2}{:}]" ) ) == "[0001 02]" );
        CHECK( to_string( hexdump( ba.data(), ba.size() ).max_bytes(3).format( "[{2}{repeat:2} {2}{:?xx}]" ) ) == "[0001 02xx]" );
    }
}

TEST_CASE( "hexarray", "[hexarray]" )
{
    bytearray ba;
    for( byte i = 0; i < 10; ++i )
        ba.push_back( i );
    std::string const ba_string = to_string( hexdump( ba.data(), ba.size() ) );

    SECTION( "same as hexdump" )
    {
        CHECK( hexarray::to_string( ba ) == ba_string );
    }
    SECTION( "but can also read hexarrays" )
    {
        CHECK( hexarray::from_string( ba_string ).get_bytes() == ba );
    }
    SECTION( "case insensitive" )
    {
        CHECK( hexarray::from_string( { "0A", 2 } ).to_string() == "0a" );
    }
    SECTION( "empty string is an empty array" )
    {
        CHECK( hexarray::from_string( { "abc", size_t(0) } ).to_string() == "" );
    }
    SECTION( "throws on invalid chars" )
    {
        CHECK_THROWS( hexarray::from_string( std::string( "1" ) ) );  // invalid length
        CHECK_NOTHROW( hexarray::from_string( std::string( "01" ) ) );
        CHECK_NOTHROW( hexarray::from_string( std::string( "02" ) ) );
        CHECK_NOTHROW( hexarray::from_string( std::string( "03" ) ) );
        CHECK_NOTHROW( hexarray::from_string( std::string( "04" ) ) );
        CHECK_NOTHROW( hexarray::from_string( std::string( "05" ) ) );
        CHECK_NOTHROW( hexarray::from_string( std::string( "06" ) ) );
        CHECK_NOTHROW( hexarray::from_string( std::string( "07" ) ) );
        CHECK_NOTHROW( hexarray::from_string( std::string( "08" ) ) );
        CHECK_NOTHROW( hexarray::from_string( std::string( "09" ) ) );
        CHECK_NOTHROW( hexarray::from_string( std::string( "0a" ) ) );
        CHECK_NOTHROW( hexarray::from_string( std::string( "0b" ) ) );
        CHECK_NOTHROW( hexarray::from_string( std::string( "0c" ) ) );
        CHECK_NOTHROW( hexarray::from_string( std::string( "0d" ) ) );
        CHECK_NOTHROW( hexarray::from_string( std::string( "0e" ) ) );
        CHECK_NOTHROW( hexarray::from_string( std::string( "0f" ) ) );
        CHECK_THROWS( hexarray::from_string( std::string( "0g" ) ) );  // all the rest should throw
    }
    SECTION( "to json" )
    {
        rsutils::json j;
        j["blah"] = hexarray( std::move( ba ) );
        CHECK( j.dump() == "{\"blah\":\"00010203040506070809\"}");
    }
    SECTION( "(same as using hexarray::to_string)" )
    {
        rsutils::json j;
        j["blah"] = hexarray::to_string( ba );
        CHECK( j.dump() == "{\"blah\":\"00010203040506070809\"}" );
    }
    SECTION( "from json" )
    {
        auto j = rsutils::json::parse( "{\"blah\":\"00010203040506070809\"}" );
        CHECK( j["blah"].get< hexarray >().get_bytes() == ba );
    }
    SECTION( "from json shuld accept bytearrays, too" )
    {
        auto j = rsutils::json::parse( "{\"blah\":[0,1,2,3,4,5,6,7,8,9]}" );
        CHECK( j["blah"].get< hexarray >().get_bytes() == ba );

        j = rsutils::json::parse( "{\"blah\":[0,1,256]}" );  // out-of-range
        CHECK_THROWS( j["blah"].get< hexarray >().get_bytes() );

        j = rsutils::json::parse( "{\"blah\":[0,1,2.0]}" );  // must be integer
        CHECK_THROWS( j["blah"].get< hexarray >().get_bytes() );
    }
}
