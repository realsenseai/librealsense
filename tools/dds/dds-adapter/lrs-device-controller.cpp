// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024-5 RealSense, Inc. All Rights Reserved.

#include "lrs-device-controller.h"

#include <common/metadata-helper.h>

#include <realdds/topics/image-msg.h>
#include <realdds/topics/imu-msg.h>
#include <realdds/topics/blob-msg.h>
#include <realdds/topics/dds-topic-names.h>
#include <realdds/topics/flexible-msg.h>
#include <realdds/topics/dds-topic-names.h>
#include <realdds/dds-device-server.h>
#include <realdds/dds-stream-server.h>
#include <realdds/dds-topic-reader-thread.h>
#include <realdds/dds-topic-writer.h>
#include <realdds/dds-participant.h>
#include <realdds/dds-guid.h>
#include <realdds/dds-sample.h>

#include <realdds/topics/ros2/get-parameters-msg.h>
#include <realdds/topics/ros2/set-parameters-msg.h>
#include <realdds/topics/ros2/list-parameters-msg.h>
#include <realdds/topics/ros2/describe-parameters-msg.h>
#include <realdds/topics/ros2/rcl_interfaces/msg/ParameterType.h>
#include <realdds/topics/ros2/participant-entities-info-msg.h>
#include <realdds/topics/ros2/parameter-events-msg.h>

#include <rsutils/number/crc32.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/json.h>
#include <rsutils/string/hexarray.h>
#include <rsutils/string/hexdump.h>
#include <rsutils/time/timer.h>
#include <rsutils/string/from.h>
#include <rsutils/ios/field.h>

#include <algorithm>
#include <iostream>

using rsutils::string::hexarray;
using rsutils::json;
using namespace realdds;
using tools::lrs_device_controller;
using field = rsutils::ios::field;

static constexpr const double MILLISEC_TO_SEC = 0.001;

