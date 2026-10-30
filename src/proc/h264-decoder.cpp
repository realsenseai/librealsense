// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "h264-decoder.h"

#include <rsutils/easylogging/easyloggingpp.h>

#include <chrono>

namespace librealsense
{
#if ! defined( _WIN32 ) && ! defined( RS2_USE_VPL_H264_DECODER )
    std::unique_ptr< h264_decoder_backend > h264_decoder_backend::create( int, int, int )
    {
        return nullptr;
    }
#endif

    bool h264_decoder::is_supported()
    {
        // Building one is the only way to find out whether the platform has a decoder, so probe once with an arbitrary
        // small profile and keep the answer. This is the single place that fact is established, so warn here and nowhere else.
        static bool const supported = []
        {
            if( h264_decoder_backend::create( 640, 360, 30 ) )
                return true;
            LOG_WARNING( "no H.264 decoder on this platform - H264 streams can be received, not decoded" );
            return false;
        }();
        return supported;
    }

    rs2::frame h264_decoder::process_frame( const rs2::frame_source & source, const rs2::frame & f )
    {
        auto vf = f.as< rs2::video_frame >();
        if( ! vf )
            return {};

        // Short-circuit before anything is allocated: without a decoder, every geometry would otherwise start a thread and
        // fill a queue only to throw both away, once per block - and the viewer holds one block per sensor
        if( ! is_supported() )
            return {};

        uint8_t const * const data = static_cast< uint8_t const * >( f.get_data() );
        size_t const size = f.get_data_size();

        if( vf.get_width() != _width || vf.get_height() != _height )
        {
            _width = vf.get_width();
            _height = vf.get_height();
            _backend.reset();
            _queued.clear();
            _reported_overflow = false;
            // A future from std::async blocks in its destructor, so overwriting one that is still running would stall the
            // frame callback for as long as the old start takes. Park it instead and let it retire in its own time.
            if( _starting.valid() )
                _abandoned.push_back( std::move( _starting ) );
            // Building a backend costs ~0.2s - D3D11 device, decoder, type negotiation - far too long to spend on the frame
            // callback, so it happens on its own thread. Once per geometry, never once per frame.
            _starting = std::async( std::launch::async,
                                    [width = _width, height = _height, fps = f.get_profile().fps()]
                                    { return h264_decoder_backend::create( width, height, fps ); } );
        }

        if( _starting.valid() )
        {
            if( _starting.wait_for( std::chrono::seconds( 0 ) ) != std::future_status::ready )
            {
                // Hold these back instead of dropping them: the camera sends SPS/PPS once, inside the very first access unit,
                // and never repeats them, so losing that one would leave the decoder unable to start on any later keyframe
                if( _queued.size() < max_queued_while_starting )
                    _queued.emplace_back( data, data + size );
                else if( ! _reported_overflow )
                {
                    // What gets dropped from here includes the parameter sets, which the camera sends once and never
                    // repeats, so the stream stops being decodable at all rather than merely degrading
                    _reported_overflow = true;
                    LOG_ERROR( "H264 backend did not start within " << max_queued_while_starting
                                                                    << " frames; the stream will not decode" );
                }
                return {};
            }
            _backend = _starting.get();
            if( ! _backend )
            {
                // is_supported() already passed, so it is this geometry alone that was refused - say so, or the stream
                // just produces nothing. Bounded: one attempt, and one message, per geometry.
                LOG_ERROR( "H264 decoder refused " << _width << 'x' << _height << "; this stream will not decode" );
                _queued.clear();
            }
        }
        if( ! _backend )
            return {};

        // The backlog holds the parameter sets and the reference frames built on them, so it has to go through the decoder
        // in order before anything else can decode; its pictures are discarded and only the frame in hand becomes output
        for( auto const & access_unit : _queued )
            _backend->decode( access_unit.data(), access_unit.size(), _nv12 );
        _queued.clear();

        if( ! _backend->decode( data, size, _nv12 ) )
            return {};

        auto ret = prepare_frame( source, f );  // also initializes _target_bpp
        uint8_t * planes[1] = { const_cast< uint8_t * >( static_cast< uint8_t const * >( ret.get_data() ) ) };
        process_function( planes, nullptr, _width, _height, _height * _width * _target_bpp, 0 );
        return ret;
    }

    void h264_decoder::process_function( uint8_t * const dest[], const uint8_t *, int width, int height,
                                         int actual_size, int )
    {
        // The unnamed source argument is the bitstream we were handed; the pixels come from _nv12, which decode() just filled
        unpack_nv12( _target_format, _target_stream, dest, _nv12.data(), width, height, actual_size );
    }

    rs2::frame h264_decoder::prepare_output( const rs2::frame_source & source,
                                             rs2::frame input,
                                             std::vector< rs2::frame > results )
    {
        // No results means process_frame decoded no picture, which is routine while starting up or waiting for a keyframe.
        // The base class would then forward the input unchanged - here the H.264 bitstream, to a caller expecting RGB8.
        if( results.empty() )
            return {};
        return color_converter::prepare_output( source, input, results );
    }
}
