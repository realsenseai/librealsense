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
    case -19: return "this model package format is not supported by this camera's firmware";
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

}  // namespace nn
}  // namespace librealsense