#define CREATE_SERVER_IF_NEEDED( X )                                                                                   \
    if( server )                                                                                                       \
    {                                                                                                                  \
        if( strcmp( server->type_string(), #X ) )                                                                      \
        {                                                                                                              \
            LOG_ERROR( #X " profile type on a stream '" << stream_name << "' that already has type "                   \
                                                        << server->type_string() );                                    \
            return;                                                                                                    \
        }                                                                                                              \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        server = std::make_shared< realdds::dds_##X##_stream_server >( stream_name, sensor_name );                     \
    }                                                                                                                  \
    break


realdds::distortion_model to_realdds( rs2_distortion model )
{
    switch( model )
    {
    case RS2_DISTORTION_BROWN_CONRADY: return realdds::distortion_model::brown;
    case RS2_DISTORTION_NONE: return realdds::distortion_model::none;
    case RS2_DISTORTION_INVERSE_BROWN_CONRADY: return realdds::distortion_model::inverse_brown;
    case RS2_DISTORTION_MODIFIED_BROWN_CONRADY: return realdds::distortion_model::modified_brown;

    default:
        throw std::runtime_error( "unexpected rs2 distortion model: " + std::string( rs2_distortion_to_string( model ) ) );
    }
}

realdds::video_intrinsics to_realdds( const rs2_intrinsics & intr )
{
    realdds::video_intrinsics ret;

    ret.width = intr.width;
    ret.height = intr.height;
    ret.principal_point.x = intr.ppx;
    ret.principal_point.y = intr.ppy;
    ret.focal_length.x = intr.fx;
    ret.focal_length.y = intr.fy;
    ret.distortion.model = realdds::distortion_model::none;
    for( auto coeff : intr.coeffs )
    {
        if( coeff != 0.f )
        {
            ret.distortion.model = to_realdds( intr.model );
            memcpy( ret.distortion.coeffs.data(), intr.coeffs, sizeof( ret.distortion.coeffs ) );
            break;
        }
    }

    return ret;
}

realdds::motion_intrinsics to_realdds( const rs2_motion_device_intrinsic & rs2_intr )
{
    realdds::motion_intrinsics intr;

    memcpy( intr.data.data(), rs2_intr.data, sizeof( intr.data ) );
    memcpy( intr.noise_variances.data(), rs2_intr.noise_variances, sizeof( intr.noise_variances ) );
    memcpy( intr.bias_variances.data(), rs2_intr.bias_variances, sizeof( intr.bias_variances ) );

    return intr;
}

realdds::extrinsics to_realdds( const rs2_extrinsics & rs2_extr )
{
    realdds::extrinsics extr;

    memcpy( extr.rotation.data(), rs2_extr.rotation, sizeof( extr.rotation ) );
    memcpy( extr.translation.data(), rs2_extr.translation, sizeof( extr.translation ) );

    return extr;
}


static std::string stream_name_from_rs2( const rs2::stream_profile & profile )
{
    // ROS stream names cannot contain spaces! We use underscores instead:
    std::string stream_name;
    switch( profile.stream_type() )
    {
    case RS2_STREAM_ACCEL:
    case RS2_STREAM_GYRO:
        stream_name = "Motion";
        break;
    default:
        stream_name = rs2_stream_to_string( profile.stream_type() );
        if( profile.stream_index() )
            stream_name += '_' + std::to_string( profile.stream_index() );
        break;
    }
    return stream_name;
}


static std::string stream_name_from_rs2( rs2::sensor const & sensor )
{
    static std::map< std::string, std::string > sensor_stream_name{
        { "Stereo Module", rs2_stream_to_string( RS2_STREAM_DEPTH ) },
        { "RGB Camera", rs2_stream_to_string( RS2_STREAM_COLOR ) },
        { "Motion Module", "Motion" },
    };

    auto it = sensor_stream_name.find( sensor.get_info( RS2_CAMERA_INFO_NAME ) );
    if( it == sensor_stream_name.end() )
        return {};
    return it->second;
}


std::vector< char const * > get_option_enum_values( rs2::sensor const & sensor,
                                                    rs2_option const opt,
                                                    rs2::option_range const & range,
                                                    float const current_value,
                                                    size_t * p_current_index,
                                                    size_t * p_default_index )
{
    // Same logic as in Viewer's option-model...
    if( range.step < 0.9f )
        return {};

    size_t current_index = 0, default_index = 0;
    std::vector< const char * > labels;
    for( auto i = range.min; i <= range.max; i += range.step )
    {
        auto label = sensor.get_option_value_description( opt, i );
        if( ! label )
            return {};  // Missing value - not an enum

        if( std::fabs( i - current_value ) < 0.001f )
            current_index = labels.size();
        if( std::fabs( i - range.def ) < 0.001f )
            default_index = labels.size();

        labels.push_back( label );
    }
    if( p_current_index )
        *p_current_index = current_index;
    if( p_default_index )
        *p_default_index = default_index;
    return labels;
}


static json json_from_roi( rs2::region_of_interest const & roi )
{
    return realdds::dds_rect_option::type{ roi.min_x, roi.min_y, roi.max_x, roi.max_y }.to_json();
}


static rs2::region_of_interest roi_from_json( json const & j )
{
    auto roi = realdds::dds_rect_option::type::from_json( j );
    return { roi.x1, roi.y1, roi.x2, roi.y2 };
}


std::vector< std::shared_ptr< realdds::dds_stream_server > > lrs_device_controller::get_supported_streams()
{
    std::map< std::string, realdds::dds_stream_profiles > stream_name_to_profiles;
    std::map< std::string, size_t > stream_name_to_default_profile;
    std::map< std::string, std::set< realdds::video_intrinsics > > stream_name_to_video_intrinsics;

    // Iterate over all profiles of all sensors and build appropriate dds_stream_servers
    for( auto & sensor : _rs_dev.query_sensors() )
    {
        std::string const sensor_name = sensor.get_info( RS2_CAMERA_INFO_NAME );
        // We keep a copy of the sensors throughout the run time:
        // Otherwise problems could arise like opening streams and they would close at on_open_streams scope end.
        _rs_sensors[sensor_name] = sensor;

        auto stream_profiles = sensor.get_stream_profiles();
        std::for_each( stream_profiles.begin(), stream_profiles.end(), [&]( const rs2::stream_profile & sp )
        {
            std::string stream_name = stream_name_from_rs2( sp );

            //Create a realdds::dds_stream_server object for each unique profile type+index
            auto & server = _stream_name_to_server[stream_name];
            switch( sp.stream_type() )
            {
            case RS2_STREAM_DEPTH: CREATE_SERVER_IF_NEEDED( depth );
            case RS2_STREAM_INFRARED: CREATE_SERVER_IF_NEEDED( ir );
            case RS2_STREAM_COLOR: CREATE_SERVER_IF_NEEDED( color );
            case RS2_STREAM_CONFIDENCE: CREATE_SERVER_IF_NEEDED( confidence );
            case RS2_STREAM_ACCEL:
            case RS2_STREAM_GYRO:
                CREATE_SERVER_IF_NEEDED( motion );
            default:
                LOG_ERROR( "unsupported stream type " << sp.stream_type() );
                return;
            }

            // Create appropriate realdds::profile for each sensor profile and map to a stream
            auto & profiles = stream_name_to_profiles[stream_name];
            std::shared_ptr< realdds::dds_stream_profile > profile;
            bool insert_profile = true;
            if( auto const vsp = rs2::video_stream_profile( sp ) )
            {
                profile = std::make_shared< realdds::dds_video_stream_profile >(
                    static_cast< int16_t >( vsp.fps() ),
                    realdds::dds_video_encoding::from_rs2( vsp.format() ),
                    static_cast< uint16_t >( vsp.width() ),
                    static_cast< int16_t >( vsp.height() ) );
                try
                {
                    auto intr = to_realdds( vsp.get_intrinsics() );
                    stream_name_to_video_intrinsics[stream_name].insert( intr );
                }
                catch( ... ) {} //Some profiles don't have intrinsics
            }
            else if( auto const msp = rs2::motion_stream_profile( sp ) )
            {
                profile = std::make_shared< realdds::dds_motion_stream_profile >(
                    static_cast< int16_t >( msp.fps() ) );

                auto motion_server = std::dynamic_pointer_cast< dds_motion_stream_server >( server );
                if( RS2_STREAM_ACCEL == msp.stream_type() )
                {
                    insert_profile = false;  // We report only Gyro profiles
                    motion_server->set_accel_intrinsics( to_realdds( msp.get_motion_intrinsics() ) );
                }
                else
                    motion_server->set_gyro_intrinsics( to_realdds( msp.get_motion_intrinsics() ) );
            }
            else
            {
                LOG_ERROR( "unknown profile type of uid " << sp.unique_id() );
                return;
            }
            if( insert_profile )
            {
                if( sp.is_default() )
                    stream_name_to_default_profile[stream_name] = profiles.size();
                profiles.push_back( profile );
                LOG_DEBUG( stream_name << ": " << profile->to_string() );
            }
        } );
    }

    override_default_profiles( stream_name_to_profiles, stream_name_to_default_profile );

    // Iterate over the mapped streams and initialize
    std::vector< std::shared_ptr< realdds::dds_stream_server > > servers;
    std::set< std::string > sensors_handled;
    for( auto & it : stream_name_to_profiles )
    {
        auto const & stream_name = it.first;

        size_t default_profile_index = 0;
        auto default_profile_it = stream_name_to_default_profile.find( stream_name );
        if( default_profile_it != stream_name_to_default_profile.end() )
            default_profile_index = default_profile_it->second;
        else
            LOG_ERROR( "no default profile found; using first available in " << stream_name );

        auto const & profiles = it.second;
        if( profiles.empty() )
        {
            LOG_ERROR( "ignoring stream '" << stream_name << "' with no profiles" );
            continue;
        }
        auto server = _stream_name_to_server[stream_name];
        if( ! server )
        {
            LOG_ERROR( "ignoring stream '" << stream_name << "' with no server" );
            continue;
        }

        if( auto video_server = std::dynamic_pointer_cast< dds_video_stream_server >( server ) )
        {
            video_server->set_intrinsics( std::move( stream_name_to_video_intrinsics[stream_name] ) );
            // Set stream metadata support (currently if the device supports metadata all streams does)
            // Must be done before calling init_profiles()
            if( _md_enabled )
                server->enable_metadata();
        }

        server->init_profiles( profiles, default_profile_index );

        // Get supported options and recommended filters for this stream
        for( auto & sensor : _rs_dev.query_sensors() )
        {
            std::string const sensor_name = sensor.get_info( RS2_CAMERA_INFO_NAME );
            if( server->sensor_name().compare( sensor_name ) != 0 )
                continue;

            // Multiple streams map to the same sensor so we may go over the same sensor multiple times - we
            // only need to do this once per sensor!
            if( sensors_handled.emplace( sensor_name ).second )
            {
                realdds::dds_options stream_options;
                auto supported_options = sensor.get_supported_options();
                for( auto option_id : supported_options )
                {
                    // Certain options are automatically added by librealsense and shouldn't actually be shared
                    if( option_id == RS2_OPTION_FRAMES_QUEUE_SIZE )
                        continue;  // Added automatically for every sensor_base

                    std::string option_name = sensor.get_option_name( option_id );
                    try
                    {
                        json j = json::array();
                        json props = json::array();
                        j += option_name;
                        // Even read-only options have ranges in librealsense
                        auto const range = sensor.get_option_range( option_id );
                        try
                        {
                            // For now, assume (legacy) librealsense options are all floats
                            float option_value = sensor.get_option( option_id );  // may throw
                            size_t current_index, default_index;
                            auto const values = get_option_enum_values( sensor, option_id, range, option_value, &current_index, &default_index );
                            if( ! values.empty() )
                            {
                                // Translate to enum
                                j += values[current_index];
                                j += values;
                                j += values[default_index];
                            }
                            else
                            {
                                j += option_value;
                                j += range.min;
                                j += range.max;
                                j += range.step;
                                j += range.def;
                            }
                        }
                        catch( ... )
                        {
                            // Some options can be queried only if certain conditions exist skip them for now
                            props += "optional";
                            j += rsutils::null_json;
                            j += range.min;
                            j += range.max;
                            j += range.step;
                            j += range.def;
                        }
                        j += sensor.get_option_description( option_id );
                        if( sensor.is_option_read_only( option_id ) )
                            props += "read-only";
                        if( ! props.empty() )
                            j += props;
                        auto dds_opt = realdds::dds_option::from_json( j );
                        LOG_DEBUG( "... option -> " << j << " -> " << dds_opt->to_json() );
                        stream_options.push_back( dds_opt );  // TODO - filter options relevant for stream type
                    }
                    catch( ... )
                    {
                        LOG_ERROR( "Cannot query details of option " << option_id );
                        // Some options can be queried only if certain conditions exist skip them for now
                    }
                }

                if( auto roi_sensor = rs2::roi_sensor( sensor ) )
                {
                    // AE ROI is exposed as an interface in the librealsense API and through a "Region of Interest"
                    // rectangle option in DDS
                    json j = json::array();
                    j += rs2_option_to_string( RS2_OPTION_REGION_OF_INTEREST );
                    json option_value;  // null - no value
                    try
                    {
                        option_value = json_from_roi( roi_sensor.get_region_of_interest() );
                    }
                    catch( ... )
                    {
                        // May be available only during streaming
                    }
                    j += option_value;
                    j += nullptr;                                 // No default value
                    j += "Region of Interest for Auto Exposure";  // Description
                    j += json::array( { "optional" } );           // Properties
                    auto dds_opt = realdds::dds_option::from_json( j );
                    stream_options.push_back( dds_opt );
                }
                server->init_options( stream_options );
            }
        }

        servers.push_back( server );
    }

    return servers;
}

#undef CREATE_SERVER_IF_NEEDED

extrinsics_map get_extrinsics_map( const rs2::device & dev )
{
    extrinsics_map ret;
    std::map< std::string, rs2::stream_profile > stream_name_to_rs2_stream_profile;

    // Iterate over profiles of all sensors and split to streams
    for( auto & sensor : dev.query_sensors() )
    {
        auto stream_profiles = sensor.get_stream_profiles();
        std::for_each( stream_profiles.begin(),
                       stream_profiles.end(),
                       [&]( const rs2::stream_profile & sp )
                       {
                           if( RS2_STREAM_ACCEL == sp.stream_type() )
                               return;  // Ignore the accelerometer; we want the gyro extrinsics!
                           std::string stream_name = stream_name_from_rs2( sp );
                           auto & rs2_stream_profile = stream_name_to_rs2_stream_profile[stream_name];
                           if( ! rs2_stream_profile )
                               rs2_stream_profile = sp;  // Any profile of this stream will do, take the first
                       } );
    }

    // For each stream, get extrinsics to all other streams
    for( auto & from : stream_name_to_rs2_stream_profile )
    {
        auto const & from_stream_name = from.first;
        for( auto & to : stream_name_to_rs2_stream_profile )
        {
            auto & to_stream_name = to.first;
            if( from_stream_name != to_stream_name )
            {
                // Get rs2::stream_profile objects for get_extrinsics API call
                const rs2::stream_profile & from_profile = from.second;
                const rs2::stream_profile & to_profile = to.second;
                const auto & extrinsics = from_profile.get_extrinsics_to( to_profile );
                ret[std::make_pair( from_stream_name, to_stream_name )] =
                    std::make_shared< realdds::extrinsics >( to_realdds( extrinsics ) );
                LOG_DEBUG( "have extrinsics from " << from_stream_name << " to " << to_stream_name );
            }
        }
    }

    return ret;
}


std::shared_ptr< dds_stream_profile > create_dds_stream_profile( std::string const & type_string, json const & j )
{
    if( "motion" == type_string )
        return dds_stream_profile::from_json< dds_motion_stream_profile >( j );

    static const std::set< std::string > video_types = { "depth", "color", "ir", "confidence" };
    if( video_types.find( type_string ) != video_types.end() )
        return dds_stream_profile::from_json< dds_video_stream_profile >( j );

    throw std::runtime_error( "unsupported stream type '" + type_string + "'" );
}


rs2_stream stream_name_to_type( std::string const & type_string )
{
    static const std::map< std::string, rs2_stream > type_to_rs2 = {
        { "Depth", RS2_STREAM_DEPTH },
        { "Color", RS2_STREAM_COLOR },
        { "Infrared", RS2_STREAM_INFRARED },
        { "Infrared_1", RS2_STREAM_INFRARED },
        { "Infrared_2", RS2_STREAM_INFRARED },
        { "Motion", RS2_STREAM_GYRO },  // We report only gyro profiles
        { "Gpio", RS2_STREAM_GPIO },
        { "Confidence", RS2_STREAM_CONFIDENCE },
    };
    auto it = type_to_rs2.find( type_string );
    if( it == type_to_rs2.end() )
    {
        LOG_ERROR( "Unknown stream type '" << type_string << "'" );
        return RS2_STREAM_ANY;
    }
    return it->second;
}


int stream_name_to_index( std::string const & type_string )
{
    int index = 0;
    static const std::map< std::string, int > type_to_index = {
        { "Infrared_1", 1 },
        { "Infrared_2", 2 },
    };
    auto it = type_to_index.find( type_string );
    if( it != type_to_index.end() )
    {
        index = it->second;
    }

    return index;
}

rs2_option option_name_to_id( const std::string & name )
{
    auto option_id = rs2_option_from_string( name.c_str() );
    if( RS2_OPTION_COUNT == option_id )
        throw std::runtime_error( "Option '" + name + "' not found" );
    return option_id;
}


bool profiles_are_compatible( std::shared_ptr< dds_stream_profile > const & p1,
                              std::shared_ptr< dds_stream_profile > const & p2,
                              bool any_encoding = false )
{
    auto vp1 = std::dynamic_pointer_cast< realdds::dds_video_stream_profile >( p1 );
    auto vp2 = std::dynamic_pointer_cast< realdds::dds_video_stream_profile >( p2 );
    if( ! ! vp1 != ! ! vp2 )
        return false;  // types aren't the same
    if( vp1 && vp2 )
    {
        if( vp1->width() != vp2->width() || vp1->height() != vp2->height() )
            return false;
        if( ! any_encoding && vp1->encoding() != vp2->encoding() )
            return false;
    }
    return p1->frequency() == p2->frequency();
}


rs2::stream_profile get_required_profile( const rs2::sensor & sensor,
                                          std::vector< rs2::stream_profile > const & sensor_stream_profiles,
                                          std::string const & stream_name,
                                          std::shared_ptr< dds_stream_profile > const & profile )
{
    auto const stream_type = stream_name_to_type( stream_name );
    auto const stream_index = stream_name_to_index( stream_name );

    auto profile_iter = std::find_if( sensor_stream_profiles.begin(),
                                      sensor_stream_profiles.end(),
                                      [&]( rs2::stream_profile const & sp ) {
                                          auto vp = sp.as< rs2::video_stream_profile >();
                                          auto dds_vp = std::dynamic_pointer_cast< dds_video_stream_profile >( profile );
                                          bool video_params_match = ( vp && dds_vp )
                                                                      ? vp.width() == dds_vp->width()
                                                                            && vp.height() == dds_vp->height()
                                                                            && vp.format() == dds_vp->encoding().to_rs2()
                                                                      : true;
                                          return sp.stream_type() == stream_type
                                              && sp.stream_index() == stream_index
                                              && sp.fps() == profile->frequency()
                                              && video_params_match;
                                      } );
    if( profile_iter == sensor_stream_profiles.end() )
    {
        throw std::runtime_error( "Could not find required profile" );
    }

    return *profile_iter;
}


static std::shared_ptr< realdds::dds_stream_profile >
find_profile( std::shared_ptr< realdds::dds_stream_server > const & stream,
              std::shared_ptr< realdds::dds_stream_profile > const & profile,
              bool any_encoding = false )
{
    auto & stream_profiles = stream->profiles();
    auto it = std::find_if( stream_profiles.begin(),
                            stream_profiles.end(),
                            [&]( std::shared_ptr< realdds::dds_stream_profile > const & sp )
                            {
                                return profiles_are_compatible( sp, profile, any_encoding );
                            } );
    std::shared_ptr< realdds::dds_stream_profile > found_profile;
    if( it != stream_profiles.end() )
        found_profile = *it;
    return found_profile;
}


std::shared_ptr< realdds::dds_stream_server >
lrs_device_controller::frame_to_streaming_server( rs2::frame const & f, rs2::stream_profile * p_profile ) const
{
    rs2::stream_profile profile_;
    if( ! p_profile )
        p_profile = &profile_;
    rs2::stream_profile & profile = *p_profile;

    profile = f.get_profile();
    auto const stream_name = stream_name_from_rs2( profile );
    auto it = _stream_name_to_server.find( stream_name );
    if( it == _stream_name_to_server.end() )
        return {};

    auto & server = it->second;
    if( ! _bridge.is_streaming( server ) )
        return {};

    return server;
}


lrs_device_controller::lrs_device_controller( rs2::device dev, std::shared_ptr< realdds::dds_device_server > dds_device_server )
    : _rs_dev( dev )
    , _dds_device_server( dds_device_server )
    , _control_dispatcher( QUEUE_MAX_SIZE )
{
    if( ! _dds_device_server )
        throw std::runtime_error( "Empty dds_device_server" );

    _dds_device_server->on_set_option( [&]( const std::shared_ptr< realdds::dds_option > & option, json & value ) {
        set_option( option, value );
    } );
    _dds_device_server->on_query_option( [&]( const std::shared_ptr< realdds::dds_option > & option ) -> json {
        return query_option( option );
    } );
    _dds_device_server->on_control(
        [this]( std::string const & id, json const & control, json & reply )
        { return on_control( id, control, reply ); } );

    std::vector< std::shared_ptr< realdds::dds_stream_server > > supported_streams;
    extrinsics_map extrinsics;
    realdds::dds_options options;

    if( is_recovery() )
    {
        _device_sn = _rs_dev.get_info( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID );
        LOG_DEBUG( "LRS device manager for recovery device: " << _device_sn << " created" );

        // Initialize with nothing: no streams, no options, empty extrinsics, etc.
        _md_enabled = false;
        _dds_device_server->init( supported_streams, options, extrinsics );
        return;
    }

    _device_sn = _rs_dev.get_info( RS2_CAMERA_INFO_SERIAL_NUMBER );
    LOG_DEBUG( "LRS device manager for device: " << _device_sn << " created" );

    // Some camera models support metadata for frames. can_support_metadata will tell us if this model does.
    // Also, to get the metadata driver support needs to be enabled, requires administrator rights on Windows and Linux.
    // is_enabled will return current state. If one of the conditions is false we cannot get metadata from the device.
    _md_enabled = rs2::metadata_helper::instance().can_support_metadata( _rs_dev.get_info( RS2_CAMERA_INFO_PRODUCT_LINE ) )
               && rs2::metadata_helper::instance().is_enabled( _rs_dev.get_info( RS2_CAMERA_INFO_PHYSICAL_PORT ) );

    // Create a supported streams list for initializing the relevant DDS topics
    supported_streams = get_supported_streams();

    _bridge.on_start_sensor(
        [this]( std::string const & sensor_name, dds_stream_profiles const & active_profiles )
        {
            auto & sensor = _rs_sensors[sensor_name];
            auto rs2_profiles = get_rs2_profiles( active_profiles );
            sensor.open( rs2_profiles );
            if( sensor.is< rs2::motion_sensor >() )
            {
                struct imu_context
                {
                    realdds::topics::imu_msg message;
                    // We will be getting gyro and accel together, from different threads:
                    // Don't want to change something mid-send
                    std::mutex mutex;
                };
                sensor.start(
                    [this, imu = std::make_shared< imu_context >()]( rs2::frame f )
                    {
                        rs2::stream_profile stream_profile;
                        auto motion = std::dynamic_pointer_cast< realdds::dds_motion_stream_server >(
                            frame_to_streaming_server( f, &stream_profile ) );
                        if( ! motion )
                            return;

                        auto xyz = reinterpret_cast< float const * >( f.get_data() );
                        if( RS2_STREAM_ACCEL == stream_profile.stream_type() )
                        {
                            std::unique_lock< std::mutex > lock( imu->mutex );
                            imu->message.accel_data().x( xyz[0] );  // in m/s^2
                            imu->message.accel_data().y( xyz[1] );
                            imu->message.accel_data().z( xyz[2] );
                            return;  // Don't actually publish
                        }
                        imu->message.gyro_data().x( xyz[0] );  // rad/sec, which is what we need
                        imu->message.gyro_data().y( xyz[1] );
                        imu->message.gyro_data().z( xyz[2] );
                        imu->message.timestamp(  // in sec.nsec
                            static_cast< long double >( f.get_timestamp() ) * MILLISEC_TO_SEC );
                        std::unique_lock< std::mutex > lock( imu->mutex );
                        motion->publish_motion( std::move( imu->message ) );

                        // motion streams have no metadata!
                    } );
            }
            else
            {
                sensor.start(
                    [this]( rs2::frame f )
                    {
                        auto video = std::dynamic_pointer_cast< realdds::dds_video_stream_server >(
                            frame_to_streaming_server( f ) );
                        if( ! video )
                            return;

                        dds_time const timestamp  // in sec.nsec
                            ( static_cast< long double >( f.get_timestamp() ) * MILLISEC_TO_SEC );

                        realdds::topics::image_msg image;
                        image.set_height( video->get_image_header().height );
                        image.set_width( video->get_image_header().width );
                        image.set_timestamp( timestamp );
                        auto data = static_cast< const uint8_t * >( f.get_data() );
                        image.raw().data().assign( data, data + f.get_data_size() );
                        video->publish_image( image );

                        publish_frame_metadata( f, timestamp );
                    } );
            }
            std::cout << sensor_name << " sensor started" << std::endl;
        } );
    _bridge.on_stop_sensor(
        [this]( std::string const & sensor_name )
        {
            auto & sensor = _rs_sensors[sensor_name];
            sensor.stop();
            sensor.close();
            std::cout << sensor_name << " sensor stopped" << std::endl;
        } );
    _bridge.on_error(
        [this]( std::string const & error_string )
        {
            json j = json::object( {
                { "id", "error" },
                { "error", error_string },
            } );
            _dds_device_server->publish_notification( std::move( j ) );
        } );
    _bridge.init( supported_streams );
    _bridge.on_stream_profile_change(
        [this]( std::shared_ptr< realdds::dds_stream_server > const & server,
                std::shared_ptr< realdds::dds_stream_profile > const & profile )
        {
            // Update that this profile has changed
            if( _parameter_events_writer )
            {
                topics::ros2::parameter_events_msg msg;
                msg.set_node_name( _ros2_node_name );
                msg.set_timestamp( realdds::now() );
                topics::ros2::parameter_events_msg::param_type p;
                p.name( server->name() + "/profile" );
                p.value().type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_STRING );
                p.value().string_value( profile->to_json().dump() );
                msg.add_changed_param( std::move( p ) );
                msg.write_to( *_parameter_events_writer );
            }
        } );

    extrinsics = get_extrinsics_map( dev );

    // Initialize the DDS device server with the supported streams
    _dds_device_server->init( supported_streams, options, extrinsics );

    for( auto & name_sensor : _rs_sensors )
    {
        auto & sensor = name_sensor.second;
        sensor.on_options_changed(
            [this, weak_sensor = std::weak_ptr< rs2_sensor >( sensor.get() )]( rs2::options_list const & options )
            {
                if( auto strong_sensor = weak_sensor.lock() )
                {
                    rs2::sensor sensor( strong_sensor );
                    json option_values = json::object();
                    topics::ros2::parameter_events_msg msg;
                    for( auto changed_option : options )
                    {
                        std::string const option_name = sensor.get_option_name( changed_option->id );
                        std::string const stream_name = stream_name_from_rs2( sensor );
                        if( stream_name.empty() )
                        {
                            LOG_ERROR( "Unknown option '" << option_name
                                                          << "' stream: " << sensor.get_info( RS2_CAMERA_INFO_NAME ) );
                            continue;
                        }
                        auto dds_option = _dds_device_server->find_option( option_name, stream_name );
                        if( ! dds_option )
                        {
                            LOG_ERROR( "Missing option '" << option_name
                                                          << "' stream: " << sensor.get_info( RS2_CAMERA_INFO_NAME ) );
                            continue;
                        }
                        json value;
                        topics::ros2::parameter_events_msg::param_type p;
                        if( changed_option->is_valid )
                        {
                            switch( changed_option->type )
                            {
                            case RS2_OPTION_TYPE_FLOAT:
                                if( auto e = std::dynamic_pointer_cast< realdds::dds_enum_option >( dds_option ) )
                                {
                                    value = e->get_choices().at( int( changed_option->as_float ) );
                                    p.value().type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_STRING );
                                    p.value().string_value( value );
                                }
                                else
                                {
                                    value = changed_option->as_float;
                                    p.value().type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_DOUBLE );
                                    p.value().double_value( value );
                                }
                                break;
                            case RS2_OPTION_TYPE_STRING:
                                value = changed_option->as_string;
                                p.value().type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_STRING );
                                p.value().string_value( value );
                                break;
                            case RS2_OPTION_TYPE_INTEGER:
                                value = changed_option->as_integer;
                                p.value().type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_INTEGER );
                                p.value().integer_value( value );
                                break;
                            case RS2_OPTION_TYPE_BOOLEAN:
                                value = (bool)changed_option->as_integer;
                                p.value().type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_BOOL );
                                p.value().bool_value( value );
                                break;
                            default:
                                LOG_ERROR( "Unknown option '" << option_name << "' type: "
                                                              << rs2_option_type_to_string( changed_option->type ) );
                                continue;
                            }
                        }
                        else
                        {
                            // No value available
                            value = rsutils::null_json;
                        }
                        dds_option->set_value( std::move( value ) );
                        option_values[stream_name][option_name] = dds_option->get_value();
                        p.name( stream_name + '/' + option_name );
                        msg.add_changed_param( std::move( p ) );
                    }
                    if( option_values.size() )
                    {
                        json j = json::object( {
                            { realdds::topics::notification::key::id, realdds::topics::reply::query_options::id },
                            { realdds::topics::reply::query_options::key::option_values, std::move( option_values ) },
                        } );
                        LOG_DEBUG( "[" << _dds_device_server->debug_name() << "] options changed: " << std::setw( 4 ) << j );
                        _dds_device_server->publish_notification( std::move( j ) );
                        if( _parameter_events_writer )
                        {
                            // Also publish on the ROS2 topic
                            msg.set_node_name( _ros2_node_name );
                            msg.set_timestamp( realdds::now() );
                            msg.write_to( *_parameter_events_writer );
                        }
                    }
                }
            } );
    }

    if( auto ccd = _rs_dev.as< rs2::calibration_change_device >() )
    {
        ccd.register_calibration_change_callback(
            [this]( rs2_calibration_status status )
            {
                if( status == RS2_CALIBRATION_SUCCESSFUL )
                {
                    json j = json::object( {
                        { realdds::topics::notification::key::id,
                          realdds::topics::notification::calibration_changed::id },
                    } );
                    bool have_changes = update_stream_trinsics( &j );
                    if( have_changes )
                    {
                        LOG_INFO( "Calibration changes detected" );
                        LOG_DEBUG( std::setw( 4 ) << j );
                        _dds_device_server->publish_notification( std::move( j ) );
                    }
                }
            } );
    }
}


