// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "matcher-factory.h"
#include "frame-holder.h"
#include "stream-interface.h"

#include <src/sync.h>
#include <stdexcept>


namespace librealsense {


// A frame number only means something within the sensor that produced it. Depth and the IRs come off one
// sensor and share a counter, so they can be matched exactly; color comes off another with a counter of its
// own, and the only thing the two have in common is time. Every matcher here is therefore built the same way:
//
//     timestamp( frame_number( depth, ir1, ir2 ),      <- one sensor, matched exactly
//                frame_number( color, ... ) )          <- another sensor, matched exactly
//                                                         the two related only by time
//
// rs2_matchers names which streams a device emits together, since a stream_interface does not say which
// sensor produced it. Perception streams are not produced per depth frame at all and are never matched.


// The profiles named by 'group'. Returns none unless every selector found a stream: a group is a set that
// must be complete, not a filter - two streams of three have no common frame to match on.
std::vector< stream_interface * > matcher_factory::select_group( std::vector< match_group > const & group,
                                                                 std::vector< stream_interface * > const & profiles )
{
    std::vector< stream_interface * > selected;
    for( auto & selector : group )
    {
        auto const before = selected.size();
        for( auto & profile : profiles )
            if( profile->get_stream_type() == selector.stream
                && ( selector.index < 0 || profile->get_stream_index() == selector.index ) )
                selected.push_back( profile );

        if( selected.size() == before )
            return {};
    }

    return selected;
}


// Relates the given profiles to one another: one matcher per stream, composed by frame number, by timestamp,
// or - for streams that are matched to nothing - not at all, each passing through on its own.
std::shared_ptr< matcher > matcher_factory::relate( std::vector< stream_interface * > const & profiles, sync_by sync )
{
    std::vector< std::shared_ptr< matcher > > matchers;
    for( auto & p : profiles )
        matchers.push_back( create_identity_matcher( p ) );

    switch( sync )
    {
    case sync_by::frame_number:
        return create_frame_number_composite_matcher( matchers );
    case sync_by::timestamp:
        return create_timestamp_composite_matcher( matchers );
    default:
        return create_composite_identity_matcher( matchers );
    }
}


// One sensor's streams, related to each other. An incomplete group leaves nothing to match on, so every
// stream falls back to being related by timestamp.
std::shared_ptr< matcher > matcher_factory::match_sensor_profiles( std::vector< match_group > const & group,
                                                                   sync_by sync,
                                                                   std::vector< stream_interface * > const & profiles )
{
    auto const selected = select_group( group, profiles );
    if( selected.empty() )
    {
        LOG_DEBUG( "Created default matcher" );
        return relate( profiles, sync_by::timestamp );
    }

    return relate( selected, sync );
}


// Two sensors: the group, and color. Each is related within itself, and the two to each other by timestamp.
// Without color there is no second sensor, so everything falls back to being related by timestamp.
std::shared_ptr< matcher >
matcher_factory::match_sensor_profiles_with_color( std::vector< match_group > const & group,
                                                   sync_by sync,
                                                   std::vector< stream_interface * > const & profiles )
{
    // A stream belongs to one matcher only
    std::vector< stream_interface * > rest;
    auto const color = separate_color( profiles, rest );
    if( color.empty() )
    {
        LOG_DEBUG( "Created default matcher" );
        return relate( profiles, sync_by::timestamp );
    }

    return create_timestamp_composite_matcher( { match_sensor_profiles( group, sync, rest ),
                                                 relate( color, sync_by::frame_number ) } );
}


std::shared_ptr< matcher > matcher_factory::create( rs2_matchers matcher,
                                                    std::vector< stream_interface * > const & profiles )
{
    // Depth is selected without an index: a device may expose more than one depth stream - raw next to
    // device-aligned - and they are produced from the same frame, so they carry its frame number.
    static const std::vector< match_group > di = { { RS2_STREAM_DEPTH, -1 }, { RS2_STREAM_INFRARED, 1 } };
    static const std::vector< match_group > dlr = { { RS2_STREAM_DEPTH, -1 },
                                                    { RS2_STREAM_INFRARED, 1 },
                                                    { RS2_STREAM_INFRARED, 2 } };
    static const std::vector< match_group > dic = { { RS2_STREAM_DEPTH, -1 },
                                                    { RS2_STREAM_INFRARED, -1 },
                                                    { RS2_STREAM_CONFIDENCE, -1 } };

    switch( matcher )
    {
        case RS2_MATCHER_DI:    return match_sensor_profiles( di, sync_by::frame_number, profiles );
        case RS2_MATCHER_DLR:   return match_sensor_profiles( dlr, sync_by::frame_number, profiles );
        // Confidence does not share the depth frame counter, so this group is related by timestamp
        case RS2_MATCHER_DIC:   return match_sensor_profiles( dic, sync_by::timestamp, profiles );

        case RS2_MATCHER_DI_C:  return match_sensor_profiles_with_color( di, sync_by::frame_number, profiles );
        case RS2_MATCHER_DIC_C: return match_sensor_profiles_with_color( dic, sync_by::timestamp, profiles );

        case RS2_MATCHER_DLR_C:
        {
            std::vector< stream_interface * > rest;
            auto const perception = separate_perception( profiles, rest );
            auto const matched = match_sensor_profiles_with_color( dlr, sync_by::frame_number, rest );
            if( perception.empty() )
                return matched;

            return create_composite_identity_matcher( { matched, relate( perception, sync_by::nothing ) } );
        }

        case RS2_MATCHER_DEFAULT:
        default:
        {
            // No group at all: whatever is streaming, related by timestamp
            std::vector< stream_interface * > rest;
            auto const perception = separate_perception( profiles, rest );
            auto const matched = relate( rest, sync_by::timestamp );
            if( perception.empty() )
                return matched;

            return create_composite_identity_matcher( { matched, relate( perception, sync_by::nothing ) } );
        }
    }
}


std::shared_ptr< matcher > matcher_factory::create_identity_matcher( stream_interface * profile )
{
    return std::make_shared< identity_matcher >( profile->get_unique_id(), profile->get_stream_type() );
}


std::shared_ptr< matcher >
matcher_factory::create_composite_identity_matcher( std::vector< std::shared_ptr< matcher > > const & matchers )
{
    return std::make_shared< composite_identity_matcher >( matchers );
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


std::vector< stream_interface * > matcher_factory::separate_color( std::vector< stream_interface * > const & profiles,
                                                                   std::vector< stream_interface * > & rest )
{
    std::vector< stream_interface * > color;
    for( auto & profile : profiles )
        if( profile->get_stream_type() == RS2_STREAM_COLOR )
            color.push_back( profile );
        else
            rest.push_back( profile );

    return color;
}


std::vector< stream_interface * > matcher_factory::separate_perception( std::vector< stream_interface * > const & profiles,
                                                                        std::vector< stream_interface * > & rest )
{
    std::vector< stream_interface * > perception;
    for( auto & profile : profiles )
        if( profile->get_stream_type() == RS2_STREAM_OBJECT_DETECTION )
            perception.push_back( profile );
        else
            rest.push_back( profile );

    return perception;
}


}  // namespace librealsense
