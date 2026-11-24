// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "custom-nn-frame.h"

#include <src/librealsense-exception.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/string/from.h>

namespace librealsense {

custom_nn_frame::custom_nn_frame( custom_nn_frame && other )
    : video_frame( std::move( other ) )
{
}

custom_nn_frame & custom_nn_frame::operator=( custom_nn_frame && other )
{
    video_frame::operator=( std::move( other ) );
    std::lock_guard< std::mutex > lock( _parse_mutex );
    _parse_done = false;
    _parsed.reset();
    return *this;
}

custom_nn_frame::parsed const * custom_nn_frame::layout() const
{
    std::lock_guard< std::mutex > lock( _parse_mutex );
    if( ! _parse_done )
    {
        _parse_done = true;
        try
        {
            // Not `data`: it is empty when the bytes live in an external (zero-copy) buffer
            auto p = std::unique_ptr< parsed >( new parsed{ nn::parse_custom_nn_frame( get_frame_data(), get_frame_data_size() ), {} } );
            for( auto const & t : p->layout.tensors )
                p->shapes.push_back( { int( t.size / nn::dtype_size( t.dtype ) ) } );
            _parsed = std::move( p );
        }
        catch( std::exception const & e )
        {
            LOG_WARNING( e.what() );
        }
    }
    return _parsed.get();
}

size_t custom_nn_frame::get_tensor_count() const
{
    auto p = layout();
    return p ? p->layout.tensors.size() : 0;
}

rs2_nn_tensor custom_nn_frame::get_tensor( size_t index ) const
{
    size_t const count = get_tensor_count();
    if( index >= count )
        throw invalid_value_exception( rsutils::string::from()
                                       << "Tensor index " << index << " is out of range (count=" << count << ")" );
    auto p = layout();
    auto const & t = p->layout.tensors[index];
    auto const & shape = p->shapes[index];
    return { "", t.dtype, t.output_index, int( shape.size() ), shape.data(), "", get_frame_data() + t.offset, t.size, 0.f };
}

uint8_t custom_nn_frame::get_status() const
{
    auto p = layout();
    if( ! p )
        throw invalid_value_exception( "Invalid Custom NN frame" );
    return p->layout.payload.status;
}

const char * custom_nn_frame::get_status_message() const
{
    auto p = layout();
    if( ! p )
        throw invalid_value_exception( "Invalid Custom NN frame" );
    return nn::custom_nn_status_message( p->layout.payload.status, p->layout.header.generation );
}

bool custom_nn_frame::find_metadata( rs2_frame_metadata_value id, rs2_metadata_type * p_value ) const
{
    auto p = layout();
    if( ! p )
        return video_frame::find_metadata( id, p_value );
    auto const & payload = p->layout.payload;
    rs2_metadata_type value;
    switch( id )
    {
    case RS2_FRAME_METADATA_FRAME_COUNTER: value = rs2_metadata_type( payload.frame_id ); break;
    case RS2_FRAME_METADATA_SOURCE_FRAME_ID: value = payload.source_frame_id; break;
    case RS2_FRAME_METADATA_NN_MODEL_GENERATION: value = p->layout.header.generation; break;
    case RS2_FRAME_METADATA_NN_STATUS: value = payload.status; break;
    case RS2_FRAME_METADATA_NN_MODEL_ID: value = p->layout.header.model_id; break;
    // The hardware timestamp of the Color frame the network ran on, in microseconds as Color's
    case RS2_FRAME_METADATA_FRAME_TIMESTAMP: value = rs2_metadata_type( payload.timestamp_ms * 1000 + 0.5 ); break;
    default: return video_frame::find_metadata( id, p_value );
    }
    if( p_value )
        *p_value = value;
    return true;
}

}  // namespace librealsense
