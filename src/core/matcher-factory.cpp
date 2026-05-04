// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "matcher-factory.h"
#include "frame-holder.h"
#include "stream-interface.h"

#include <src/sync.h>
#include <stdexcept>


namespace librealsense {


std::shared_ptr< matcher > matcher_factory::create( rs2_matchers matcher,
                                                    std::vector< stream_interface * > const & profiles )
{
    switch( matcher )
    {
    case RS2_MATCHER_DI:
        return create_DI_matcher( profiles );
    case RS2_MATCHER_DI_C:
        return create_DI_C_matcher( profiles );
    case RS2_MATCHER_DLR_C:
        return create_DLR_C_matcher( profiles );
    case RS2_MATCHER_DLR:
        return create_DLR_matcher( profiles );
    case RS2_MATCHER_DIC:
        return create_DIC_matcher( profiles );
    case RS2_MATCHER_DIC_C:
        return create_DIC_C_matcher( profiles );

    case RS2_MATCHER_DEFAULT:
    default:
        return create_timestamp_matcher( profiles );
    }
}


std::shared_ptr< matcher > matcher_factory::create_DLR_C_matcher( std::vector< stream_interface * > const & profiles )
{
    auto color = get_color_profiles( profiles );
    if( color.empty() )
    {
        LOG_DEBUG( "Created default matcher" );
        return create_timestamp_matcher( profiles );
    }

    auto infer = get_inference_profiles( profiles );
    if( ! infer.empty() )
    {
        return create_timestamp_composite_matcher( { create_DLR_matcher( profiles ),
                                                     create_color_composite_matcher( color ),
                                                     create_timestamp_matcher( infer ) } );
    }

    return create_timestamp_composite_matcher( { create_DLR_matcher( profiles ), create_color_composite_matcher( color ) } );
}


std::shared_ptr< matcher > matcher_factory::create_DI_C_matcher( std::vector< stream_interface * > const & profiles )
{
    auto color = get_color_profiles( profiles );
    if( color.empty() )
    {
        LOG_DEBUG( "Created default matcher" );
        return create_timestamp_matcher( profiles );
    }

    return create_timestamp_composite_matcher( { create_DI_matcher( profiles ), create_color_composite_matcher( color ) } );
}


std::shared_ptr< matcher > matcher_factory::create_DLR_matcher( std::vector< stream_interface * > const & profiles )
{
    auto depth = find_profile( RS2_STREAM_DEPTH, 0, profiles );
    auto left = find_profile( RS2_STREAM_INFRARED, 1, profiles );
    auto right = find_profile( RS2_STREAM_INFRARED, 2, profiles );

    if( ! depth || ! left || ! right )
    {
        LOG_DEBUG( "Created default matcher" );
        return create_timestamp_matcher( profiles );
    }
    return create_frame_number_matcher( { depth, left, right } );
}


std::shared_ptr< matcher > matcher_factory::create_DI_matcher( std::vector< stream_interface * > const & profiles )
{
    auto depth = find_profile( RS2_STREAM_DEPTH, 0, profiles );
    auto ir = find_profile( RS2_STREAM_INFRARED, 1, profiles );

    if( ! depth || ! ir )
    {
        LOG_DEBUG( "Created default matcher" );
        return create_timestamp_matcher( profiles );
    }
    return create_frame_number_matcher( { depth, ir } );
}


std::shared_ptr< matcher > matcher_factory::create_DIC_matcher( std::vector< stream_interface * > const & profiles )
{
    std::vector< std::shared_ptr< matcher > > matchers;
    if( auto depth = find_profile( RS2_STREAM_DEPTH, -1, profiles ) )
        matchers.push_back( create_identity_matcher( depth ) );
    if( auto ir = find_profile( RS2_STREAM_INFRARED, -1, profiles ) )
        matchers.push_back( create_identity_matcher( ir ) );
    if( auto confidence = find_profile( RS2_STREAM_CONFIDENCE, -1, profiles ) )
        matchers.push_back( create_identity_matcher( confidence ) );

    if( matchers.empty() )
    {
        LOG_ERROR( "no depth, ir, or confidence streams found for matcher" );
        for( auto && p : profiles )
            LOG_DEBUG( p->get_stream_type() << '/' << p->get_unique_id() );
        throw std::runtime_error( "no depth, ir, or confidence streams found for matcher" );
    }

    return create_timestamp_composite_matcher( matchers );
}


std::shared_ptr< matcher > matcher_factory::create_DIC_C_matcher( std::vector< stream_interface * > const & profiles )
{
    auto color = get_color_profiles( profiles );
    if( color.empty() )
        throw std::runtime_error( "no color stream found for matcher" );

    return create_timestamp_composite_matcher( { create_DIC_matcher( profiles ), create_color_composite_matcher( color ) } );
}


std::shared_ptr< matcher >
matcher_factory::create_frame_number_matcher( std::vector< stream_interface * > const & profiles )
{
    std::vector< std::shared_ptr< matcher > > matchers;
    for( auto & p : profiles )
        matchers.push_back( std::make_shared< identity_matcher >( p->get_unique_id(), p->get_stream_type() ) );

    return create_frame_number_composite_matcher( matchers );
}


std::shared_ptr< matcher >
matcher_factory::create_timestamp_matcher( std::vector< stream_interface * > const & profiles )
{
    std::vector< std::shared_ptr< matcher > > matchers;
    for( auto & p : profiles )
        matchers.push_back( std::make_shared< identity_matcher >( p->get_unique_id(), p->get_stream_type() ) );

    return create_timestamp_composite_matcher( matchers );
}


std::shared_ptr< matcher > matcher_factory::create_identity_matcher( stream_interface * profile )
{
    return std::make_shared< identity_matcher >( profile->get_unique_id(), profile->get_stream_type() );
}

std::shared_ptr< matcher >
matcher_factory::create_frame_number_composite_matcher( std::vector< std::shared_ptr< matcher > > const & matchers )
{
    return std::make_shared< frame_number_composite_matcher >( matchers );
}


std::shared_ptr< matcher >
matcher_factory::create_timestamp_composite_matcher( std::vector< std::shared_ptr< matcher > > const & matchers )
{
    return std::make_shared< timestamp_composite_matcher >( matchers );
}


std::vector< stream_interface * >
matcher_factory::get_color_profiles( std::vector< stream_interface * > const & profiles )
{
    // We need one profile per stream - matcher use only UID and type
    std::map< int, stream_interface * > color_profiles;
    for( auto & profile : profiles )
        if( profile->get_stream_type() == RS2_STREAM_COLOR )
            color_profiles[profile->get_stream_index()] = profile;

    std::vector< stream_interface * > ret;
    for( auto & profile : color_profiles )
        ret.push_back( profile.second );

    return ret;
}


std::vector< stream_interface * >
matcher_factory::get_inference_profiles( std::vector< stream_interface * > const & profiles )
{
    std::vector< stream_interface * > ret;
    for( auto & profile : profiles )
        if( profile->get_stream_type() == RS2_STREAM_OBJECT_DETECTION)
            ret.push_back( profile );

    return ret;
}


std::shared_ptr< matcher >
matcher_factory::create_color_composite_matcher( std::vector< stream_interface * > const & profiles )
{
    std::vector< std::shared_ptr< matcher > > color_matchers;
    for( auto & profile : profiles )
        color_matchers.push_back( std::make_shared< identity_matcher >( profile->get_unique_id(),
                                                                        profile->get_stream_type() ) );

    return create_frame_number_composite_matcher( color_matchers );
}


}  // namespace librealsense