void lrs_device_controller::initialize_ros2_node_entities( std::string const & node_name,
                                                           std::shared_ptr< realdds::dds_topic_writer > parameter_events_writer )
{
    // Note that we need to use the node name and not necessarily the realsense topic-root (which could be different!)
    assert( '/' == *topics::ros2::NAMESPACE );
    _ros2_node_name = rsutils::string::from()  // E.g., "/realsense/D455_12345678"
        << topics::ros2::NAMESPACE
        << topics::SEPARATOR
        << node_name;
    // Create ROS2 request & response channels
    std::string const full_name = _ros2_node_name.substr( 1 );  // without the beginning /
    auto const request_root = topics::ros2::SERVICE_REQUEST_ROOT + full_name;
    auto const response_root = topics::ros2::SERVICE_RESPONSE_ROOT + full_name;
    auto participant = _dds_device_server->participant();
    auto subscriber = _dds_device_server->subscriber();
    auto publisher = _dds_device_server->publisher();
    if( auto topic = topics::ros2::get_parameters_request_msg::create_topic(
            participant,
            request_root + topics::ros2::GET_PARAMETERS_NAME + topics::ros2::REQUEST_SUFFIX ) )
    {
        _get_params_reader = std::make_shared< dds_topic_reader >( topic, subscriber );
        _get_params_reader->on_data_available( [&] { on_get_params_request(); } );
        dds_topic_reader::qos rqos( eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS );
        rqos.override_from_json( participant->settings().nested( "device", "get-parameters-request" ) );
        _get_params_reader->run( rqos );
    }
    if( auto topic = topics::ros2::get_parameters_response_msg::create_topic(
            participant,
            response_root + topics::ros2::GET_PARAMETERS_NAME + topics::ros2::RESPONSE_SUFFIX ) )
    {
        _get_params_writer = std::make_shared< dds_topic_writer >( topic, publisher );
        dds_topic_writer::qos wqos( eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS );
        wqos.history().depth = 10;  // default is 1
        _get_params_writer->override_qos_from_json( wqos,
            participant->settings().nested( "device", "get-parameters-response" ) );
        _get_params_writer->run( wqos );
    }
    if( auto topic = topics::ros2::set_parameters_request_msg::create_topic(
            participant,
            request_root + topics::ros2::SET_PARAMETERS_NAME + topics::ros2::REQUEST_SUFFIX ) )
    {
        _set_params_reader = std::make_shared< dds_topic_reader >( topic, subscriber );
        _set_params_reader->on_data_available( [&] { on_set_params_request(); } );
        dds_topic_reader::qos rqos( eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS );
        rqos.override_from_json( participant->settings().nested( "device", "set-parameters-request" ) );
        _set_params_reader->run( rqos );
    }
    if( auto topic = topics::ros2::set_parameters_response_msg::create_topic(
            participant,
            response_root + topics::ros2::SET_PARAMETERS_NAME + topics::ros2::RESPONSE_SUFFIX ) )
    {
        _set_params_writer = std::make_shared< dds_topic_writer >( topic, publisher );
        dds_topic_writer::qos wqos( eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS );
        wqos.history().depth = 10;  // default is 1
        _set_params_writer->override_qos_from_json( wqos,
            participant->settings().nested( "device", "set-parameters-response" ) );
        _set_params_writer->run( wqos );
    }
    if( auto topic = topics::ros2::list_parameters_request_msg::create_topic(
            participant,
            request_root + topics::ros2::LIST_PARAMETERS_NAME + topics::ros2::REQUEST_SUFFIX ) )
    {
        _list_params_reader = std::make_shared< dds_topic_reader >( topic, subscriber );
        _list_params_reader->on_data_available( [&] { on_list_params_request(); } );
        dds_topic_reader::qos rqos( eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS );
        rqos.override_from_json( participant->settings().nested( "device", "list-parameters-request" ) );
        _list_params_reader->run( rqos );
    }
    if( auto topic = topics::ros2::list_parameters_response_msg::create_topic(
            participant,
            response_root + topics::ros2::LIST_PARAMETERS_NAME + topics::ros2::RESPONSE_SUFFIX ) )
    {
        _list_params_writer = std::make_shared< dds_topic_writer >( topic, publisher );
        dds_topic_writer::qos wqos( eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS );
        wqos.history().depth = 10;  // default is 1
        _list_params_writer->override_qos_from_json( wqos,
            participant->settings().nested( "device", "list-parameters-response" ) );
        _list_params_writer->run( wqos );
    }
    if( auto topic = topics::ros2::describe_parameters_request_msg::create_topic(
            participant,
            request_root + topics::ros2::DESCRIBE_PARAMETERS_NAME + topics::ros2::REQUEST_SUFFIX ) )
    {
        _describe_params_reader = std::make_shared< dds_topic_reader >( topic, subscriber );
        _describe_params_reader->on_data_available( [&] { on_describe_params_request(); } );
        dds_topic_reader::qos rqos( eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS );
        rqos.override_from_json( participant->settings().nested( "device", "describe-parameters-request" ) );
        _describe_params_reader->run( rqos );
    }
    if( auto topic = topics::ros2::describe_parameters_response_msg::create_topic(
            participant,
            response_root + topics::ros2::DESCRIBE_PARAMETERS_NAME + topics::ros2::RESPONSE_SUFFIX ) )
    {
        _describe_params_writer = std::make_shared< dds_topic_writer >( topic, publisher );
        dds_topic_writer::qos wqos( eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS );
        wqos.history().depth = 10;  // default is 1
        _describe_params_writer->override_qos_from_json( wqos,
            participant->settings().nested( "device", "describe-parameters-response" ) );
        _describe_params_writer->run( wqos );
    }
    _parameter_events_writer = parameter_events_writer;
}


