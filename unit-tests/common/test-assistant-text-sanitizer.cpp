// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:add-file ../../common/assistant/assistant-text-sanitizer.cpp

#include <unit-tests/catch.h>
#include <common/assistant/assistant-text-sanitizer.h>

using rs2::assistant_detail::sanitize_for_display;
using rs2::assistant_detail::is_utf8_continuation_byte;


TEST_CASE( "assistant-sanitize/ascii-passthrough", "[assistant]" )
{
    CHECK( sanitize_for_display( "" ) == "" );
    CHECK( sanitize_for_display( "hello world 123" ) == "hello world 123" );
}

TEST_CASE( "assistant-sanitize/unicode-punctuation", "[assistant]" )
{
    // \xNN escapes are greedy in MSVC (swallow any following hex digit a-f/A-F/0-9), so any byte
    // sequence followed by a hex-digit-looking character must be split into adjacent literals.
    CHECK( sanitize_for_display( "\xE2\x80\x98" "a" "\xE2\x80\x99" ) == "'a'" );        // single quotes
    CHECK( sanitize_for_display( "\xE2\x80\x9C" "a" "\xE2\x80\x9D" ) == "\"a\"" );      // double quotes
    CHECK( sanitize_for_display( "a\xE2\x80\x93 b" ) == "a- b" );                       // en dash
    CHECK( sanitize_for_display( "a\xE2\x80\x94 b" ) == "a- b" );                       // em dash
    CHECK( sanitize_for_display( "a\xE2\x80\xA6" ) == "a..." );                         // ellipsis
    CHECK( sanitize_for_display( "a\xE2\x84\xA2" ) == "a" );                            // trademark, dropped
    CHECK( sanitize_for_display( "a\xC2\xAE" ) == "a" );                                // registered, dropped
    CHECK( sanitize_for_display( "30\xC2\xB0" ) == "30 deg" );                          // degree sign
    CHECK( sanitize_for_display( "5\xC2\xB1" "3" ) == "5+/-3" );                        // plus-minus
    CHECK( sanitize_for_display( "2\xC3\x97" "3" ) == "2x3" );                          // multiplication
}

TEST_CASE( "assistant-sanitize/citation-marker-stripped", "[assistant]" )
{
    std::string marker = "\xE3\x80\x90" "1:2\xE2\x80\xA0source" "\xE3\x80\x91";
    CHECK( sanitize_for_display( "before" + marker + "after" ) == "beforeafter" );
    CHECK( sanitize_for_display( marker ) == "" );
}

TEST_CASE( "assistant-sanitize/unmatched-multibyte-dropped", "[assistant]" )
{
    // A multi-byte sequence not in the replacement table (e.g. an emoji) is dropped outright
    // rather than left as a stray '?' - see sanitize_for_display's own rationale.
    std::string emoji = "\xF0\x9F\x98\x80"; // U+1F600 grinning face, 4-byte sequence
    CHECK( sanitize_for_display( "a" + emoji + "b" ) == "ab" );
}

TEST_CASE( "assistant-sanitize/utf8-continuation-byte", "[assistant]" )
{
    CHECK_FALSE( is_utf8_continuation_byte( 0x00 ) );
    CHECK_FALSE( is_utf8_continuation_byte( 0x7F ) );
    CHECK( is_utf8_continuation_byte( 0x80 ) );
    CHECK( is_utf8_continuation_byte( 0xBF ) );
    CHECK_FALSE( is_utf8_continuation_byte( 0xC0 ) );
    CHECK_FALSE( is_utf8_continuation_byte( 0xFF ) );
}
