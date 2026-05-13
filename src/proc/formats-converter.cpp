// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023-4 RealSense, Inc. All Rights Reserved.

#include "proc/formats-converter.h"
#include "stream.h"
#include <src/composite-frame.h>
#include <src/core/frame-callback.h>

#include <rsutils/string/from.h>
#include <ostream>

namespace librealsense
{

void formats_converter::register_converter( const std::vector< stream_profile > & source,
                                            const std::vector< stream_profile > & target,
                                            std::function< std::shared_ptr< processing_block >( void ) > generate_func )
{
    _pb_factories.push_back( std::make_shared< processing_block_factory >( source, target, generate_func ) );
}

void formats_converter::register_converter( const processing_block_factory & pbf )
{
    _pb_factories.push_back( std::make_shared<processing_block_factory>( pbf ) );
}

void formats_converter::register_converters( const std::vector< processing_block_factory > & pbfs )
{
    for( auto & pbf : pbfs )
        register_converter( pbf );
}

void formats_converter::clear_registered_converters()
{
    _pb_factories.clear();
}

void formats_converter::drop_non_basic_formats()
{
    for( size_t i = 0; i < _pb_factories.size(); ++i )
    {
        const auto & source = _pb_factories[i]->get_source_info();
        const auto & target = _pb_factories[i]->get_target_info();

        bool is_identity = true;
        for( auto & t : target )
        {
            if( source[0].format != t.format )
            {
                is_identity = false;
                break;
            }
        }
        if( is_identity )
        {
            // Keep this converter unless it is colored infrared
            bool colored_infrared = target[0].stream == RS2_STREAM_INFRARED && source[0].format == RS2_FORMAT_UYVY;
            if( ! colored_infrared )
                continue;
        }

        if( source[0].format == RS2_FORMAT_Y8I || source[0].format == RS2_FORMAT_Y12I )
            continue; // Convert interleaved formats.

        // Remove unwanted converters. Move to last element in vector and pop it out.
        if( i != ( _pb_factories.size() -1 ) )
            std::swap( _pb_factories[i], _pb_factories.back() );
        _pb_factories.pop_back();
        --i; // Don't advance the counter because we reduce the vector size.
    }
}

std::ostream & operator<<( std::ostream & os, const std::shared_ptr< stream_profile_interface > & profile )
{
    if( profile )
    {
        os << rs2_stream_to_string( profile->get_stream_type() );
        if( auto stream_index = profile->get_stream_index() )
            os << " " << stream_index;
        if( auto vsp = As< video_stream_profile, stream_profile_interface >( profile ) )
        {
            os << " " << vsp->get_width();
            os << "x" << vsp->get_height();
        }
        os << " " << rs2_format_to_string( profile->get_format() );
        os << " @ " << profile->get_framerate() << " Hz";
        if( auto bsp = std::dynamic_pointer_cast< backend_stream_profile >( profile ) )
            bsp->to_stream( os );
    }

    return os;
}

stream_profiles formats_converter::get_all_possible_profiles( const stream_profiles & raw_profiles )
{
    // For each profile that can be used as input check all registered factories if they can create
    // a converter from the profile format (source). If so, create appropriate profiles for all possible target formats
    // that the converters support.
    // Note - User profile type is stream_profile_interface, factories profile type is stream_profile.
    stream_profiles to_profiles;

    for( auto & raw_profile : raw_profiles )
    {
        //LOG_DEBUG( "Raw profile: " << raw_profile );
        for( auto & pbf : _pb_factories )
        {
            const auto & sources = pbf->get_source_info();
            for( auto & source : sources )
            {
                if( source.format == raw_profile->get_format() &&
                   ( source.stream == raw_profile->get_stream_type() || source.stream == RS2_STREAM_ANY ) )
                {
                    // targets are saved with format, type and sometimes index. Updating fps and resolution before using as key
                    for( const auto & target : pbf->get_target_info() )
                    {
                        // When interleaved streams are seperated to two distinct streams (e.g. sent as DDS streams),
                        // same converters are registered for both streams. We handle the relevant one based on index.
                        // Currently for infrared streams only.
                        if( source.stream == RS2_STREAM_INFRARED && raw_profile->get_stream_index() != target.index )
                            continue;

                        auto cloned_profile = clone_profile( raw_profile );
                        cloned_profile->set_format( target.format );
                        cloned_profile->set_stream_index( target.index );
                        cloned_profile->set_stream_type( target.stream );
                        // UVC raw profile name is not set, default name can be created based on type and index, but raw UVC index is always 0.
                        // Use temporary variable to generate name without changing original raw_profile object.
                        auto && tmp_raw_profile = std::dynamic_pointer_cast< stream_profile_base >( raw_profile ).get();
                        tmp_raw_profile->set_stream_index( target.index );
                        cloned_profile->set_name( tmp_raw_profile->get_name() );

                        auto cloned_vsp = As< video_stream_profile, stream_profile_interface >( cloned_profile );
                        if( cloned_vsp )
                        {
                            // Conversion may involve changing the resolution (rotation, expansion, etc.)
                            auto width = cloned_vsp->get_width();
                            auto height = cloned_vsp->get_height();
                            target.resolution_transform( width, height );
                            cloned_vsp->set_dims( width, height );
                        }
                        //LOG_DEBUG( "          -> " << cloned_profile );

                        // Cache pbf supported profiles for efficiency in find_pbf_matching_most_profiles
                        _pbf_supported_profiles[pbf.get()].push_back( cloned_profile );

                        // Cache mapping of each target profile to profiles it is converting from.
                        // Using map key type stream_profile and calling to_profile because stream_profile_interface is
                        // abstract, can't use as key. shared_ptr< stream_profile_interface > saves pointer as key which
                        // will result in bugs when mapping multiple raw profiles to the same converted profile.
                        _target_profiles_to_raw_profiles[to_profile( cloned_profile.get() )].push_back( raw_profile );

                        // TODO - Duplicates in the list happen when 2 raw_profiles have conversion to same target.
                        // In this case it is faster to check if( _target_to_source_profiles_map[target].size() > 1 )
                        // rather then if( is_profile_in_list( cloned_profile, to_profiles ) ), but L500 unit-tests
                        // fail if we change. Need to understand difference
                        if( is_profile_in_list( cloned_profile, to_profiles ) )
                            continue;

                        // Only injective cloning in many to one mapping.
                        // One to many is not affected.
                        if( sources.size() > 1 && target.format != source.format )
                            continue;

                        to_profiles.push_back( cloned_profile );
                    }
                }
            }
        }
    }

    return to_profiles;
}

std::shared_ptr< stream_profile_interface >
formats_converter::clone_profile( const std::shared_ptr< stream_profile_interface > & raw_profile ) const
{
    std::shared_ptr< stream_profile_interface > cloned = nullptr;

    auto vsp = std::dynamic_pointer_cast< video_stream_profile >( raw_profile );
    auto msp = std::dynamic_pointer_cast< motion_stream_profile >( raw_profile );
    if( vsp )
    {
        cloned = std::make_shared< video_stream_profile >();
        if( ! cloned )
            throw librealsense::invalid_value_exception( "failed to clone profile" );

        auto video_clone = std::dynamic_pointer_cast< video_stream_profile >( cloned );
        video_clone->set_dims( vsp->get_width(), vsp->get_height() );
        // Do not set intrinsics. Device is setting after all profiles are returned. On update_target_profiles_data
        // raw profile calls the set cloned intrinsics and if device is not setting a call cycle will be created.
        // There is a default implementation throwing if no other function is set.
        // video_clone->set_intrinsics( [vsp]() { return vsp->get_intrinsics(); } );
    }
    else if( msp )
    {
        cloned = std::make_shared< motion_stream_profile >();
        if( ! cloned )
            throw librealsense::invalid_value_exception( "failed to clone profile" );

        auto motion_clone = std::dynamic_pointer_cast< motion_stream_profile >( cloned );
        // motion_clone->set_intrinsics( [msp]() { return msp->get_intrinsics(); } );
    }
    else
        throw librealsense::not_implemented_exception( "Unsupported profile type to clone" );

    cloned->set_framerate( raw_profile->get_framerate() );

    // No need to set the ID, when calling get_all_possible_target_profiles the raw_profiles don't have an assigned ID
    // yet, it will be assigned according to stream later on.
    // cloned->set_unique_id( raw_profile->get_unique_id() );
    // 
    // Needed for clone_profile wholeness but will be overwritten later by target profile fields. Can skip.
    // cloned->set_format( raw_profile->get_format() );
    // cloned->set_stream_index( raw_profile->get_stream_index() );
    // cloned->set_stream_type( raw_profile->get_stream_type() );

    return cloned;
}

bool formats_converter::is_profile_in_list( const std::shared_ptr< stream_profile_interface > & profile,
                                            const stream_profiles & profiles ) const
{
    // Converting to stream_profile to avoid dynamic casting to video/motion_stream_profile
    auto is_duplicate_predicate = [&profile]( const std::shared_ptr< stream_profile_interface > & spi ) {
        return to_profile( spi.get() ) == to_profile( profile.get() );
    };

    return std::any_of( begin( profiles ), end( profiles ), is_duplicate_predicate );
}

// Not passing const & because we modify from_profiles, would otherwise need to create a copy
void formats_converter::prepare_to_convert( stream_profiles from_profiles )
{
    LOG_DEBUG( "Requested: " << from_profiles );
    clear_active_cache();

    // Add missing data to target profiles (was not available during get_all_possible_target_profiles)
    update_target_profiles_data( from_profiles );

    // Caching from_profiles to set as processed frames profile before calling user callback
    cache_from_profiles( from_profiles );

    while( ! from_profiles.empty() )
    {
        const auto & best_match = find_pbf_matching_most_profiles( from_profiles );
        auto & factory_of_best_match = best_match.first;
        auto & from_profiles_of_best_match = best_match.second;

        // Mark matching profiles as handled
        for( auto & profile : from_profiles_of_best_match )
        {
            const auto & matching_profiles_predicate = [&profile]( const std::shared_ptr<stream_profile_interface> & sp ) {
                return to_profile( profile.get() ) == to_profile( sp.get() );
            };
            from_profiles.erase( std::remove_if( begin( from_profiles ), end( from_profiles ), matching_profiles_predicate ) );
        }

        // Retrieve source profile from cached map and generate the relevant processing block.
        std::unordered_set< std::shared_ptr< stream_profile_interface > > current_resolved_reqs;
        auto best_pb = factory_of_best_match->generate();
        for( const auto & from_profile : from_profiles_of_best_match )
        {
            auto & mapped_raw_profiles = _target_profiles_to_raw_profiles[to_profile( from_profile.get() )];

            for( const auto & raw_profile : mapped_raw_profiles )
            {
                if( factory_of_best_match->has_source( raw_profile ) )
                {
                    current_resolved_reqs.insert( raw_profile );

                    // Caching converters to invoke appropriate converters for received frames
                    _raw_profile_to_converters[raw_profile].insert( best_pb );
                }
            }
        }
        const stream_profiles & print_current_resolved_reqs = { current_resolved_reqs.begin(), current_resolved_reqs.end() };
        LOG_DEBUG( "Resolved to: " << print_current_resolved_reqs );
    }
}


stream_profiles const & formats_converter::get_source_profiles_from_target(
    std::shared_ptr< stream_profile_interface > const & target_profile ) const
{
    auto it = _target_profiles_to_raw_profiles.find( to_profile( target_profile.get() ) );
    if( it == _target_profiles_to_raw_profiles.end() )
        throw invalid_value_exception( rsutils::string::from()
                                       << "target profile [" << target_profile << "] not found" );
    return it->second;
}


void formats_converter::update_target_profiles_data( const stream_profiles & from_profiles )
{
    for( auto & from_profile : from_profiles )
    {
        for( auto & raw_profile : _target_profiles_to_raw_profiles[to_profile( from_profile.get() )] )
        {
            raw_profile->set_stream_index( from_profile->get_stream_index() );
            raw_profile->set_unique_id( from_profile->get_unique_id() );
            raw_profile->set_stream_type( from_profile->get_stream_type() );
            auto video_raw_profile = As< video_stream_profile, stream_profile_interface >( raw_profile );
            const auto video_from_profile = As< video_stream_profile, stream_profile_interface >( from_profile );
            if( video_raw_profile )
            {
                video_raw_profile->set_intrinsics( [video_from_profile]()
                {
                    if( video_from_profile )
                        return video_from_profile->get_intrinsics();
                    else
                        return rs2_intrinsics{};
                } );

                // Hack for L515 confidence.
                // Requesting source resolution from the camera, getting frame size of target (*2 y axis resolution)
                video_raw_profile->set_dims( video_from_profile->get_width(), video_from_profile->get_height() );
            }
        }
    }
}

void formats_converter::cache_from_profiles( const stream_profiles & from_profiles )
{
    for( auto & from_profile : from_profiles )
    {
        _format_mapping_to_from_profiles[from_profile->get_format()].push_back( from_profile );
    }
}

void formats_converter::clear_active_cache()
{
    _raw_profile_to_converters.clear();
    _format_mapping_to_from_profiles.clear();
}

stream_profiles formats_converter::get_active_source_profiles() const
{
    stream_profiles active_source_profiles;

    for( auto & iter : _raw_profile_to_converters )
    {
        active_source_profiles.push_back( iter.first );
    }

    return active_source_profiles;
}

std::vector< std::shared_ptr< processing_block > > formats_converter::get_active_converters() const
{
    std::vector< std::shared_ptr< processing_block > > active_converters;

    for( auto & source_converters : _raw_profile_to_converters )
    {
        active_converters.insert( active_converters.end(),
                                  source_converters.second.begin(),
                                  source_converters.second.end() );
    }

    return active_converters;
}

std::pair< std::shared_ptr< processing_block_factory >, stream_profiles >
formats_converter::find_pbf_matching_most_profiles( const stream_profiles & from_profiles )
{
    // Find and retrieve best fitting processing block to the given requests, and the requests which were the best fit.

    // For video stream, the best fitting processing block is defined as the processing block which its sources
    // covers the maximum amount of requests.

    stream_profiles best_match_profiles;
    std::shared_ptr< processing_block_factory > best_match_processing_block_factory;

    size_t best_source_size = 0;
    size_t max_satisfied_count = 0;

    for( auto & pbf : _pb_factories )
    {
        stream_profiles satisfied_req = pbf->find_satisfied_requests( from_profiles, _pbf_supported_profiles[pbf.get()] );
        size_t satisfied_count = satisfied_req.size();
        size_t source_size = pbf->get_source_info().size();
        if( satisfied_count > max_satisfied_count ||
           ( satisfied_count == max_satisfied_count && source_size < best_source_size ) )
        {
            max_satisfied_count = satisfied_count;
            best_source_size = source_size;
            best_match_processing_block_factory = pbf;
            best_match_profiles = std::move( satisfied_req );
        }
    }

    return { best_match_processing_block_factory, best_match_profiles };
}

void formats_converter::set_frames_callback( rs2_frame_callback_sptr callback )
{
    _converted_frames_callback = callback;

    // After processing callback
    auto output_cb = make_frame_callback( [&]( frame_holder f ) {
        std::vector< frame_interface * > frames_to_be_processed;
        frames_to_be_processed.push_back( f.frame );

        const auto & composite = dynamic_cast< composite_frame * >( f.frame );
        if( composite )
        {
            for( size_t i = 0; i < composite->get_embedded_frames_count(); i++ )
            {
                frames_to_be_processed.push_back( composite->get_frame( static_cast< int >( i ) ) );
            }
        }

        // Process only frames which aren't composite.
        for( auto & fr : frames_to_be_processed )
        {
            if( ! dynamic_cast< composite_frame * >( fr ) )
            {
                // We find a from profile with the same format+index+type as the frame profile and save it back
                // to the frame. Reason - viewer uses syncher and matcher that uses rs2::stream_profile.clone()
                // that generates a new ID for the clone and than the match can fail.
                auto cached_from_profile = find_cached_profile_for_frame( fr );

                if( cached_from_profile )
                {
                    fr->set_stream( cached_from_profile );
                }
                else
                    continue;

                fr->acquire();
                if( _converted_frames_callback )
                    _converted_frames_callback->on_frame( (rs2_frame *)fr );
            }
        }
    } );

    // Set callbacks for all of the relevant processing blocks
    for( const auto & entry : _raw_profile_to_converters )
    {
        auto & converters = entry.second;
        for( auto & converter : converters )
            if( converter )
            {
                converter->set_output_callback( output_cb );
            }
    }
}

void formats_converter::convert_frame( frame_holder & f )
{
    if( ! f )
        return;

    auto & converters = _raw_profile_to_converters[f->get_stream()];
    for( auto & converter : converters )
    {
        f->acquire();
        converter->invoke( f.frame );
    }
}

std::shared_ptr< stream_profile_interface > formats_converter::find_cached_profile_for_frame( const frame_interface * f )
{
    const auto & iter = _format_mapping_to_from_profiles.find( f->get_stream()->get_format() );
    if( iter == _format_mapping_to_from_profiles.end() )
        return nullptr;

    auto & from_profiles = iter->second;
    auto from_profile = std::find_if( begin( from_profiles ),
                                      end( from_profiles ),
                                      [&f]( const std::shared_ptr< stream_profile_interface > & prof ) {
                                          return ( prof->get_stream_index() == f->get_stream()->get_stream_index() &&
                                                   prof->get_stream_type() == f->get_stream()->get_stream_type() );
                                      } );

    return from_profile != end( from_profiles ) ? *from_profile : nullptr;
}

} // namespace librealsense