lrs_device_controller::~lrs_device_controller()
{
    LOG_DEBUG( "LRS device manager for device: " << _device_sn << " deleted" );
}


bool lrs_device_controller::is_recovery() const
{
    auto update_device = rs2::update_device( _rs_dev );
    return update_device;
}


void lrs_device_controller::fill_ros2_node_entities( realdds::topics::ros2::node_entities_info & node ) const
{
    node.add_writer( _get_params_writer->guid() );
    node.add_writer( _set_params_writer->guid() );
    node.add_writer( _list_params_writer->guid() );
    node.add_writer( _describe_params_writer->guid() );
    node.add_reader( _get_params_reader->guid() );
    node.add_reader( _set_params_reader->guid() );
    node.add_reader( _list_params_reader->guid() );
    node.add_reader( _describe_params_reader->guid() );
}


bool lrs_device_controller::on_open_streams( json const & control, json & reply )
{
    // Note this function is a response to "open-streams" so does not actually start streaming.
    // It simply sets and locks in which streams should be open when streaming starts.
    // This effectively lets one control _specifically_ which streams should be streamable, and nothing else: if left
    // out, a sensor is reset back to its default state using implicit stream selection.
    // (For example, the 'Stereo Module' sensor controls Depth, IR1, IR2: but turning on all 3 has performance
    // implications and may not be desirable. So you can open only Depth and IR1/2 will stay inactive...)
    if( control.nested( topics::control::open_streams::key::reset ).default_value( false ) )
        _bridge.reset();

    json msg_profiles;
    control.nested( topics::control::open_streams::key::stream_profiles ).get_ex( msg_profiles ); // Might be an empty list
    for( auto const & name2profile : msg_profiles.items() )
    {
        std::string const & stream_name = name2profile.key();
        auto name2server = _stream_name_to_server.find( stream_name );
        if( name2server == _stream_name_to_server.end() )
            throw std::runtime_error( "invalid stream name '" + stream_name + "'" );
        auto server = name2server->second;

        auto requested_profile
            = create_dds_stream_profile( server->type_string(), name2profile.value() );
        auto profile = find_profile( server, requested_profile );
        if( ! profile )
            throw std::runtime_error( "invalid profile " + requested_profile->to_string() + " for stream '"
                                      + stream_name + "'" );

        _bridge.open( profile );
    }

    // We're here so all the profiles were acceptable; lock them in -- with no implicit profiles!
    if( control.nested( topics::control::open_streams::key::commit ).default_value( true ) )
        _bridge.commit();

    // We don't touch the reply - it's already filled in for us
    return true;
}


