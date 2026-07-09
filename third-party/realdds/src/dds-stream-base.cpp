// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include <realdds/dds-stream-base.h>
#include <realdds/dds-utilities.h>


namespace realdds {


dds_stream_base::dds_stream_base( std::string const & name,
                                  std::string const & sensor_name )
    : _name( name )
    , _sensor_name( sensor_name )
{
}


void dds_stream_base::enable_metadata()
{
    // Ensure no changes after initialization stage
    if( !_profiles.empty() )
        DDS_THROW( runtime_error, "enable metadata to stream '" + _name + "' before initializing profiles" );

    _metadata_enabled = true;
}


void dds_stream_base::init_profiles( dds_stream_profiles const & profiles, size_t default_profile_index )
{
    if( !_profiles.empty() )
        DDS_THROW( runtime_error, "stream '" + _name + "' profiles are already initialized" );
    if( profiles.empty() )
        DDS_THROW( runtime_error, "at least one profile is required to initialize stream '" + _name + "'" );

    _default_profile_index = default_profile_index;
    if( _default_profile_index >= profiles.size() )
        DDS_THROW( runtime_error,
                   "invalid default profile index (" + std::to_string( _default_profile_index ) + " for "
                       + std::to_string( profiles.size() ) + " stream profiles" );

    // weak_from_this() is C++17; Apple clang builds realdds as C++14 (cxx_std_14).
    // Equivalent C++14 form keeps DDS stream init working on macOS arm64.
    auto self = std::weak_ptr< dds_stream_base >( shared_from_this() );
    for( auto const & profile : profiles )
    {
        check_profile( profile );
        profile->init_stream( self );
        if( auto vsp = std::dynamic_pointer_cast< dds_video_stream_profile >( profile ) )
            _compressed = vsp->is_compressed_encoding();
    }

    _profiles = profiles;
}


void dds_stream_base::init_options( dds_options const & options )
{
    if( ! _options.empty() )
        DDS_THROW( runtime_error, "stream '" + _name + "' options are already initialized" );

    auto this_stream = shared_from_this();
    for( auto option : options )
        option->init_stream( this_stream );
    _options = options;
}

void dds_stream_base::init_embedded_filters( dds_embedded_filters const & embedded_filters )
{
    if( !_embedded_filters.empty() )
        DDS_THROW( runtime_error, "stream '" + _name + "' embedded filters are already initialized" );

    auto this_stream = shared_from_this();
    for( auto const & filter : embedded_filters )
        if( filter )
            filter->init_stream( this_stream );
    _embedded_filters = embedded_filters;
}

void dds_stream_base::check_profile( std::shared_ptr< dds_stream_profile > const & profile ) const
{
    if( ! profile )
        DDS_THROW( runtime_error, "null profile passed in" );
}


}  // namespace realdds
