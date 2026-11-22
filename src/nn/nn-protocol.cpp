// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "nn-protocol.h"

#include <src/librealsense-exception.h>
#include <src/ds/d500/d500-types/common.h>

#include <rsutils/number/crc32.h>
#include <rsutils/string/from.h>

#include <cstring>

namespace librealsense {
namespace nn {

static_assert( sizeof( table_header ) == TABLE_HEADER_SIZE, "unexpected table header size" );


std::vector< uint8_t > build_table( uint16_t table_id, const uint8_t * file, size_t size )
{
    table_header header( TABLE_VERSION, table_id, static_cast< uint32_t >( size ), 0, rsutils::number::calc_crc32( file, size ) );
    std::vector< uint8_t > table( sizeof( header ) + size );
    std::memcpy( table.data(), &header, sizeof( header ) );
    std::memcpy( table.data() + sizeof( header ), file, size );
    return table;
}


status parse_status( const std::vector< uint8_t > & reply )
{
    status result;
    if( reply.size() < TABLE_HEADER_SIZE + sizeof( result ) )
    {
        throw invalid_value_exception( rsutils::string::from() << "NN status table too short: " << reply.size() << " bytes" );
    }
        
    auto header = reinterpret_cast< const table_header * >( reply.data() );
    auto const data = reply.data() + TABLE_HEADER_SIZE;
    auto table_size = header->get_table_size();
    
    if( header->get_table_type() != STATUS_TABLE_ID 
        || table_size < sizeof( result )
        || table_size > reply.size() - TABLE_HEADER_SIZE
        || header->get_crc32() != rsutils::number::calc_crc32( data, table_size ) )
    {
        throw invalid_value_exception( "invalid NN status table" );
    }
    
    std::memcpy( &result, data, sizeof( result ) );
    return result;
}


std::string error_message( int32_t code )
{
    switch( code )
    {
    case -1: return "invalid command";
    case -2: return "invalid upload parameter";
    case -3: return "a model in the device file system overrides the flash slot; remove it before uploading";
    case -4:
    case -34:
        return "NN model upload is not available on this camera because it is locked. In this release, models can only be "
               "uploaded to unlocked cameras";
    case -5:
        return "the model file is not valid for this camera. Check that it was converted for this camera's firmware "
               "version";
    case -6: return "package CRC mismatch";
    case -10: return "the model is too large for this camera's model slot; the limit depends on the camera firmware";
    case -12: return "NN upload is not supported by this firmware";
    case -15:
        return "writing the model to the camera failed. The camera has no detection model until an upload succeeds; "
               "upload again";
    case -19:
        return "this model package is not supported by this camera's firmware (its format, codec, signature or model "
               "type)";
    case -20: return "incomplete transfer";
    case -21: return "the camera could not allocate resources for the upload; reset the camera and try again";
    case -41: return "the camera is busy: stop the Perception sensor and any other upload, then try again";
    default: return rsutils::string::from() << "device error " << code;
    }
}


void throw_error( int32_t code )
{
    std::string msg = rsutils::string::from() << "NN upload failed: " << error_message( code ) << " (" << code << ")";
    if( code == -1 || code == -4 || code == -12 || code == -19 || code == -34 )
        throw not_implemented_exception( msg );
    throw invalid_value_exception( msg );
}


size_t dtype_size( rs2_nn_dtype dtype )
{
    switch( dtype )
    {
    case RS2_NN_DTYPE_S8:
    case RS2_NN_DTYPE_U8: return 1;
    case RS2_NN_DTYPE_S16:
    case RS2_NN_DTYPE_U16: return 2;
    case RS2_NN_DTYPE_F32: return 4;
    default: return 0;
    }
}


static size_t round_up( size_t size ) { return ( size + CUSTOM_NN_ALIGNMENT - 1 ) / CUSTOM_NN_ALIGNMENT * CUSTOM_NN_ALIGNMENT; }


custom_nn_frame_layout parse_custom_nn_frame( const uint8_t * data, size_t size )
{
    auto fail = []( std::string const & why ) { throw invalid_value_exception( "Invalid Custom NN frame: " + why ); };
    if( size < CUSTOM_NN_ALIGNMENT )
        fail( rsutils::string::from() << size << " bytes" );

    custom_nn_frame_layout layout;
    std::memcpy( &layout.header, data, sizeof( layout.header ) );
    std::memcpy( &layout.payload, data + sizeof( layout.header ), sizeof( layout.payload ) );
    auto const & header = layout.header;
    if( header.magic != CUSTOM_NN_MAGIC )
        fail( rsutils::string::from() << "magic 0x" << std::hex << header.magic );
    if( header.version != CUSTOM_NN_VERSION )
        fail( rsutils::string::from() << "unsupported version 0x" << std::hex << header.version );

    size_t const end = sizeof( header ) + size_t( header.payload_size );
    if( end < CUSTOM_NN_ALIGNMENT || end > size )
        fail( rsutils::string::from() << "payload size " << header.payload_size << " in a frame of " << size << " bytes" );
    auto const crc = rsutils::number::calc_crc32( data + sizeof( header ), header.payload_size );
    if( crc != header.crc32 )
        fail( rsutils::string::from() << "CRC 0x" << std::hex << crc << ", expected 0x" << header.crc32 );

    if( ( header.flags & CUSTOM_NN_ERROR_FLAG ) || layout.payload.status != uint8_t( custom_nn_status::ok ) )
        return layout;

    size_t offset = CUSTOM_NN_ALIGNMENT;
    for( int i = 0; i < layout.payload.tensor_count; ++i )
    {
        if( offset + CUSTOM_NN_ALIGNMENT > end )
            fail( rsutils::string::from() << "tensor " << i << " header past the payload" );
        custom_nn_tensor_header th;
        std::memcpy( &th, data + offset, sizeof( th ) );
        custom_nn_tensor t{ th.output_index, rs2_nn_dtype( th.dtype ), offset + CUSTOM_NN_ALIGNMENT, th.byte_size };
        size_t const element = th.dtype < RS2_NN_DTYPE_COUNT ? dtype_size( t.dtype ) : 0;
        if( ! element || t.size % element )
            fail( rsutils::string::from() << "tensor " << i << " dtype " << int( th.dtype ) << " with " << t.size << " bytes" );
        if( t.offset + t.size > end )
            fail( rsutils::string::from() << "tensor " << i << " of " << t.size << " bytes past the payload" );
        layout.tensors.push_back( t );
        offset = t.offset + round_up( t.size );
    }
    return layout;
}


const char * custom_nn_status_message( uint8_t status, uint32_t generation )
{
    switch( custom_nn_status( status ) )
    {
    case custom_nn_status::ok: return "";
    case custom_nn_status::load_failed:
        return generation ? "The Custom NN model didn't load. Upload it again."
                          : "A new Custom NN model is installed; reset the camera to use it.";
    case custom_nn_status::dsp_error: return "Inference failed on this frame.";
    default: return "The camera couldn't run the Custom NN model on this frame.";
    }
}

}  // namespace nn
}  // namespace librealsense