void lrs_device_controller::publish_frame_metadata( const rs2::frame & f, realdds::dds_time const & timestamp )
{
    if( ! _dds_device_server->has_metadata_readers() )
        return;

    json md_header = json::object( {
        { topics::metadata::header::key::frame_number, f.get_frame_number() },               // communicated; up to client to pick up
        { topics::metadata::header::key::timestamp, timestamp.to_ns() },                     // syncer key: needs to match the image timestamp, bit-for-bit!
        { topics::metadata::header::key::timestamp_domain, f.get_frame_timestamp_domain() }  // needed if we're dealing with different domains!
    } );
    if( f.is< rs2::depth_frame >() )
        md_header[topics::metadata::header::key::depth_units] = f.as< rs2::depth_frame >().get_units();

    json metadata = json::object();
    for( size_t i = 0; i < static_cast< size_t >( RS2_FRAME_METADATA_COUNT ); ++i )
    {
        rs2_frame_metadata_value val = static_cast< rs2_frame_metadata_value >( i );
        if( f.supports_frame_metadata( val ) )
            metadata[rs2_frame_metadata_to_string( val )] = f.get_frame_metadata( val );
    }

    json md_msg = json::object( {
        { topics::metadata::key::stream_name, stream_name_from_rs2( f.get_profile() ) },
        { topics::metadata::key::header, std::move( md_header ) },
        { topics::metadata::key::metadata, std::move( metadata ) },
    } );
    _dds_device_server->publish_metadata( std::move( md_msg ) );
}


std::vector< rs2::stream_profile >
lrs_device_controller::get_rs2_profiles( realdds::dds_stream_profiles const & dds_profiles ) const
{
    std::vector< rs2::stream_profile > rs_profiles;
    for( auto & dds_profile : dds_profiles )
    {
        std::string stream_name = dds_profile->stream()->name();
        std::string sensor_name = dds_profile->stream()->sensor_name();

        auto it = _rs_sensors.find( sensor_name );
        if( it == _rs_sensors.end() )
        {
            LOG_ERROR( "invalid sensor name '" << sensor_name << "'" );
            continue;
        }
        auto & sensor = it->second;
        auto const sensor_stream_profiles = sensor.get_stream_profiles();
        auto rs2_profile = get_required_profile( sensor, sensor_stream_profiles, stream_name, dds_profile );
        rs_profiles.push_back( rs2_profile );

        if( rs2_profile.stream_type() == RS2_STREAM_GYRO )
        {
            // When we start the Gyro, we want to start the Accelerometer, too!
            rs2::stream_profile accel_profile;
            assert( accel_profile.fps() == 0 );
            for( auto & sp : sensor_stream_profiles )
            {
                if( sp.stream_type() != RS2_STREAM_ACCEL )
                    continue;
                if( sp.format() != rs2_profile.format() )
                    continue;
                if( sp.fps() > accel_profile.fps() )
                    accel_profile = sp;
            }
            if( accel_profile )
            {
                LOG_DEBUG( "adding accel profile @ " << accel_profile.fps() << " FPS" );
                rs_profiles.push_back( accel_profile );
            }
        }
    }
    return rs_profiles;
}


void lrs_device_controller::set_option( const std::shared_ptr< realdds::dds_option > & option, json const & new_value )
{
    auto stream = option->stream();
    if( ! stream )
        // TODO device option? not implemented yet
        throw std::runtime_error( "device option not implemented" );
    auto it = _stream_name_to_server.find( stream->name() );
    if( it == _stream_name_to_server.end() )
        throw std::runtime_error( "no stream '" + stream->name() + "' in device" );
    auto server = it->second;
    auto & sensor = _rs_sensors[server->sensor_name()];
    if( auto roi_option = std::dynamic_pointer_cast< realdds::dds_rect_option >( option ) )
    {
        // ROI has its own API in librealsense
        if( auto roi_sensor = rs2::roi_sensor( sensor ) )
            roi_sensor.set_region_of_interest( roi_from_json( roi_option->get_value() ) );
        else
            throw std::runtime_error( "rect option in sensor that has no ROI" );
    }
    else
    {
        // The librealsense API uses floats, so we need to convert from non-floats
        float float_value;
        switch( new_value.type() )
        {
        case json::value_t::number_float:
        case json::value_t::number_integer:
        case json::value_t::number_unsigned:
            float_value = new_value;
            break;
        
        case json::value_t::boolean:
            float_value = new_value.get< bool >();
            break;

        case json::value_t::string:
            // Only way is for this to be an enum...
            if( auto e = std::dynamic_pointer_cast< realdds::dds_enum_option >( option ) )
            {
                auto const & choices = e->get_choices();
                auto it = std::find( choices.begin(), choices.end(), new_value.string_ref() );
                if( it == choices.end() )
                    throw std::runtime_error( rsutils::string::from() << "not a valid enum value: " << new_value );
                float_value = float( it - choices.begin() );
                break;
            }
            // fall thru
        default:
            throw std::runtime_error( rsutils::string::from() << "unsupported value: " << new_value );
        }

        sensor.set_option( option_name_to_id( option->get_name() ), float_value );
    }
}


json lrs_device_controller::query_option( const std::shared_ptr< realdds::dds_option > & option )
{
    auto stream = option->stream();
    if( ! stream )
        // TODO device option? not implemented yet
        throw std::runtime_error( "device option not implemented" );
    auto it = _stream_name_to_server.find( stream->name() );
    if( it == _stream_name_to_server.end() )
        throw std::runtime_error( "no stream '" + stream->name() + "' in device" );
    auto server = it->second;
    auto & sensor = _rs_sensors[server->sensor_name()];
    try
    {
        if( auto roi_option = std::dynamic_pointer_cast< realdds::dds_rect_option >( option ) )
        {
            if( auto roi_sensor = rs2::roi_sensor( sensor ) )
                return json_from_roi( roi_sensor.get_region_of_interest() );
            else
                throw std::runtime_error( "rect option in sensor that has no ROI" );
        }
        return sensor.get_option( option_name_to_id( option->get_name() ) );
    }
    catch( rs2::invalid_value_error const & )
    {
        // It's possible for an option to no longer be supported (temperates do this) because we stopped streaming...
        if( option->is_optional() )
            return rsutils::null_json;
        throw;
    }
}


void lrs_device_controller::override_default_profiles( const std::map< std::string, realdds::dds_stream_profiles > & stream_name_to_profiles,
                                                       std::map< std::string, size_t > & stream_name_to_default_profile ) const
{
    // Default resolution for RealSense modules, set according to system SW architect definitions
    std::string const product_line = _rs_dev.get_info( RS2_CAMERA_INFO_PRODUCT_LINE );
    if( product_line == "D400" )
    {
        static const std::string RS405_PID( "0B5B", 4 );
        static const std::string RS410_PID( "0AD2", 4 );
        static const std::string RS415_PID( "0AD3", 4 );

        std::string product_id = _rs_dev.get_info( RS2_CAMERA_INFO_PRODUCT_ID );

        // For best image quality global shutter should use 848x480 resolution, rolling shutter 1280x720
        uint16_t fps = 30;
        uint16_t width = 848;
        uint16_t height = 480;
        if( product_id == RS415_PID || product_id == RS410_PID )
        {
            width = 1280;
            height = 720;
        }
        stream_name_to_default_profile["Depth"] = get_index_of_profile( stream_name_to_profiles.at( "Depth" ),
            realdds::dds_video_stream_profile( fps, realdds::dds_video_encoding::from_rs2( RS2_FORMAT_Z16 ), width, height ) );
        stream_name_to_default_profile["Infrared_1"] = get_index_of_profile( stream_name_to_profiles.at( "Infrared_1" ),
            realdds::dds_video_stream_profile( fps, realdds::dds_video_encoding::from_rs2( RS2_FORMAT_Y8 ), width, height ) );
        stream_name_to_default_profile["Infrared_2"] = get_index_of_profile( stream_name_to_profiles.at( "Infrared_2" ),
            realdds::dds_video_stream_profile( fps, realdds::dds_video_encoding::from_rs2( RS2_FORMAT_Y8 ), width, height ) );

        width = 1280;
        height = 720;
        if( product_id == RS405_PID )
        {
            width = 848;
            height = 480;
        }
        stream_name_to_default_profile["Color"] = get_index_of_profile( stream_name_to_profiles.at( "Color" ),
            realdds::dds_video_stream_profile( fps, realdds::dds_video_encoding::from_rs2( RS2_FORMAT_YUYV ), width, height ) );
    }
}


size_t lrs_device_controller::get_index_of_profile( const realdds::dds_stream_profiles & profiles,
                                                    const realdds::dds_video_stream_profile & profile ) const
{
    for( size_t i = 0; i < profiles.size(); ++i )
    {
        auto dds_vp = std::dynamic_pointer_cast< dds_video_stream_profile >( profiles[i] );
        if( ! dds_vp )
        {
            LOG_ERROR( "Profile is not a video profile" << profiles[i] );
            continue;
        }
        if( dds_vp->frequency() == profile.frequency()
            && dds_vp->encoding() == profile.encoding()
            && dds_vp->width()  == profile.width()
            && dds_vp->height() == profile.height() )
            return i;
    }

    return 0;
}


