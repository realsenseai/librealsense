// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/h/rs_sensor.h>

#include <cstdint>
#include <cstddef>
#include <vector>

namespace librealsense
{
    // PNG encode/decode for ROS2 CompressedImage frame topics.
    // Encoding uses libdeflate (portable, SIMD-accelerated checksums) — output is
    // byte-identical across architectures. The zstd fallback remains only for
    // formats PNG cannot represent.
    namespace ros2_image_codec
    {
        // Wire prefix expected by compressed_depth_image_transport on /compressedDepth
        // topics: enum compressionFormat { UNDEFINED = -1, INV_DEPTH } + 2 float params.
        // Mirrors ConfigHeader in image_transport_plugins compressed_depth_image_transport/compression_common.h.
        struct config_header
        {
            int32_t format = 0; // INV_DEPTH
            float depth_param[2] = { 0.f, 0.f };
        };
        static_assert(sizeof(config_header) == 12, "compressedDepth ConfigHeader must be 12 bytes");

        struct png_layout
        {
            size_t bytes_per_channel; // 1 or 2
            size_t num_channels;      // 1/3/4
            bool bgr_order;           // input channel order is B,G,R(,A)
            const char* ros_encoding; // sensor_msgs image encoding string
            bool is_depth;            // publish via compressedDepth convention
        };

        // Maps an rs2_format to its PNG representation. Returns false for formats
        // PNG cannot represent losslessly (YUYV, UYVY, MJPEG, ...).
        bool png_layout_for_format(rs2_format format, png_layout& out);

        // Encodes raw pixels into a standard PNG appended to `out` (existing content,
        // e.g. a compressedDepth ConfigHeader, is preserved). 16-bit input is little-endian
        // (as on every supported host) and is byte-swapped to the big-endian samples PNG
        // requires. Throws on encoder failure.
        void encode_png(const png_layout& layout,
                        const uint8_t* pixels,
                        size_t width,
                        size_t height,
                        size_t stride,
                        std::vector<uint8_t>& out);

        // Decodes a standard PNG back to raw host-endian pixels; sets the channel
        // count found in the stream. Throws on malformed input.
        std::vector<uint8_t> decode_png(const uint8_t* png_data,
                                        size_t png_size,
                                        size_t& num_channels);
    }
}
