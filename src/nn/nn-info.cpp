// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "nn-info.h"

#include <src/librealsense-exception.h>

#include <rsutils/json.h>
#include <rsutils/string/from.h>

namespace librealsense {
namespace nn {


static rs2_nn_dtype parse_dtype( const std::string & dtype )
{
    static const char * names[] = { "s16", "u16", "s8", "u8", "f32" };
    for( int i = 0; i < RS2_NN_DTYPE_COUNT; ++i )
        if( dtype == names[i] )
            return rs2_nn_dtype( i );
    throw invalid_value_exception( "unknown Custom NN output dtype '" + dtype + "'" );
}


// Strides in elements, the last one varying fastest; none given means dense. A size-1 dimension's stride is never used.
static bool is_dense( const std::vector< int > & sizes, const std::vector< int > & strides )
{
    if( strides.empty() )
        return true;
    if( strides.size() != sizes.size() )
        return false;
    int expected = 1;
    for( size_t i = sizes.size(); i-- > 0; )
    {
        if( sizes[i] != 1 && strides[i] != expected )
            return false;
        expected *= sizes[i];
    }
    return true;
}


static uint32_t parse_model_id( const rsutils::json & metadata )
{
    auto const id = metadata.find( "model_id" );
    if( id == metadata.end() || ! id->is_number_unsigned() || id->get< uint64_t >() > UINT32_MAX )
        return 0;
    return id->get< uint32_t >();
}


std::shared_ptr< const model_description > parse_model_description( const std::string & metadata, uint32_t generation )
{
    try
    {
        auto j = rsutils::json::parse( metadata );
        if( j.at( "format" ).get< std::string >() != "custom_nn_v1" )
            throw invalid_value_exception( "unsupported format " + j.at( "format" ).dump() );

        auto desc = std::make_shared< model_description >();
        desc->generation = generation;
        desc->model = j.value( "model", "" );
        desc->model_id = parse_model_id( j );
        auto const & outputs = j.at( "outputs" );
        desc->outputs.resize( outputs.size() );
        std::vector< bool > seen( outputs.size() );
        for( auto const & o : outputs )
        {
            size_t const index = o.at( "index" ).get< size_t >();
            if( index >= outputs.size() || seen[index] )
                throw invalid_value_exception( rsutils::string::from() << "output index " << index );
            seen[index] = true;
            auto & out = desc->outputs[index];
            out = { o.at( "name" ).get< std::string >(),
                    parse_dtype( o.at( "dtype" ).get< std::string >() ),
                    o.at( "sizes" ).get< std::vector< int > >(),
                    o.value( "format", "" ),
                    o.value( "strides", std::vector< int >() ),
                    o.at( "buffer_size" ).get< uint32_t >(),
                    o.at( "dequant_scale" ).get< float >() };
            out.dense = is_dense( out.sizes, out.strides );
        }
        return desc;
    }
    catch( std::exception const & e )
    {
        throw invalid_value_exception( std::string( "Invalid Custom NN model metadata: " ) + e.what() );
    }
}


std::string to_json( const stored_model & stored )
{
    auto const & b = stored.block;
    rsutils::json j = { { "present", b.present != 0 },
                        { "model_state", b.model_state },
                        { "generation", b.generation },
                        { "frame_size", b.frame_size },
                        { "frame_height", b.frame_height } };
    if( b.present )
    {
        auto metadata = rsutils::json::parse( stored.metadata, nullptr, false );
        j["metadata"] = metadata.is_discarded() ? rsutils::json( stored.metadata ) : metadata;  // as stored if not JSON
        j["model_id"] = metadata.is_object() ? parse_model_id( metadata ) : 0;  // what the frames will carry
    }
    return j.dump();
}


}  // namespace nn
}  // namespace librealsense