size_t lrs_device_controller::get_index_of_profile( const realdds::dds_stream_profiles & profiles,
                                                    const realdds::dds_motion_stream_profile & profile ) const
{
    for( size_t i = 0; i < profiles.size(); ++i )
    {
        if( profiles[i]->frequency() == profile.frequency() )
            return i;
    }

    return 0;
}


bool lrs_device_controller::on_control( std::string const & id, json const & control, json & reply )
{
    static std::map< std::string, bool ( lrs_device_controller::* )( json const &, json & ) > const control_handlers{
        { realdds::topics::control::hw_reset::id, &lrs_device_controller::on_hardware_reset },
        { realdds::topics::control::open_streams::id, &lrs_device_controller::on_open_streams },
        { realdds::topics::control::hwm::id, &lrs_device_controller::on_hwm },
        { realdds::topics::control::dfu_start::id, &lrs_device_controller::on_dfu_start },
        { realdds::topics::control::dfu_apply::id, &lrs_device_controller::on_dfu_apply },
    };
    auto it = control_handlers.find( id );
    if( it == control_handlers.end() )
        return false;

    return (this->*(it->second))( control, reply );
}


bool lrs_device_controller::on_hardware_reset( json const & control, json & reply )
{
    _dds_device_server->broadcast_disconnect();
    _rs_dev.hardware_reset();
    return true;
}


bool lrs_device_controller::on_hwm( json const & control, json & reply )
{
    rs2::debug_protocol dp( _rs_dev );
    if( ! dp )
        throw std::runtime_error( "device does not have a debug protocol implemented" );

    rsutils::string::hexarray data;

    uint32_t opcode;
    if( control.nested( topics::control::hwm::key::opcode ).get_ex( opcode ) )
    {
        // In the presence of 'opcode', we're asked to build the command using optional parameters
        uint32_t param1 = 0, param2 = 0, param3 = 0, param4 = 0;
        control.nested( topics::control::hwm::key::param1 ).get_ex( param1 );
        control.nested( topics::control::hwm::key::param2 ).get_ex( param2 );
        control.nested( topics::control::hwm::key::param3 ).get_ex( param3 );
        control.nested( topics::control::hwm::key::param4 ).get_ex( param4 );

        control.nested( topics::control::hwm::key::data ).get_ex( data );  // optional

        // Build the HWM command
        data = dp.build_command( opcode, param1, param2, param3, param4, data.get_bytes() );

        // And, if told to not actually run it, we return the HWM command
        if( control.nested( topics::control::hwm::key::build_command ).default_value( false ) )
        {
            reply[topics::reply::hwm::key::data] = data;
            return true;
        }
    }
    else
    {
        if( ! control.nested( topics::control::hwm::key::data ).get_ex( data ) )
            throw std::runtime_error( "no 'data' in HWM control" );
    }

    data = dp.send_and_receive_raw_data( data.get_bytes() );
    reply[topics::reply::hwm::key::data] = data;
    return true;
}


struct lrs_device_controller::dfu_support
{
    rs2::device rsdev;
    std::string uid;
    std::weak_ptr< dds_device_server > server;
    std::weak_ptr< lrs_device_controller > controller;
    std::shared_ptr< realdds::dds_topic_reader_thread > reader;
    std::shared_ptr< realdds::topics::blob_msg > image;
    realdds::dds_guid_prefix initiator;
    size_t image_size;
    uint32_t image_crc;

    std::string debug_name() const
    {
        if( auto dev = server.lock() )
            return rsutils::string::from() << "[" << dev->debug_name() << "] ";
        return {};
    }

    // reset to a non-DFU state
    void reset( char const * error = nullptr )
    {
        if( reader )
            reader->stop();
        if( auto c = controller.lock() )
        {
            if( error )
                LOG_ERROR( debug_name() << "DFU " << error );
            c->_dfu.reset();  // no longer in DFU state
        }
    }
};


bool lrs_device_controller::on_dfu_start( rsutils::json const & control, rsutils::json & reply )
{
    if( _dfu )
        throw std::runtime_error( "DFU already in progress" );

    // The FW-update-ID is used to recognize the device, even in recovery mode (serial-number is not enough)
    if( ! _rs_dev.supports( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID ) )
        throw std::runtime_error( "device does not support fw-update-id" );

    // Either updatable (regular device) or update_device (recovery) implement the check_fw_compatibility API, from
    // which we can proceed
    if( ! rs2::updatable( _rs_dev ) && ! rs2::update_device( _rs_dev ) )
        throw std::runtime_error( "device is not in an updatable state" );

    _dfu = std::make_shared< dfu_support >();
    _dfu->server = _dds_device_server;
    _dfu->controller = shared_from_this();
    _dfu->rsdev = _rs_dev;
    _dfu->initiator
        = realdds::guid_from_string( reply[realdds::topics::reply::key::sample][0].string_ref() ).guidPrefix;
    _dfu->uid = _rs_dev.get_info( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID );
    control.at( realdds::topics::control::dfu_start::key::size ).get_to( _dfu->image_size );  // throws
    control.at( realdds::topics::control::dfu_start::key::crc ).get_to( _dfu->image_crc );  // throws

    // Open a DFU topic and wait for the image on another thread
    auto topic = topics::blob_msg::create_topic( _dds_device_server->participant(),
                                                 _dds_device_server->topic_root() + realdds::topics::DFU_TOPIC_NAME );

    _dfu->reader = std::make_shared< dds_topic_reader_thread >( topic, _dds_device_server->subscriber() );
    _dfu->reader->on_data_available(
        [weak_dfu = std::weak_ptr< dfu_support >( _dfu )]
        {
            topics::blob_msg blob;
            realdds::dds_sample sample;
            while( auto dfu = weak_dfu.lock() )
            {
                if( ! topics::blob_msg::take_next( *dfu->reader, &blob, &sample ) )
                    break;
                if( ! blob.is_valid() )
                    continue;
                auto const & sender = sample.sample_identity.writer_guid().guidPrefix;
                if( sender != dfu->initiator )
                {
                    LOG_ERROR( dfu->debug_name()
                               << "DFU image received from " << realdds::print_raw_guid_prefix( sender ) << " != "
                               << realdds::print_raw_guid_prefix( dfu->initiator ) << " that started the DFU" );
                }
                else if( dfu->image )
                {
                    LOG_ERROR( dfu->debug_name() << "More than one DFU image received!" );
                }
                else
                {
                    LOG_INFO( dfu->debug_name() << "DFU image received" );

                    // Build a reply
                    rsutils::json j = rsutils::json::object( {
                        { realdds::topics::notification::key::id, realdds::topics::notification::dfu_ready::id },
                    } );

                    try
                    {
                        // Check the image
                        size_t const n_bytes = blob.data().size();
                        if( n_bytes != dfu->image_size )
                            throw std::runtime_error( rsutils::string::from()
                                                      << "image size (" << n_bytes << ") does not match expected ("
                                                      << dfu->image_size << ")" );

                        auto const crc = rsutils::number::calc_crc32( blob.data().data(), blob.data().size() );
                        if( crc != dfu->image_crc )
                            throw std::runtime_error( rsutils::string::from()
                                                      << "image CRC (" << rsutils::string::hexdump( crc )
                                                      << ") does not match expected ("
                                                      << rsutils::string::hexdump( dfu->image_crc ) << ")" );

                        rs2_error * e = nullptr;
                        bool is_compatible = rs2_check_firmware_compatibility( dfu->rsdev.get().get(),
                                                                               blob.data().data(),
                                                                               (int)blob.data().size(),
                                                                               &e );
                        rs2::error::handle( e );

                        if( ! is_compatible )
                            throw std::runtime_error( "image is incompatible" );

                        // Keep it until we get a 'dfu-apply'
                        dfu->image = std::make_shared< topics::blob_msg >( std::move( blob ) );
                    }
                    catch( std::exception const & e )
                    {
                        j[realdds::topics::reply::key::status] = "error";
                        j[realdds::topics::reply::key::explanation] = e.what();
                        LOG_ERROR( dfu->debug_name() << "DFU image check failed: " << e.what() << "; exiting DFU state" );
                        dfu->reset();
                    }

                    if( auto server = dfu->server.lock() )
                        server->publish_notification( std::move( j ) );
                }
            }
        } );

    dds_topic_reader::qos rqos( eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS );
    rqos.override_from_json( _dds_device_server->participant()->settings().nested( "device", "dfu" ) );
    _dfu->reader->run( rqos );

    // Start a thread that will cancel if we don't receive an image in time, or if the process somehow takes too long
    std::thread(
        [weak_dfu = std::weak_ptr< dfu_support >( _dfu )]
        {
            std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
            if( auto dfu = weak_dfu.lock() )
            {
                if( ! dfu->image )
                {
                    dfu->reset( "timed out waiting for image; resetting" );
                    if( auto server = dfu->server.lock() )
                    {
                        rsutils::json j = rsutils::json::object(
                            { { realdds::topics::notification::key::id, realdds::topics::notification::dfu_ready::id },
                              { realdds::topics::reply::key::status, "error" },
                              { realdds::topics::reply::key::explanation,
                                "timed out waiting for an image; resetting" } } );
                        server->publish_notification( std::move( j ) );
                    }
                    return;
                }
            }
            else
                return;

            // We have an image: sleep some more and cancel the DFU after a while if no dfu-apply is received
            std::this_thread::sleep_for( std::chrono::seconds( 10 ) );
            if( auto dfu = weak_dfu.lock() )
            {
                dfu->reset( "timed out waiting for apply; resetting" );
                if( auto server = dfu->server.lock() )
                {
                    rsutils::json j = rsutils::json::object(
                        { { realdds::topics::notification::key::id, realdds::topics::notification::dfu_ready::id },
                          { realdds::topics::reply::key::status, "error" },
                          { realdds::topics::reply::key::explanation, "timed out; resetting" } } );
                    server->publish_notification( std::move( j ) );
                }
            }
        } )
        .detach();

    return true;  // we handled it
}


