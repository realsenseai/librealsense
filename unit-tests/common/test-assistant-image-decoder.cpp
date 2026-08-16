// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:add-file ../../common/assistant/assistant-image-decoder.cpp

#include <unit-tests/catch.h>
#include <common/assistant/assistant-image-decoder.h>

using rs2::assistant_detail::decode_image_bytes;


namespace {

// A real, valid 1x1 opaque-red PNG (RGBA), generated via Python's zlib so the compressed IDAT
// and every chunk CRC are correct - not hand-typed, which would be too easy to get subtly wrong.
const std::vector<uint8_t> one_pixel_png = {
    137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,
    0,0,0,13,73,68,65,84,120,156,99,248,207,192,240,31,0,5,0,1,255,137,153,61,29,
    0,0,0,0,73,69,78,68,174,66,96,130
};

}  // namespace


TEST_CASE("assistant-image-decoder/static-png", "[assistant]")
{
    auto img = decode_image_bytes(one_pixel_png.data(), one_pixel_png.size());

    REQUIRE(img.frames.size() == 1);
    CHECK(img.width == 1);
    CHECK(img.height == 1);
    CHECK(img.frames[0].rgba.size() == 4); // 1x1 RGBA
    CHECK(img.frames[0].rgba[0] == 255); // R
    CHECK(img.frames[0].rgba[1] == 0);   // G
    CHECK(img.frames[0].rgba[2] == 0);   // B
    CHECK(img.frames[0].rgba[3] == 255); // A
    CHECK(img.frames[0].delay_ms == 0);  // static image, not animated
}

TEST_CASE("assistant-image-decoder/garbage-bytes-fail-gracefully", "[assistant]")
{
    std::string junk = "this is not an image file at all";
    auto img = decode_image_bytes((const uint8_t*)junk.data(), junk.size());
    CHECK(img.frames.empty());
}

TEST_CASE("assistant-image-decoder/truncated-gif-signature-does-not-crash", "[assistant]")
{
    // Starts with the GIF magic (routes into the animated-GIF decode path) but has no valid
    // header/data after it - must fail gracefully, not crash, on a malformed response body.
    // A byte vector (not a string literal) so embedded zero bytes aren't truncated.
    std::vector<uint8_t> junk = { 'G','I','F','8','9','a', 0,0,0,0, 'x','x','x','x' };
    auto img = decode_image_bytes(junk.data(), junk.size());
    CHECK(img.frames.empty());
}

TEST_CASE("assistant-image-decoder/empty-input", "[assistant]")
{
    auto img = decode_image_bytes(nullptr, 0);
    CHECK(img.frames.empty());

    uint8_t dummy = 0;
    img = decode_image_bytes(&dummy, 0);
    CHECK(img.frames.empty());
}
