// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "ros2_image_codec.h"

#include <librealsense2/rs.h>
#include <src/librealsense-exception.h>
#include <rsutils/string/from.h>

#include <libdeflate.h>

#include <memory>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../../../third-party/stb_image.h"

namespace librealsense
{
    namespace ros2_image_codec
    {
        bool png_layout_for_format(rs2_format format, png_layout& out)
        {
            switch (format)
            {
            case RS2_FORMAT_Z16:  out = { 2, 1, false, "16UC1", true };  return true;
            case RS2_FORMAT_Y16:  out = { 2, 1, false, "mono16", false }; return true;
            case RS2_FORMAT_Y8:   out = { 1, 1, false, "mono8", false };  return true;
            case RS2_FORMAT_RGB8: out = { 1, 3, false, "rgb8", false };   return true;
            case RS2_FORMAT_BGR8: out = { 1, 3, true,  "bgr8", false };   return true;
            case RS2_FORMAT_RGBA8: out = { 1, 4, false, "rgba8", false }; return true;
            case RS2_FORMAT_BGRA8: out = { 1, 4, true,  "bgra8", false }; return true;
            default: return false;
            }
        }

        static void put_be32(std::vector<uint8_t>& v, uint32_t x)
        {
            v.push_back(uint8_t(x >> 24)); v.push_back(uint8_t(x >> 16));
            v.push_back(uint8_t(x >> 8)); v.push_back(uint8_t(x));
        }

        static void put_chunk(std::vector<uint8_t>& png, const char type[4], const uint8_t* data, size_t len)
        {
            put_be32(png, uint32_t(len));
            size_t start = png.size();
            png.insert(png.end(), type, type + 4);
            if (len)
                png.insert(png.end(), data, data + len);
            put_be32(png, uint32_t(libdeflate_crc32(0, png.data() + start, 4 + len)));
        }

        // Sub filter for 8-bit rows; none for 16-bit — sensor noise makes horizontal
        // deltas larger than the raw values, so Sub costs both speed and size there
        static void build_scanlines(const png_layout& layout, const uint8_t* pixels,
                                    size_t width, size_t height, size_t stride,
                                    std::vector<uint8_t>& scanlines)
        {
            const size_t bpp = layout.bytes_per_channel * layout.num_channels;
            const size_t row = width * bpp;
            scanlines.resize((row + 1) * height);

            for (size_t y = 0; y < height; ++y)
            {
                const uint8_t* src = pixels + y * stride;
                uint8_t* dst = scanlines.data() + y * (row + 1);
                if (layout.bytes_per_channel == 2)
                {
                    dst[0] = 0; // no filter — byte-swap to big-endian only
                    for (size_t i = 0; i < row; i += 2)
                    {
                        dst[1 + i] = src[i + 1];
                        dst[1 + i + 1] = src[i];
                    }
                }
                else if (!layout.bgr_order)
                {
                    dst[0] = 1; // Sub filter
                    for (size_t i = 0; i < row; ++i)
                        dst[1 + i] = uint8_t(src[i] - (i >= bpp ? src[i - bpp] : 0));
                }
                else
                {
                    // BGR→RGB is a fixed per-pixel permutation (BGR formats are always
                    // 1 byte/channel) — fold it into the Sub-filter pass
                    dst[0] = 1; // Sub filter
                    for (size_t i = 0; i < row; i += bpp)
                        for (size_t c = 0; c < bpp; ++c)
                        {
                            size_t j = i + (c < 3 ? 2 - c : c);
                            dst[1 + i + c] = uint8_t(src[j] - (i ? src[j - bpp] : 0));
                        }
                }
            }
        }

        void encode_png(const png_layout& layout, const uint8_t* pixels,
                        size_t width, size_t height, size_t stride, std::vector<uint8_t>& out)
        {
            if (!pixels)
                throw invalid_value_exception("null pixel buffer");
            if (width > 0x7FFFFFFFu || height > 0x7FFFFFFFu)
                throw invalid_value_exception("image dimensions exceed the PNG limit");

            // allocation retried on failure rather than caching a permanently-null compressor
            static thread_local std::unique_ptr<libdeflate_compressor, void(*)(libdeflate_compressor*)>
                compressor(nullptr, libdeflate_free_compressor);
            if (!compressor)
                compressor.reset(libdeflate_alloc_compressor(1));
            if (!compressor)
                throw io_exception("Failed to allocate libdeflate compressor");

            // scratch buffers keep their capacity across frames, like the compressor above
            static thread_local std::vector<uint8_t> scanlines;
            static thread_local std::vector<uint8_t> compressed;
            build_scanlines(layout, pixels, width, height, stride, scanlines);

            compressed.resize(libdeflate_zlib_compress_bound(compressor.get(), scanlines.size()));
            size_t csize = libdeflate_zlib_compress(compressor.get(), scanlines.data(), scanlines.size(),
                                                    compressed.data(), compressed.size());
            if (!csize)
                throw io_exception("PNG encoding failed: zlib compression error");

            auto& png = out;
            png.reserve(png.size() + csize + 128);
            static const uint8_t magic[8] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
            png.insert(png.end(), magic, magic + 8);

            uint8_t ihdr[13] = {};
            ihdr[0] = uint8_t(width >> 24); ihdr[1] = uint8_t(width >> 16);
            ihdr[2] = uint8_t(width >> 8); ihdr[3] = uint8_t(width);
            ihdr[4] = uint8_t(height >> 24); ihdr[5] = uint8_t(height >> 16);
            ihdr[6] = uint8_t(height >> 8); ihdr[7] = uint8_t(height);
            ihdr[8] = uint8_t(layout.bytes_per_channel * 8);
            ihdr[9] = layout.num_channels == 3 ? 2 : layout.num_channels == 4 ? 6 : 0; // color type
            put_chunk(png, "IHDR", ihdr, 13);
            put_chunk(png, "IDAT", compressed.data(), csize);
            put_chunk(png, "IEND", nullptr, 0);
        }

        std::vector<uint8_t> decode_png(const uint8_t* png_data, size_t png_size,
                                        size_t& num_channels)
        {
            int w = 0, h = 0, ch = 0;
            bool is16 = stbi_is_16_bit_from_memory(png_data, static_cast<int>(png_size));
            void* pixels = is16
                ? (void*)stbi_load_16_from_memory(png_data, static_cast<int>(png_size), &w, &h, &ch, 0)
                : (void*)stbi_load_from_memory(png_data, static_cast<int>(png_size), &w, &h, &ch, 0);
            if (!pixels)
                throw io_exception(rsutils::string::from() << "PNG decoding failed: " << stbi_failure_reason());
            auto bytes = static_cast<uint8_t*>(pixels);
            std::vector<uint8_t> out(bytes, bytes + size_t(w) * h * ch * (is16 ? 2 : 1));
            stbi_image_free(pixels);
            num_channels = ch;
            return out;
        }
    }
}