bool lrs_device_controller::on_dfu_apply( rsutils::json const & control, rsutils::json & reply )
{
    if( _dfu )
    {
        auto const sender
            = realdds::guid_from_string( reply[realdds::topics::reply::key::sample][0].string_ref() ).guidPrefix;
        if( sender != _dfu->initiator )
            throw std::runtime_error( "only the DFU initiator can apply/cancel" );
    }

    if( control.nested( realdds::topics::control::dfu_apply::key::cancel ).default_value( false ) )
    {
        if( _dfu )
            LOG_DEBUG( _dfu->debug_name() << "DFU cancelled" );
        _dfu.reset();
        return true;  // handled
    }

    if( ! _dfu )
        throw std::runtime_error( "DFU not started" );
    if( ! _dfu->reader )
        throw std::runtime_error( "dfu-apply already received" );

    // Keep the DFU alive once apply starts, even if reset somewhere else - but the reader is no longer needed
    auto dfu = _dfu;
    _dfu->reader.reset();

    if( ! _dfu->image )
        throw std::runtime_error( "no image received" );

    // We want to return a reply right away; we already have an image in hand, so all that needs to happen is to apply
    // it in the background and return a reply when done
    std::thread(
        [dfu]
        {
            auto on_fail = [dfu]( char const * what )
            {
                LOG_ERROR( "Failed to apply DFU image: " << what );
                if( auto server = dfu->server.lock() )
                {
                    rsutils::json j
                        = json::object( { { realdds::topics::reply::key::id, realdds::topics::reply::dfu_apply::id },
                                          { realdds::topics::reply::key::status, "error" } } );
                    if( what )
                        j[realdds::topics::reply::key::explanation] = what;
                    server->publish_notification( std::move( j ) );
                }
            };

            try
            {
                // We already checked FW compatibility; to actually do a signed update, we switch to recovery mode (we need
                // an update_device to do a signed update - but to get one, we need to be in recovery mode):
                rs2::update_device update_device = dfu->rsdev;
                if( ! update_device )
                {
                    LOG_INFO( "Switching device (update ID " << dfu->uid << ") to recovery mode" );
                    if( auto server = dfu->server.lock() )
                        server->broadcast_disconnect( { 3, 0 } );  // timeout for ACKs
                    rs2::updatable( dfu->rsdev ).enter_update_state();
                    // NOTE: the device will go offline; the adapter will see the device disappear, and so will the controller/server!

                    // We need a context but cannot get one from the device... so we can just create a new one:
                    rsutils::json settings;
                    settings["dds"] = false;  // don't want DDS devices
                    rs2::context context( settings.dump() );

                    // Wait for the recovery device
                    rsutils::time::timer timeout( std::chrono::seconds( 60 ) );
                    while( dfu->controller.lock() )
                    {
                        try
                        {
                            auto devs = context.query_devices( RS2_PRODUCT_LINE_ANY_INTEL );
                            for( uint32_t j = 0; j < devs.size(); j++ )
                            {
                                auto d = devs[j];
                                if( d.is< rs2::update_device >() && d.supports( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID )
                                    && dfu->uid == d.get_info( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID ) )
                                {
                                    LOG_DEBUG( "... found DFU recovery device (update ID " << dfu->uid << ")" );
                                    update_device = d;
                                    break;
                                }
                            }
                        }
                        catch( std::exception const & e )
                        {
                            LOG_DEBUG( "... error looking for DFU device: " << e.what() );
                        }
                        catch( ... )
                        {
                            LOG_DEBUG( "... error looking for DFU device" );
                        }

                        if( update_device )
                            break;

                        if( timeout.has_expired() )
                            throw std::runtime_error( "failed to find recovery device" );

                        // Wait a little and retry
                        std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
                    }
                }
                if( update_device )
                {
                    if( auto controller = dfu->controller.lock() )
                    {
                        LOG_INFO( "Updating - DO NOT SHUT DOWN" );
                        int progress = 0;
                        update_device.update(
                            dfu->image->data(),
                            [dfu, progress = int( 0 )]( float percent ) mutable
                            {
                                int new_progress = int( percent * 10 );  // update only every 10%
                                if( new_progress != progress )
                                {
                                    progress = new_progress;
                                    if( auto server = dfu->server.lock() )
                                        server->publish_notification(
                                            json::object( { { realdds::topics::notification::key::id,
                                                              realdds::topics::notification::dfu_apply::id },
                                                            { realdds::topics::notification::dfu_apply::key::progress,
                                                              percent } } ) );
                                }
                            } );
                        // NOTE: the device will go offline again, and should come up normally
                        LOG_INFO( "DFU done" );
                    }
                }
            }
            catch( std::exception const & e )
            {
                on_fail( e.what() );
            }
            catch( ... )
            {
                on_fail( nullptr );
            }

            // Whether successful or not, we're done with the DFU
            dfu->reset();
        } )
        .detach();

    return true;  // handled
}


bool lrs_device_controller::update_stream_trinsics( json * p_changes )
{
    // Returns true if any changes are detected
    // If p_changes is not null, it should point to a json object which will be populated with stream-name:changes
    // mappings.
    if( p_changes && ! p_changes->is_object() )
        throw std::runtime_error( "expecting a json object" );

    bool have_changes = false;
    std::map< std::string, std::set< realdds::video_intrinsics > > stream_name_to_video_intrinsics;

    // Iterate over all profiles of all sensors and build appropriate dds_stream_servers
    for( auto & name_sensor : _rs_sensors )
    {
        std::string const & sensor_name = name_sensor.first;
        auto const & sensor = name_sensor.second;

        auto const stream_profiles = sensor.get_stream_profiles();
        std::for_each( stream_profiles.begin(),
                       stream_profiles.end(),
                       [&]( const rs2::stream_profile & sp )
                       {
                           std::string const stream_name = stream_name_from_rs2( sp );
                           auto server_it = _stream_name_to_server.find( stream_name );
                           if( server_it == _stream_name_to_server.end() )
                           {
                               LOG_DEBUG( "could not find server '" << stream_name << "'" );
                               return;
                           }
                           auto const & server = server_it->second;

                           // Create appropriate realdds::profile for each sensor profile and map to a stream
                           if( auto const vsp = rs2::video_stream_profile( sp ) )
                           {
                               try
                               {
                                   auto intr = to_realdds( vsp.get_intrinsics() );
                                   stream_name_to_video_intrinsics[stream_name].insert( intr );
                               }
                               catch( ... )
                               {
                               }  // Some profiles don't have intrinsics
                           }
                           else if( auto const msp = rs2::motion_stream_profile( sp ) )
                           {
                               auto motion_server = std::dynamic_pointer_cast< dds_motion_stream_server >( server );
                               auto const intr = to_realdds( msp.get_motion_intrinsics() );
                               if( RS2_STREAM_ACCEL == msp.stream_type() )
                               {
                                   if( motion_server->get_accel_intrinsics() != intr )
                                   {
                                       have_changes = true;
                                       if( p_changes )
                                       {
                                           ( *p_changes )
                                               [stream_name]
                                               [realdds::topics::notification::calibration_changed::key::intrinsics]
                                               [realdds::topics::notification::stream_options::intrinsics::key::accel]
                                               = intr.to_json();
                                       }
                                       motion_server->set_accel_intrinsics( intr );
                                   }
                               }
                               else if( motion_server->get_gyro_intrinsics() != intr )
                               {
                                   have_changes = true;
                                   if( p_changes )
                                   {
                                       ( *p_changes )
                                           [stream_name]
                                           [realdds::topics::notification::calibration_changed::key::intrinsics]
                                           [realdds::topics::notification::stream_options::intrinsics::key::gyro]
                                           = intr.to_json();
                                   }
                                   motion_server->set_gyro_intrinsics( intr );
                               }
                           }
                       } );
    }

    for( auto & name_intr : stream_name_to_video_intrinsics )
    {
        auto const & stream_name = name_intr.first;
        auto const & server = _stream_name_to_server[stream_name];
        if( auto video_server = std::dynamic_pointer_cast< dds_video_stream_server >( server ) )
        {
            auto & intr = name_intr.second;
            if( video_server->get_intrinsics() != intr )
            {
                if( intr.size() != video_server->get_intrinsics().size() )
                {
                    LOG_ERROR( "unexpected change in number of intrinsics for '" << stream_name << "'" );
                    continue;
                }
                have_changes = true;
                if( p_changes )
                {
                    auto & j_intrinsics
                        = ( *p_changes )[stream_name]
                                        [realdds::topics::notification::calibration_changed::key::intrinsics];
                    if( 1 == intr.size() )
                    {
                        // Use an object with a single intrinsic
                        j_intrinsics = intr.begin()->to_json();
                    }
                    else
                    {
                        // Multiple intrinsics are available
                        j_intrinsics = json::array();
                        for( auto const & i : intr )
                            j_intrinsics.push_back( i.to_json() );
                    }
                }
                video_server->set_intrinsics( std::move( name_intr.second ) );
            }
        }
    }

    return have_changes;
}


