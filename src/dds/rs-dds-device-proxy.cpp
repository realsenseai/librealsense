// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include "rs-dds-device-proxy.h"
#include "rs-dds-color-sensor-proxy.h"
#include "rs-dds-depth-sensor-proxy.h"
#include "rs-dds-motion-sensor-proxy.h"

#include <realdds/dds-device.h>
#include <realdds/dds-stream.h>
#include <realdds/dds-trinsics.h>
#include <realdds/dds-participant.h>
#include <realdds/dds-topic-writer.h>
#include <realdds/dds-utilities.h>

#include <realdds/topics/device-info-msg.h>
#include <realdds/topics/flexible-msg.h>
#include <realdds/topics/blob-msg.h>
#include <realdds/topics/dds-topic-names.h>

#include <src/stream.h>
#include <src/environment.h>

#include <rsutils/json.h>
#include <rsutils/string/hexarray.h>
#include <rsutils/string/from.h>
#include <rsutils/number/crc32.h>
#include <rsutils/time/timer.h>

#include <src/ds/d500/d500-auto-calibration.h>
#include <src/ds/d500/d500-debug-protocol-calibration-engine.h>
#include <src/ds/d400/d400-private.h>
using rsutils::json;


namespace librealsense {


static rs2_stream to_rs2_stream_type( std::string const & type_string )
{
    static const std::map< std::string, rs2_stream > type_to_rs2 = {
        { "depth", RS2_STREAM_DEPTH },
        { "color", RS2_STREAM_COLOR },
        { "ir", RS2_STREAM_INFRARED },
        { "motion", RS2_STREAM_MOTION },
        { "confidence", RS2_STREAM_CONFIDENCE },
    };
    auto it = type_to_rs2.find( type_string );
    if( it == type_to_rs2.end() )
        throw invalid_value_exception( "unknown stream type '" + type_string + "'" );

    return it->second;
}


rs2_distortion to_rs2_distortion( realdds::distortion_model model )
{
    switch( model )
    {
    case realdds::distortion_model::none: return RS2_DISTORTION_NONE;
    case realdds::distortion_model::brown: return RS2_DISTORTION_BROWN_CONRADY;
    case realdds::distortion_model::inverse_brown: return RS2_DISTORTION_INVERSE_BROWN_CONRADY;
    case realdds::distortion_model::modified_brown: return RS2_DISTORTION_MODIFIED_BROWN_CONRADY;
    default:
        throw invalid_value_exception( "unexpected realdds distortion model: " + std::to_string( (int)model ) );
    }
}


rs2_intrinsics to_rs2_intrinsics( const realdds::video_intrinsics & intrinsics )
{
    rs2_intrinsics intr;
    intr.width = intrinsics.width;
    intr.height = intrinsics.height;
    intr.ppx = intrinsics.principal_point.x;
    intr.ppy = intrinsics.principal_point.y;
    intr.fx = intrinsics.focal_length.x;
    intr.fy = intrinsics.focal_length.y;
    intr.model = to_rs2_distortion( intrinsics.distortion.model );
    memcpy( intr.coeffs, intrinsics.distortion.coeffs.data(), sizeof( intr.coeffs ) );
    return intr;
}


static rs2_video_stream to_rs2_video_stream( rs2_stream const stream_type,
                                             sid_index const & sidx,
                                             std::shared_ptr< realdds::dds_video_stream_profile > const & profile,
                                             const std::set< realdds::video_intrinsics > & intrinsics )
{
    rs2_video_stream prof;
    prof.type = stream_type;
    prof.index = sidx.index;
    prof.uid = sidx.sid;
    prof.width = profile->width();
    prof.height = profile->height();
    prof.fps = profile->frequency();
    prof.fmt = static_cast< rs2_format >( profile->encoding().to_rs2() );

    // Handle intrinsics
    auto intr = std::find_if( intrinsics.begin(),
                              intrinsics.end(),
                              [profile]( const realdds::video_intrinsics & intr )
                              { return profile->width() == intr.width && profile->height() == intr.height; } );
    if( intr != intrinsics.end() )  // Some profiles don't have intrinsics
    {
        prof.intrinsics = to_rs2_intrinsics( *intr );
    }

    return prof;
}


static rs2_motion_stream to_rs2_motion_stream( rs2_stream const stream_type,
                                               sid_index const & sidx,
                                               std::shared_ptr< realdds::dds_motion_stream_profile > const & profile,
                                               const realdds::motion_intrinsics & intrinsics )
{
    rs2_motion_stream prof;
    prof.type = stream_type;
    prof.index = sidx.index;
    prof.uid = sidx.sid;
    prof.fps = profile->frequency();
    prof.fmt = RS2_FORMAT_COMBINED_MOTION;

    memcpy( prof.intrinsics.data, intrinsics.data.data(), sizeof( prof.intrinsics.data ) );
    memcpy( prof.intrinsics.noise_variances,
            intrinsics.noise_variances.data(),
            sizeof( prof.intrinsics.noise_variances ) );
    memcpy( prof.intrinsics.bias_variances,
            intrinsics.bias_variances.data(),
            sizeof( prof.intrinsics.bias_variances ) );

    return prof;
}


static rs2_extrinsics to_rs2_extrinsics( const std::shared_ptr< realdds::extrinsics > & dds_extrinsics )
{
    rs2_extrinsics rs2_extr;

    memcpy( rs2_extr.rotation, dds_extrinsics->rotation.data(), sizeof( rs2_extr.rotation ) );
    memcpy( rs2_extr.translation, dds_extrinsics->translation.data(), sizeof( rs2_extr.translation ) );

    return rs2_extr;
}


dds_device_proxy::dds_device_proxy( std::shared_ptr< const device_info > const & dev_info,
                                    std::shared_ptr< realdds::dds_device > const & dev)
    : software_device( dev_info, true )
    , auto_calibrated_proxy()
    , ds_advanced_mode_base()
    , _dds_dev( dev )
{
    //LOG_DEBUG( "=====> dds-device-proxy " << this << " created on top of dds-device " << _dds_dev.get() );
    std::string suffix = dev->device_info().is_recovery() ? " Recovery" : "";
    register_info( RS2_CAMERA_INFO_NAME, dev->device_info().name() + suffix );
    register_info( RS2_CAMERA_INFO_PHYSICAL_PORT, dev->device_info().topic_root() );
    register_info( RS2_CAMERA_INFO_PRODUCT_ID, "DDS" );

    auto & j = dev->device_info().to_json();
    std::string str;
    if( j.nested( "serial" ).get_ex( str ) )
    {
        register_info( RS2_CAMERA_INFO_SERIAL_NUMBER, str );
        j.nested( "fw-update-id" ).get_ex( str );  // if fails, str will be the serial
        register_info( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID, str );
    }
    else if( j.nested( "fw-update-id" ).get_ex( str ) )
        register_info( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID, str );
    if( j.nested( "fw-version" ).get_ex( str ) )
        register_info( RS2_CAMERA_INFO_FIRMWARE_VERSION, str );
    if( j.nested( "product-line" ).get_ex( str ) )
        register_info( RS2_CAMERA_INFO_PRODUCT_LINE, str );
    if( j.nested( "imu-type" ).get_ex( str ) )
        register_info( RS2_CAMERA_INFO_IMU_TYPE, str );
    register_info( RS2_CAMERA_INFO_CAMERA_LOCKED, j.nested( "locked" ).default_value( true ) ? "YES" : "NO" );
    register_info(RS2_CAMERA_INFO_CONNECTION_TYPE, "DDS" );
    ds_advanced_mode_base::_enabled = j.nested( "advanced-mode" ).default_value( false );
    
    eth_config_device::init( static_cast< debug_interface * >( this ) ); // Call after registering device info

    // Assumes dds_device initialization finished
    struct sensor_info
    {
        std::shared_ptr< dds_sensor_proxy > proxy;
        int sensor_index = 0;
        rs2_stream type = RS2_STREAM_ANY;
        // dds_streams bear stream type and index information, we add it to a dds_sensor_proxy mapped by a newly generated
        // unique ID. After the sensor initialization we get all the "final" profiles from formats-converter with type and
        // index but without IDs. We need to find the dds_stream that each profile was created from so we create a map from
        // type and index to dds_stream ID and index, because the dds_sensor_proxy holds a map from sidx to dds_stream. We
        // need both the ID from that map key and the stream itself (for intrinsics information)
        std::map< sid_index, sid_index > type_and_index_to_dds_stream_sidx;
    };
    std::map< std::string, sensor_info > sensor_name_to_info;

    // Figure out the sensor types
    _dds_dev->foreach_stream(
        [&]( std::shared_ptr< realdds::dds_stream > const & stream )
        {
            auto & sensor = sensor_name_to_info[stream->sensor_name()];
            if( strcmp( stream->type_string(), "depth" ) == 0 
                || strcmp( stream->type_string(), "ir" ) == 0 )
            {
                // If there's depth or infrared, it is a depth sensor regardless of what else is in there
                // E.g., the D405 has a color stream in the depth sensor
                sensor.type = RS2_STREAM_DEPTH;
            }
            else if( RS2_STREAM_ANY == sensor.type )
            {
                if( strcmp( stream->type_string(), "color" ) == 0 )
                {
                    sensor.type = RS2_STREAM_COLOR;
                }
                else if( strcmp( stream->type_string(), "motion" ) == 0 )
                {
                    sensor.type = RS2_STREAM_MOTION;
                }
                else
                {
                    // Leave it as "ANY" - we'll create a generic sensor
                }
            }
        } );  // End foreach_stream lambda

    _dds_dev->foreach_stream(
        [&]( std::shared_ptr< realdds::dds_stream > const & stream )
        {
            auto & sensor_info = sensor_name_to_info[stream->sensor_name()];
            if( ! sensor_info.proxy )
            {
                // This is a new sensor we haven't seen yet
                sensor_info.proxy = create_sensor( stream->sensor_name(), sensor_info.type );
                sensor_info.sensor_index = add_sensor( sensor_info.proxy );
                assert( sensor_info.sensor_index == _software_sensors.size() );
                _software_sensors.push_back( sensor_info.proxy );
            }
            auto stream_type = to_rs2_stream_type( stream->type_string() );
            int index = get_unique_index_in_sensor( sensor_info.proxy, stream->name(), stream_type );
            sid_index sidx( environment::get_instance().generate_stream_id(), index);
            sid_index type_and_index( stream_type, index );
            _stream_name_to_librs_stream[stream->name()]
                = std::make_shared< librealsense::stream >( stream_type, sidx.index );
            sensor_info.proxy->add_dds_stream( sidx, stream );
            _stream_name_to_owning_sensor[stream->name()] = sensor_info.proxy;
            if( ! sensor_info.type_and_index_to_dds_stream_sidx.insert( { type_and_index, sidx } ).second )
                LOG_ERROR( "Failed to insert '" << stream->sensor_name() << "' " << type_and_index.to_string() << " "
                                                << get_string( sensor_info.type ) << " -> " << get_string( stream_type )
                                                << " " << sidx.to_string() << " '" << stream->name() << "' mapping" );
            //else
            //    LOG_DEBUG( "'" << stream->sensor_name() << "' " << type_and_index.to_string() << " "
            //                   << get_string( sensor_info.type ) << " -> " << get_string( stream_type ) << " "
            //                   << sidx.to_string() << " '" << stream->name() << "'" );

            software_sensor & sensor = get_software_sensor( sensor_info.sensor_index );
            auto video_stream = std::dynamic_pointer_cast< realdds::dds_video_stream >( stream );
            auto motion_stream = std::dynamic_pointer_cast< realdds::dds_motion_stream >( stream );
            auto & profiles = stream->profiles();
            auto const & default_profile = profiles[stream->default_profile_index()];
            for( auto & profile : profiles )
            {
                //LOG_DEBUG( "    " << profile->details_to_string() );
                try
                {
                    if( video_stream )
                    {
                        auto video_profile = std::static_pointer_cast< realdds::dds_video_stream_profile >( profile );
                        auto raw_stream_profile = sensor.add_video_stream(
                            to_rs2_video_stream( stream_type, sidx, video_profile, video_stream->get_intrinsics() ),
                            profile == default_profile, stream->name() );
                        _stream_name_to_profiles[stream->name()].push_back( raw_stream_profile );
                    }
                    else if( motion_stream )
                    {
                        auto motion_profile = std::static_pointer_cast< realdds::dds_motion_stream_profile >( profile );
                        auto raw_stream_profile = sensor.add_motion_stream(
                            to_rs2_motion_stream( stream_type, sidx, motion_profile, motion_stream->get_gyro_intrinsics() ),
                            profile == default_profile, stream->name() );
                        _stream_name_to_profiles[stream->name()].push_back( raw_stream_profile );
                    }
                }
                catch( std::exception const& e )
                {
                    LOG_ERROR( "Cannot add profile " << profile->details_to_string() << " to stream " << stream->name()
                                                     << ". " << e.what() );
                }
                // NOTE: the raw profile will be cloned and overriden by the format converter!
            }

            auto & options = stream->options();
            for( auto & option : options )
            {
                try
                {
                    sensor_info.proxy->add_option( option );
                }
                catch( std::exception const& e )
                {
                    LOG_ERROR( e.what() );
                }
            }
            // Add locally handled options. Do it only after all dds options are added to avoid duplicates.
            sensor_info.proxy->add_local_options();

            auto recommended_filters_names = get_recommended_filters_names(stream);
            for( auto & filter_name : recommended_filters_names )
            {
                sensor_info.proxy->add_processing_block( filter_name );
            }

            auto& embedded_filters = stream->embedded_filters();
            for (auto& embedded_filter : embedded_filters)
            {
                sensor_info.proxy->add_embedded_filter(embedded_filter);
            }
        } );  // End foreach_stream lambda

    for( auto & name_info : sensor_name_to_info )
    {
        auto & sensor_name = name_info.first;
        auto & sensor_info = name_info.second;
        auto & sensor_proxy = sensor_info.proxy;
        //LOG_DEBUG( sensor_info.first );

        // Set profile's ID based on the dds_stream's ID (index already set). Connect the profile to the extrinsics graph.
        // The get_stream_profiles() call will initialize the profiles (calling dds_sensor_proxy::init_stream_profiles())
        for( auto & profile : sensor_proxy->get_stream_profiles() )
        {
            auto source_profile = profile;
            if( get_format_conversion() != format_conversion::raw )
            {
                auto & source_profiles = sensor_proxy->_formats_converter.get_source_profiles_from_target( profile );
                if( source_profiles.size() != 1 )
                    LOG_ERROR( "More than one source profile available for [" << profile << "]: " << source_profiles );
                auto source_profile = source_profiles[0];
            }

            sid_index type_and_index( source_profile->get_stream_type(), source_profile->get_stream_index() );
            const auto & it = sensor_info.type_and_index_to_dds_stream_sidx.find( type_and_index );
            if( it != sensor_info.type_and_index_to_dds_stream_sidx.end())
            {
                sid_index sidx = (*it).second;

                auto & streams = sensor_proxy->streams();
                auto stream_iter = streams.find( sidx );
                if( stream_iter == streams.end() )
                {
                    LOG_ERROR( "No dds stream " << sidx.to_string() << " found for '" << sensor_name << "' " << profile
                                                << " -> " << source_profile << " " << type_and_index.to_string() );
                    continue;
                }

                //LOG_DEBUG( "    " << profile << " -> " << source_profile << " " << type_and_index.to_string() );
                profile->set_unique_id( sidx.sid );  // Was lost on clone

                // NOTE: the 'get_stream_profiles' call above creates target profiles from the raw profiles we supplied it.
                // The raw profile intrinsics will be overriden to call the target's intrinsics function (which by default
                // calls the raw again, creating an infinite loop), so we must override the target:
                set_profile_intrinsics( profile, stream_iter->second );

                _stream_name_to_profiles.at( stream_iter->second->name() ).push_back( profile );  // For extrinsics

                tag_default_profile_of_stream( profile, stream_iter->second );
            }
        }
    }

    if( _dds_dev->supports_metadata() )
    {
        _metadata_subscription = _dds_dev->on_metadata_available(
            [this]( std::shared_ptr< const json > const & dds_md )
            {
                auto & stream_name = dds_md->nested( realdds::topics::metadata::key::stream_name ).string_ref();
                auto it = _stream_name_to_owning_sensor.find( stream_name );
                if( it != _stream_name_to_owning_sensor.end() )
                    it->second->handle_new_metadata( stream_name, dds_md );
            } );
    }

    // According to extrinsics_graph (in environment.h) we need 3 steps:

    // 1. Register streams with extrinsics between them
    if( _dds_dev->has_extrinsics() )
    {
        for( auto & from_stream : _stream_name_to_librs_stream )
        {
            for( auto & to_stream : _stream_name_to_librs_stream )
            {
                if( from_stream.first != to_stream.first )
                {
                    auto const dds_extr = _dds_dev->get_extrinsics( from_stream.first, to_stream.first );
                    if( ! dds_extr )
                    {
                        //LOG_DEBUG( "missing extrinsics from " << from_stream.first << " to " << to_stream.first );
                        continue;
                    }
                    rs2_extrinsics extr = to_rs2_extrinsics( dds_extr );
                    environment::get_instance().get_extrinsics_graph().register_extrinsics( *from_stream.second,
                                                                                            *to_stream.second,
                                                                                            extr );
                }
            }
        }
    }

    // 2. Register all profiles
    for( auto & it : _stream_name_to_profiles )
    {
        for( auto profile : it.second )
        {
            environment::get_instance().get_extrinsics_graph().register_profile( *profile );
        }
    }

    // 3. Link profile to it's stream
    for( auto & it : _stream_name_to_librs_stream )
    {
        for( auto & profile : _stream_name_to_profiles[it.first] )
        {
            environment::get_instance().get_extrinsics_graph().register_same_extrinsics( *it.second, *profile );
        }
    }

    if( ! dev->device_info().is_recovery() )
    {
        // Call after seneors have been initialized
        ds_advanced_mode_base::initialize_advanced_mode( this );  // Call even if not enabled, so API calls won't throw
        device_specific_initialization();
    }
    // Use the default D400 matchers:
    // Depth & IR matched by frame-number, time-stamp-matched to color.
    // Motion streams will not get synced.
    rs2_matchers matcher = RS2_MATCHER_DLR_C;
    if( auto matcher_j = _dds_dev->participant()->settings().nested( "device", "matcher" ) )
    {
        if( ! matcher_j.is_string() || ! try_parse( matcher_j.string_ref(), matcher ) )
            LOG_WARNING( "Invalid 'device/matcher' value " << matcher_j );
    }
    set_matcher_type( matcher );

    if( supports_info( RS2_CAMERA_INFO_PRODUCT_LINE )
        && ! strcmp( get_info( RS2_CAMERA_INFO_PRODUCT_LINE ).c_str(), "D500" ) )
    {
        // Find depth sensor to pass into d500_auto_calibrated object
        sensor_base * depth_sensor = nullptr;
        for( auto & sensor : sensor_name_to_info )
            if( sensor.second.type == RS2_STREAM_DEPTH )
                depth_sensor = sensor.second.proxy.get();

        set_auto_calibration_capability( std::make_shared< d500_auto_calibrated >(
            std::make_shared< d500_debug_protocol_calibration_engine >( this ), this, depth_sensor ) );
    }

    _calibration_changed_subscription = _dds_dev->on_calibration_changed(
        [this]( std::shared_ptr< const realdds::dds_stream > const & stream )
        {
            auto it = _stream_name_to_profiles.find( stream->name() );
            if( it == _stream_name_to_profiles.end() )
                throw std::runtime_error( rsutils::string::from() << "no such stream?!" );

            if( auto video_stream = std::dynamic_pointer_cast< const realdds::dds_video_stream >( stream ) )
            {
                for( auto & profile : it->second )
                    set_video_profile_intrinsics( profile, video_stream );

                for( const auto & cb : _calib_changed_callbacks )
                    cb->on_calibration_change( rs2_calibration_status::RS2_CALIBRATION_SUCCESSFUL );
            }
            else
            {
                throw std::runtime_error( rsutils::string::from() << "non-video stream calibrations not supported" );
            }
        } );
}


int dds_device_proxy::get_index_from_stream_name( const std::string & name ) const
{
    int index = 0;

    // If camera sends stream index the info is appended to the stream name with underscore after the name
    // e.g 'Infrared_1'. However, there might be other underscores in the name, e.g. 'Compressed_Color_1'
    auto delimiter_pos = name.find_last_of( '_' );
    if( delimiter_pos != std::string::npos )
    {
        std::string index_as_string = name.substr( delimiter_pos + 1, name.size() );
        try
        {
            index = std::stoi(index_as_string);
        }
        catch (...)
        {
            //Do nothing, return default index
        }
    }

    return index;
}


int dds_device_proxy::get_unique_index_in_sensor( const std::shared_ptr< dds_sensor_proxy > & sensor,
                                                  const std::string & stream_name, rs2_stream stream_type ) const
{
    bool index_adjusted = false;
    int index = get_index_from_stream_name( stream_name );

    // Sending stream index as part of stream name is optional. We want to distinguish streams of same type in the
    // sensor so we create an internal, running index for it.
    do
    {
        index_adjusted = false;
        for( auto & stream : sensor->streams() )
        {
            // We can have multiple streams with same index (e.g. 0) but not two of the same type
            if( index == stream.first.index && stream_type == to_rs2_stream_type( stream.second->type_string() ) )
            {
                index++;
                index_adjusted = true;
            }
        }
    }
    while( index_adjusted );

    return index;
}


void dds_device_proxy::set_profile_intrinsics( std::shared_ptr< stream_profile_interface > const & profile,
                                               const std::shared_ptr< realdds::dds_stream > & stream ) const
{
    if( auto video_stream = std::dynamic_pointer_cast< realdds::dds_video_stream >( stream ) )
    {
        set_video_profile_intrinsics( profile, video_stream );
    }
    else if( auto motion_stream = std::dynamic_pointer_cast< realdds::dds_motion_stream >( stream ) )
    {
        set_motion_profile_intrinsics( profile, motion_stream );
    }
}


void dds_device_proxy::set_video_profile_intrinsics( std::shared_ptr< stream_profile_interface > const & profile,
                                                     std::shared_ptr< const realdds::dds_video_stream > const & stream ) const
{
    auto vsp = std::dynamic_pointer_cast< video_stream_profile >( profile );
    int const w = vsp->get_width();
    int const h = vsp->get_height();

    auto & stream_intrinsics = stream->get_intrinsics();
    if( 1 == stream_intrinsics.size() )
    {
        // A single set of intrinsics will get scaled to any profile resolution
        auto intr = stream_intrinsics.begin()->scaled_to( w, h );
        vsp->set_intrinsics( [intr = to_rs2_intrinsics( intr )]() { return intr; } );
    }
    else
    {
        // When we have multiple sets of intrinsics (one per resolution), we're limited to these
        auto it = std::find_if( stream_intrinsics.begin(),
                                stream_intrinsics.end(),
                                [w, h]( const realdds::video_intrinsics & intr )
                                { return intr.width == w && intr.height == h; } );
        if( it != stream_intrinsics.end() )  // Some profiles don't have intrinsics
        {
            vsp->set_intrinsics( [intr = to_rs2_intrinsics( *it )]() { return intr; } );
        }
    }

}


void dds_device_proxy::set_motion_profile_intrinsics( std::shared_ptr< stream_profile_interface > profile,
                                                      std::shared_ptr< realdds::dds_motion_stream > stream ) const
{
    auto msp = std::dynamic_pointer_cast< motion_stream_profile >( profile );
    auto & stream_intrinsics = stream->get_gyro_intrinsics();
    rs2_motion_device_intrinsic intr;
    memcpy( intr.data, stream_intrinsics.data.data(), sizeof( intr.data ) );
    memcpy( intr.noise_variances, stream_intrinsics.noise_variances.data(), sizeof( intr.noise_variances ) );
    memcpy( intr.bias_variances, stream_intrinsics.bias_variances.data(), sizeof( intr.bias_variances ) );
    msp->set_intrinsics( [intr]() { return intr; } );
}


std::shared_ptr< dds_sensor_proxy > dds_device_proxy::create_sensor( const std::string & sensor_name,
                                                                     rs2_stream sensor_type )
{
    switch( sensor_type )
    {
    case RS2_STREAM_DEPTH:
        return std::make_shared< dds_depth_sensor_proxy >( sensor_name, this, _dds_dev );
    case RS2_STREAM_COLOR:
        return std::make_shared< dds_color_sensor_proxy >( sensor_name, this, _dds_dev );
    case RS2_STREAM_MOTION:
        return std::make_shared< dds_motion_sensor_proxy >( sensor_name, this, _dds_dev );
    case RS2_STREAM_ANY:
        // Generic: no type
        return std::make_shared< dds_sensor_proxy >( sensor_name, this, _dds_dev );
    }
    throw std::runtime_error( rsutils::string::from()
                              << "invalid sensor '" << sensor_name << "' type " << sensor_type );
}


// Tagging converted profiles. dds_sensor_proxy::add_video/motion_stream tagged the raw profiles.
void dds_device_proxy::tag_default_profile_of_stream(
    const std::shared_ptr< stream_profile_interface > & profile,
    const std::shared_ptr< const realdds::dds_stream > & stream ) const
{
    auto const & dds_default_profile = stream->default_profile();
    int target_format;
    auto dds_vsp = std::dynamic_pointer_cast< realdds::dds_video_stream_profile >( dds_default_profile );
    if( dds_vsp )
    {
        target_format = dds_vsp->encoding().to_rs2();
        switch( target_format )
        {
        case RS2_FORMAT_YUYV:
            // We want to change to RGB8 - that's the LibRS default
            if( get_format_conversion() == format_conversion::full )
                target_format = RS2_FORMAT_RGB8;
            break;
        }
    }

    if( profile->get_stream_type() == to_rs2_stream_type( stream->type_string() ) &&
        profile->get_framerate() == dds_default_profile->frequency() )
    {
        auto vsp = std::dynamic_pointer_cast< video_stream_profile >( profile );
        if( vsp && dds_vsp )
        {
            if( vsp->get_width() != dds_vsp->width() || vsp->get_height() != dds_vsp->height() )
                return;  // Incompatible resolutions

            if( vsp->get_format() != target_format )
                return;  // Incompatible format
        }

        // Default  = picked up in pipeline default config (without specifying streams)
        // Superset = picked up in pipeline with enable_all_stream() config
        // We want color and depth to be default, and add infrared as superset
        int tag = PROFILE_TAG_SUPERSET;
        if( ( strcmp( stream->type_string(), "color" ) == 0 && profile->get_stream_index() == 0 ) || // Now we have cameras with more then one color stream, take the first
              strcmp( stream->type_string(), "depth" ) == 0 )
            tag |= PROFILE_TAG_DEFAULT;
        else if( strcmp( stream->type_string(), "ir" ) != 0 || profile->get_stream_index() >= 2 )
            return;  // leave untagged
        profile->tag_profile( tag );
    }
}


void dds_device_proxy::tag_profiles( stream_profiles profiles ) const
{
    //Do nothing. PROFILE_TAG_DEFAULT is already added in tag_default_profile_of_stream.
}


void dds_device_proxy::hardware_reset()
{
    json control = json::object( { { realdds::topics::control::key::id, realdds::topics::control::hw_reset::id } } );
    json reply;
    _dds_dev->send_control( control, &reply );
}

bool dds_device_proxy::is_in_recovery_mode() const
{
    return _dds_dev->device_info().is_recovery();
}

std::string dds_device_proxy::get_opcode_string(int opcode) const
{
    std::string product_line = get_info(RS2_CAMERA_INFO_PRODUCT_LINE);
    if (product_line.find("D400") != std::string::npos)
    {
        // d400 device
        return ds::d400_hwmon_response().hwmon_error2str(opcode);
    }
    else if (product_line.find("D500") != std::string::npos)
    {
        // d500 device
        return ds::d500_hwmon_response().hwmon_error2str(opcode);
    }
    return "";
}


std::vector< uint8_t > dds_device_proxy::send_receive_raw_data( const std::vector< uint8_t > & input )
{
    // debug_interface function
    auto hexdata = rsutils::string::hexarray::to_string( input );
    json control = json::object( { { realdds::topics::control::key::id, realdds::topics::control::hwm::id },
                                   { realdds::topics::control::hwm::key::data, hexdata } } );
    json reply;
    _dds_dev->send_control( control, &reply );
    rsutils::string::hexarray data;
    if( ! reply.nested( realdds::topics::reply::hwm::key::data ).get_ex( data ) )
        throw std::runtime_error( "Failed HWM: missing 'data' in reply" );
    return data.detach();
}


std::vector< uint8_t > dds_device_proxy::build_command( uint32_t opcode,
                                                        uint32_t param1,
                                                        uint32_t param2,
                                                        uint32_t param3,
                                                        uint32_t param4,
                                                        uint8_t const * data,
                                                        size_t dataLength ) const
{
    // debug_interface function
    rsutils::string::hexarray hexdata( std::vector< uint8_t >( data, data + dataLength ) );
    json control = rsutils::json::object( { { realdds::topics::control::key::id, realdds::topics::control::hwm::id },
                                            { realdds::topics::control::hwm::key::data, hexdata },
                                            { realdds::topics::control::hwm::key::opcode, opcode },
                                            { realdds::topics::control::hwm::key::param1, param1 },
                                            { realdds::topics::control::hwm::key::param2, param2 },
                                            { realdds::topics::control::hwm::key::param3, param3 },
                                            { realdds::topics::control::hwm::key::param4, param4 },
                                            { realdds::topics::control::hwm::key::build_command, true } } );
    json reply;
    _dds_dev->send_control( control, &reply );
    if( ! reply.nested( realdds::topics::reply::hwm::key::data ).get_ex( hexdata ) )
        throw std::runtime_error( "Failed HWM: missing 'data' in reply" );
    return hexdata.detach();
}


bool dds_device_proxy::check_fw_compatibility( const std::vector< uint8_t > & image ) const
{
    try
    {
        // Start DFU
        auto const crc = rsutils::number::calc_crc32( image.data(), image.size() );
        json dfu_start{
            { realdds::topics::control::key::id, realdds::topics::control::dfu_start::id },
            { realdds::topics::control::dfu_start::key::size, image.size() },
            { realdds::topics::control::dfu_start::key::crc, crc },
        };
        json reply;
        _dds_dev->send_control( dfu_start, &reply );  // throws on error

        // Set up a reply handler that will get the "dfu-ready" message
        std::mutex mutex;
        std::condition_variable cv;
        json dfu_ready;
        auto subscription = _dds_dev->on_notification(
            [&]( std::string const & id, json const & notification )
            {
                if( id != realdds::topics::notification::dfu_ready::id )
                    return;
                std::unique_lock< std::mutex > lock( mutex );
                dfu_ready = notification;
                cv.notify_all();
            } );

        // Upload the image; this will check its compatibility
        auto topic = realdds::topics::blob_msg::create_topic( _dds_dev->participant(),
                                                              _dds_dev->device_info().topic_root()
                                                                  + realdds::topics::DFU_TOPIC_NAME );
        auto writer = std::make_shared< realdds::dds_topic_writer >( topic );
        realdds::dds_topic_writer::qos wqos( eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS );
        wqos.publish_mode().kind = eprosima::fastdds::dds::ASYNCHRONOUS_PUBLISH_MODE;
        wqos.publish_mode().flow_controller_name = "dfu";
        writer->override_qos_from_json( wqos, _dds_dev->participant()->settings().nested( "device", "dfu" ) );
        writer->run( wqos );
        if( ! writer->wait_for_readers( { 3, 0 } ) )
            throw std::runtime_error( "timeout waiting for DFU subscriber" );
        LOG_DEBUG( "transmitting image: " << image.size() << " bytes; crc= " << crc );
        auto blob = realdds::topics::blob_msg( std::vector< uint8_t >( image ) );
        blob.write_to( *writer );

        double timeout = 5.;  // seconds
        if( auto controller
            = _dds_dev->participant()->find_flow_controller( wqos.publish_mode().flow_controller_name ) )
        {
            // Timeout depends on how much data we need to send, because of the flow controller
            auto const seconds_to_send = realdds::estimate_seconds_to_send( blob.data().size(), *controller );
            timeout += seconds_to_send * 2;  // *2 in case of resend etc.
            LOG_DEBUG( "expecting ~" << rsutils::string::from( seconds_to_send, 2 ) << " seconds for image to get sent; timeout= " << rsutils::string::from( timeout, 2 ) );
        }
        if( ! writer->wait_for_acks( timeout ) )
            throw std::runtime_error( "timeout waiting for DFU image ack" );

        // Wait for a reply
        {
            std::unique_lock< std::mutex > lock( mutex );
            if( ! cv.wait_for( lock, std::chrono::seconds( 5 ), [&]() { return ! dfu_ready.is_null(); } ) )
                throw std::runtime_error( "timeout waiting for dfu-ready" );
            subscription.cancel();
        }
        LOG_DEBUG( dfu_ready );
        std::string tmp_error_str;
        realdds::dds_device::check_reply( dfu_ready, &tmp_error_str );
        if( ! tmp_error_str.empty() )
            return false; // Error is logged by notification reader thread
    }
    catch( std::exception const & e )
    {
        throw invalid_value_exception( e.what() );
    }

    return true;
}


void dds_device_proxy::update_flash( std::vector< uint8_t > const & image, rs2_update_progress_callback_sptr, int update_mode )
{
    throw not_implemented_exception( "update_flash not yet implemented" );
}


void dds_device_proxy::update( const void * /*image*/, int /*image_size*/, rs2_update_progress_callback_sptr callback ) const
{
    // Set up a reply handler that will get the "dfu-apply" messages and progress notifications
    // NOTE: this depends on the implementation! If the device goes offline (as the DDS adapter device will), we won't
    // get anything...
    std::atomic< bool > dfu_done( false );
    std::string error_desc;
    auto subscription = _dds_dev->on_notification(
        [callback, &dfu_done, &error_desc]( std::string const & id, json const & notification )
        {
            if( id != realdds::topics::notification::dfu_apply::id )
                return;

            // Break early on error. No status field means no error.
            if( auto status_j = notification.nested( realdds::topics::reply::key::status ) )
            {
                if( status_j.is_string() && status_j.string_ref() == realdds::topics::reply::status::ok )
                    return;
                error_desc = notification.nested( realdds::topics::reply::key::explanation ).string_ref_or_empty();
                dfu_done = true;
            }

            // Update progress to user, signal device-proxy thread that process is done at 100%
            if( auto progress
                = notification.nested( realdds::topics::notification::dfu_apply::key::progress, &json::is_number ) )
            {
                auto fraction = progress.get< float >();
                LOG_DEBUG( "... DFU progress: " << ( fraction * 100 ) );
                if( callback )
                    callback->on_update_progress( fraction );
                if (fraction >= 1) // 100%
                    dfu_done = true;
            }
        } );

    // Will throw if an error is returned
    json reply;
    _dds_dev->send_control(
        json::object( { { realdds::topics::control::key::id, realdds::topics::control::dfu_apply::id } } ),
        &reply );

    // The device will take time to do its thing. We want to return only when it's done, but we cannot know when it's
    // done if it goes down. It should go down right before restarting, so that's what we wait for:
    rsutils::time::timer timer{ std::chrono::minutes( 5 ) };
    do
    {
        if( timer.has_expired() )
            throw std::runtime_error( "timeout waiting for device update" );
        std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
    }
    while( ! dfu_done && ! _dds_dev->is_offline() );

    if( ! error_desc.empty() )
        throw std::runtime_error( "Device DFU failure: " + error_desc );
}


bool dds_device_proxy::supports_ethernet_configuration()
{
    std::string dev_name = get_info( RS2_CAMERA_INFO_NAME ); // Info supported, registered at constructor
    if( dev_name.find( "D555" ) == std::string::npos ) // Currently only D555 devices support eth_config
        return false;

    return eth_config_device::supports_ethernet_configuration();
}


void dds_device_proxy::device_specific_initialization()
{
    if( ds_advanced_mode_base::_enabled )
        ds_advanced_mode_base::_amplitude_factor_support = true;
}

std::vector<std::string> dds_device_proxy::get_recommended_filters_names(const std::shared_ptr<realdds::dds_stream> stream) const
{
    std::vector<std::string> filter_names;
    if (auto depth_stream = std::dynamic_pointer_cast< realdds::dds_depth_stream >(stream))
    {
        filter_names.push_back("Decimation Filter");
        filter_names.push_back("Rotation Filter");
        filter_names.push_back("HDR Merge");
        filter_names.push_back("Filter By Sequence id");
        filter_names.push_back("Threshold Filter");
        filter_names.push_back("Depth to Disparity");
        filter_names.push_back("Spatial Filter");
        filter_names.push_back("Temporal Filter");
        filter_names.push_back("Hole Filling Filter");
        filter_names.push_back("Disparity to Depth");
    }
    else if (auto color_stream = std::dynamic_pointer_cast< realdds::dds_color_stream >(stream))
    {
        filter_names.push_back("Decimation Filter");
        filter_names.push_back("Rotation Filter");
    }
    return filter_names;
}

}  // namespace librealsense
