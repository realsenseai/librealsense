// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "color-formats-converter.h"

#include <memory>
#include <vector>
#include <future>
#include <stdint.h>

namespace librealsense
{
    // Platform H.264 decoder: takes one Annex-B access unit at a time and produces tightly packed NV12 at the requested
    // resolution. Stateful and not thread-safe, so one instance serves one stream and is driven from one thread.
    class h264_decoder_backend
    {
    public:
        virtual ~h264_decoder_backend() = default;

        // Decodes one access unit into 'nv12' (width*height*3/2 bytes). False means no picture yet - the decoder is priming,
        // or waiting for parameter sets after a loss - and the caller should drop the frame rather than show stale content.
        virtual bool decode( uint8_t const * data, size_t size, std::vector< uint8_t > & nv12 ) = 0;

        // nullptr when the platform has no H.264 decoder at all, or when it refused this particular geometry
        static std::unique_ptr< h264_decoder_backend > create( int width, int height, int fps );
    };

    // Decodes an H.264 stream to a color format. Unlike the stateless color converters a frame here depends on the frames
    // before it, so one block serves one stream, and returning nothing for an input frame is normal rather than an error.
    class h264_decoder : public color_converter
    {
    public:
        h264_decoder( rs2_format target_format )
            : color_converter( "H264 Decoder", target_format ) {}

        // False when the platform has no decoder; H264 is then offered as passthrough only, for the application to decode
        static bool is_supported();

    protected:
        rs2::frame prepare_output( const rs2::frame_source & source,
                                   rs2::frame input,
                                   std::vector< rs2::frame > results ) override;
        rs2::frame process_frame( const rs2::frame_source & source, const rs2::frame & f ) override;
        void process_function( uint8_t * const dest[], const uint8_t * source, int width, int height,
                               int actual_size, int input_size ) override;

    private:
        // Bounds the start-up backlog below. Deliberately generous - ~2s at 30fps against a start-up costing ~0.2s - so it
        // only trips if something is badly wrong, and even then the oldest units, carrying the parameter sets, are kept.
        static constexpr size_t max_queued_while_starting = 64;

        std::unique_ptr< h264_decoder_backend > _backend;
        std::future< std::unique_ptr< h264_decoder_backend > > _starting;  // backend creation, kept off the frame callback
        std::vector< std::future< std::unique_ptr< h264_decoder_backend > > > _abandoned;  // superseded starts, retired on destruction
        std::vector< std::vector< uint8_t > > _queued;                     // access units that arrived while starting
        std::vector< uint8_t > _nv12;  // what the backend decodes into, converted to the target format before it goes out
        int _width = 0;
        int _height = 0;
        bool _reported_overflow = false;
    };
}