void lrs_device_controller::on_get_params_request()
{
    topics::ros2::get_parameters_request_msg control;
    dds_sample sample;
    while( control.take_next( *_get_params_reader, &sample ) )
    {
        if( ! control.is_valid() )
            continue;

        auto sample_j = json::array( {
            rsutils::string::from( realdds::print_raw_guid( sample.sample_identity.writer_guid() ) ),
            sample.sample_identity.sequence_number().to64long(),
        } );

        LOG_DEBUG( "[" << _dds_device_server->debug_name() << "] <----- get_parameters" );
        topics::ros2::get_parameters_response_msg response;
        for( auto & name : control.names() )
        {
            topics::ros2::get_parameters_response_msg::value_type value;  // NOT_SET
            try
            {
                auto const sep = name.find( '/' );
                std::string stream_name;  // empty, i.e. device option
                std::string option_name = name;
                if( sep != std::string::npos )
                {
                    stream_name = name.substr( 0, sep );
                    auto stream_it = _stream_name_to_server.find( stream_name );
                    if( stream_it == _stream_name_to_server.end() )
                        throw std::runtime_error( "invalid stream name" );

                    option_name = name.substr( sep + 1 );
                    if( option_name == "profile" )
                    {
                        // return the current profile as a JSON array string
                        auto profile = _bridge.get_profile( stream_it->second );
                        value.string_value( profile->to_json().dump() );
                        value.type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_STRING );
                    }
                }
                if( value.type() == rcl_interfaces::msg::ParameterType_Constants::PARAMETER_NOT_SET )
                {
                    auto option = _dds_device_server->find_option( option_name, stream_name );
                    if( ! option )
                        throw std::runtime_error( "option not found" );
                    if( option->is_valid() )  // Otherwise leave the value as unset
                    {
                        auto & j = option->get_value();
                        switch( j.type() )
                        {
                        case json::value_t::string:
                            value.string_value( j );
                            value.type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_STRING );
                            break;
                        case json::value_t::number_float:
                            value.double_value( j );
                            value.type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_DOUBLE );
                            break;
                        case json::value_t::number_integer:
                        case json::value_t::number_unsigned:
                            value.integer_value( j );
                            value.type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_INTEGER );
                            break;
                        case json::value_t::boolean:
                            value.bool_value( j );
                            value.type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_BOOL );
                            break;
                        default:
                            // Everything else, we'll communicate but as a JSON string...
                            value.string_value( j.dump() );
                            value.type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_STRING );
                            break;
                        }
                    }
                }
                response.add( value );
            }
            catch( std::exception const & e )
            {
                LOG_ERROR( "[" << _dds_device_server->debug_name() << "][" << name << "] " << e.what() );
                response.add( value );
            }
        }
        // Now send the response back
        try
        {
            response.respond_to( sample, *_get_params_writer );
        }
        catch( std::exception const & e )
        {
            LOG_ERROR( "[" << _dds_device_server->debug_name() << "] failed to send response: " << e.what() );
        }
    }
}


void lrs_device_controller::on_set_params_request()
{
    topics::ros2::set_parameters_request_msg control;
    dds_sample sample;
    while( control.take_next( *_set_params_reader, &sample ) )
    {
        if( ! control.is_valid() )
            continue;

        auto sample_j = json::array( {
            rsutils::string::from( realdds::print_raw_guid( sample.sample_identity.writer_guid() ) ),
            sample.sample_identity.sequence_number().to64long(),
        } );
        LOG_DEBUG( "[" << _dds_device_server->debug_name() << "] <----- set_parameters" );
        topics::ros2::set_parameters_response_msg response;
        for( auto & parameter : control.parameters() )
        {
            auto & name = parameter.name();
            auto & value = parameter.value();
            topics::ros2::set_parameters_response_msg::result_type result;  // failed; no reason
            try
            {
                auto const sep = name.find( '/' );
                std::string stream_name;  // empty, i.e. device option
                std::string option_name = name;
                if( sep != std::string::npos )
                {
                    stream_name = name.substr( 0, sep );
                    auto stream_it = _stream_name_to_server.find( stream_name );
                    if( stream_it == _stream_name_to_server.end() )
                        throw std::runtime_error( "invalid stream name" );

                    option_name = name.substr( sep + 1 );
                    if( option_name == "profile" )
                    {
                        if( value.type() != rcl_interfaces::msg::ParameterType_Constants::PARAMETER_STRING )
                            throw std::runtime_error( "invalid profile type" );

                        if( value.string_value().empty() )
                        {
                            // We need to provide a way to reset the profile, back to an implicit state
                            // Providing an empty profile will do this
                            // NOTE that it will do this for ALL streams!
                            // NOTE this will call the on_stream_profile_change() callback and send out notifications
                            _bridge.reset();
                        }
                        else
                        {
                            auto server = stream_it->second;
                            auto requested_profile = create_dds_stream_profile( server->type_string(),
                                                                                json::parse( value.string_value() ) );
                            auto profile = find_profile( server, requested_profile );
                            if( ! profile )
                                throw std::runtime_error( "invalid profile '" + requested_profile->to_string() + "'" );

                            LOG_DEBUG( "[" << _dds_device_server->debug_name() << "][" << name
                                           << "] = " << value.string_value() );

                            // NOTE this will call the on_stream_profile_change() callback and send out notifications
                            _bridge.open( profile );
                        }
                        result.successful( true );
                        response.add( result );
                        continue;
                    }
                }
                auto option = _dds_device_server->find_option( option_name, stream_name );
                if( ! option )
                    throw std::runtime_error( "option not found" );
                json jvalue;
                switch( value.type() )
                {
                case rcl_interfaces::msg::ParameterType_Constants::PARAMETER_STRING:
                    try
                    {
                        jvalue = json::parse( value.string_value() );
                    }
                    catch( ... )
                    {
                        jvalue = value.string_value();
                    }
                    break;
                case rcl_interfaces::msg::ParameterType_Constants::PARAMETER_BOOL:
                    jvalue = value.bool_value();
                    break;
                case rcl_interfaces::msg::ParameterType_Constants::PARAMETER_INTEGER:
                    jvalue = value.integer_value();
                    break;
                case rcl_interfaces::msg::ParameterType_Constants::PARAMETER_DOUBLE:
                    jvalue = value.double_value();
                    break;
                default:
                    // Everything else, we don't yet support
                    throw std::runtime_error( "unsupported value type " + std::to_string( int( value.type() ) ) );
                }
                LOG_DEBUG( "[" << _dds_device_server->debug_name() << "][" << name << "] = " << jvalue );
                set_option( option, jvalue );
                result.successful( true );
                response.add( result );
            }
            catch( std::exception const & e )
            {
                LOG_ERROR( "[" << _dds_device_server->debug_name() << "][" << name << "] " << e.what() );
                result.reason( e.what() );
                response.add( result );
            }
        }
        // Now send the response back
        try
        {
            response.respond_to( sample, *_set_params_writer );
        }
        catch( std::exception const & e )
        {
            LOG_ERROR( "[" << _dds_device_server->debug_name() << "] failed to send response: " << e.what() );
        }
    }
}


void lrs_device_controller::on_list_params_request()
{
    topics::ros2::list_parameters_request_msg control;
    dds_sample sample;
    while( control.take_next( *_list_params_reader, &sample ) )
    {
        if( ! control.is_valid() )
            continue;

        auto sample_j = json::array( {
            rsutils::string::from( realdds::print_raw_guid( sample.sample_identity.writer_guid() ) ),
            sample.sample_identity.sequence_number().to64long(),
        } );
        LOG_DEBUG( "[" << _dds_device_server->debug_name() << "] <----- list_parameters" << field::group() << control );
        topics::ros2::list_parameters_response_msg response;
        if( control.prefixes().empty() || control.prefixes().size() == 1 && control.prefixes().front() == "" )
        {
            // Include device options (with empty prefix)
            for( auto & option : _dds_device_server->options() )
                response.add( option->get_name() );
        }
        for( auto & stream_server : _dds_device_server->streams() )
        {
            // Stream options use the stream name as the prefix
            response.add( stream_server.first + "/profile" );
            if( control.prefixes().empty()
                || std::find( control.prefixes().begin(), control.prefixes().end(), stream_server.first )
                       != control.prefixes().end() )
            {
                for( auto & option : stream_server.second->options() )
                    response.add( stream_server.first + '/' + option->get_name() );
            }
        }
        // Now send the response back
        try
        {
            response.respond_to( sample, *_list_params_writer );
        }
        catch( std::exception const & e )
        {
            LOG_ERROR( "[" << _dds_device_server->debug_name() << "] failed to send response: " << e.what() );
        }
    }
}


void lrs_device_controller::on_describe_params_request()
{
    topics::ros2::describe_parameters_request_msg control;
    dds_sample sample;
    while( control.take_next( *_describe_params_reader, &sample ) )
    {
        if( ! control.is_valid() )
            continue;

        auto sample_j = json::array( {
            rsutils::string::from( realdds::print_raw_guid( sample.sample_identity.writer_guid() ) ),
            sample.sample_identity.sequence_number().to64long(),
        } );
        LOG_DEBUG( "[" << _dds_device_server->debug_name() << "] <----- describe_parameters" );
        topics::ros2::describe_parameters_response_msg response;
        for( auto & name : control.names() )
        {
            topics::ros2::describe_parameters_response_msg::descriptor_type desc;  // NOT_SET
            desc.name( name );
            try
            {
                auto const sep = name.find( '/' );
                std::string stream_name;  // empty, i.e. device option
                std::string option_name = name;
                if( sep != std::string::npos )
                {
                    stream_name = name.substr( 0, sep );
                    auto stream_it = _stream_name_to_server.find( stream_name );
                    if( stream_it == _stream_name_to_server.end() )
                        throw std::runtime_error( "invalid stream name" );

                    option_name = name.substr( sep + 1 );
                    if( option_name == "profile" )
                    {
                        desc.description( "the current stream profile" );
                        desc.type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_STRING );
                    }
                }
                if( desc.type() == rcl_interfaces::msg::ParameterType_Constants::PARAMETER_NOT_SET )
                {
                    auto option = _dds_device_server->find_option( option_name, stream_name );
                    if( ! option )
                        throw std::runtime_error( "option not found" );
                    desc.description( option->get_description() );
                    desc.read_only( option->is_read_only() );
                    if( option->is_valid() )  // Otherwise leave the type as unset
                    {
                        auto & j = option->get_value();
                        switch( j.type() )
                        {
                        case json::value_t::string:
                            desc.type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_STRING );
                            break;
                        case json::value_t::number_float:
                            desc.type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_DOUBLE );
                            break;
                        case json::value_t::number_integer:
                        case json::value_t::number_unsigned:
                            desc.type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_INTEGER );
                            break;
                        case json::value_t::boolean:
                            desc.type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_BOOL );
                            break;
                        default:
                            // Everything else, we'll communicate but as a JSON string...
                            desc.type( rcl_interfaces::msg::ParameterType_Constants::PARAMETER_STRING );
                            break;
                        }
                    }
                }
                response.add( desc );
            }
            catch( std::exception const & e )
            {
                LOG_ERROR( "[" << _dds_device_server->debug_name() << "][" << name << "] " << e.what() );
                //response.add( desc );
            }
        }
        // Now send the response back
        try
        {
            response.respond_to( sample, *_describe_params_writer );
        }
        catch( std::exception const & e )
        {
            LOG_ERROR( "[" << _dds_device_server->debug_name() << "] failed to send response: " << e.what() );
        }
    }
}


