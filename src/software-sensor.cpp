// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "software-sensor.h"
#include "software-device.h"
#include "stream.h"
#include "option.h"
#include "core/video-frame.h"
#include "core/notification.h"
#include "depth-sensor.h"
#include <src/metadata-parser.h>

#include <rsutils/string/from.h>
#include <rsutils/deferred.h>

using rsutils::deferred;


namespace librealsense {


static std::shared_ptr< metadata_parser_map > create_software_metadata_parser_map()
{
    auto md_parser_map = std::make_shared< metadata_parser_map >();
    for( int i = 0; i < static_cast< int >( rs2_frame_metadata_value::RS2_FRAME_METADATA_COUNT ); ++i )
    {
        auto key = static_cast< rs2_frame_metadata_value >( i );
        std::shared_ptr< md_attribute_parser_base > fallback;
        switch( key )
        {
        case RS2_FRAME_METADATA_TIME_OF_ARRIVAL:
            fallback = make_additional_data_parser_unless( &frame_additional_data::system_time, rs2_time_t( 0 ) );
            break;
        case RS2_FRAME_METADATA_BACKEND_TIMESTAMP:
            fallback = make_additional_data_parser_unless( &frame_additional_data::backend_timestamp, rs2_time_t(0) );
            break;
        case RS2_FRAME_METADATA_RAW_FRAME_SIZE:
            fallback = make_additional_data_parser_unless( &frame_additional_data::raw_size, uint32_t(0) );
            break;
        case RS2_FRAME_METADATA_ACTUAL_FPS:
            fallback = std::make_shared< ds_md_attribute_actual_fps >();
            break;
        }
        md_parser_map->emplace( key, std::make_shared< md_array_parser >( key, fallback ) );
    }
    return md_parser_map;
}


class software_sensor::stereo_extension : public depth_stereo_sensor
{
public:
    stereo_extension( software_sensor * owner )
        : _owner( owner )
    {
    }

    float get_depth_scale() const override { return _owner->get_option( RS2_OPTION_DEPTH_UNITS ).query(); }

    float get_stereo_baseline_mm() const override { return _owner->get_option( RS2_OPTION_STEREO_BASELINE ).query(); }

private:
    software_sensor * _owner;
};


software_sensor::software_sensor( std::string const & name, software_device * owner )
    : sensor_base( name, owner )
    , _stereo_extension( [this]() { return stereo_extension( this ); } )
    , _metadata_map{}  // to all 0's
{
    // At this time (and therefore for backwards compatibility) no register_metadata is required for SW sensors,
    // and metadata persists between frames (!!!!!!!). All SW sensors support ALL metadata. We can therefore
    // also share their parsers:
    static auto software_metadata_parser_map = create_software_metadata_parser_map();
    _metadata_parsers = software_metadata_parser_map;
}


software_sensor::~software_sensor()
{
}


std::shared_ptr< stream_profile_interface > software_sensor::add_video_stream( rs2_video_stream video_stream,
                                                                               bool is_default, std::string name )
{
    auto profile = std::make_shared< video_stream_profile >();
    profile->set_dims( video_stream.width, video_stream.height );
    profile->set_format( video_stream.fmt );
    profile->set_framerate( video_stream.fps );
    profile->set_stream_index( video_stream.index );
    profile->set_stream_type( video_stream.type );
    profile->set_unique_id( video_stream.uid );
    profile->set_intrinsics( [=]() { return video_stream.intrinsics; } );
    profile->set_name( name );
    if( is_default )
        profile->tag_profile( profile_tag::PROFILE_TAG_DEFAULT );
    _sw_profiles.push_back( profile );

    return profile;
}


std::shared_ptr< stream_profile_interface > software_sensor::add_motion_stream( rs2_motion_stream motion_stream,
                                                                                bool is_default, std::string name )
{
    auto profile = std::make_shared< motion_stream_profile >();
    profile->set_format( motion_stream.fmt );
    profile->set_framerate( motion_stream.fps );
    profile->set_stream_index( motion_stream.index );
    profile->set_stream_type( motion_stream.type );
    profile->set_unique_id( motion_stream.uid );
    profile->set_intrinsics( [=]() { return motion_stream.intrinsics; } );
    profile->set_name( name );
    if( is_default )
        profile->tag_profile( profile_tag::PROFILE_TAG_DEFAULT );
    _sw_profiles.push_back( profile );

    return profile;
}


std::shared_ptr< stream_profile_interface > software_sensor::add_pose_stream( rs2_pose_stream pose_stream, bool is_default, std::string name )
{
    auto profile = std::make_shared< pose_stream_profile >();
    if( ! profile )
        throw librealsense::invalid_value_exception( "null pointer passed for argument \"profile\"." );

    profile->set_format( pose_stream.fmt );
    profile->set_framerate( pose_stream.fps );
    profile->set_stream_index( pose_stream.index );
    profile->set_stream_type( pose_stream.type );
    profile->set_unique_id( pose_stream.uid );
    profile->set_name( name );
    if( is_default )
        profile->tag_profile( profile_tag::PROFILE_TAG_DEFAULT );
    _sw_profiles.push_back( profile );

    return std::move( profile );
}


std::shared_ptr< stream_profile_interface > software_sensor::add_inference_stream( rs2_inference_stream inference_stream, bool is_default, std::string name )
{
    auto profile = std::make_shared< inference_stream_profile >();
    if( ! profile )
        throw librealsense::invalid_value_exception( "null pointer passed for argument \"profile\"." );

    profile->set_framerate( inference_stream.fps );
    profile->set_stream_index( inference_stream.index );
    profile->set_stream_type( inference_stream.type );
    profile->set_unique_id( inference_stream.uid );
    profile->set_name( name );
    if( is_default )
        profile->tag_profile( profile_tag::PROFILE_TAG_DEFAULT );
    _sw_profiles.push_back( profile );

    return std::move( profile );
}


bool software_sensor::extend_to( rs2_extension extension_type, void ** ptr )
{
    if( extension_type == RS2_EXTENSION_DEPTH_SENSOR )
    {
        if( supports_option( RS2_OPTION_DEPTH_UNITS ) )
        {
            auto & ext = *_stereo_extension;
            *ptr = static_cast< depth_sensor * >( &ext );
            return true;
        }
    }
    else if( extension_type == RS2_EXTENSION_DEPTH_STEREO_SENSOR )
    {
        if( supports_option( RS2_OPTION_DEPTH_UNITS ) && supports_option( RS2_OPTION_STEREO_BASELINE ) )
        {
            *ptr = &( *_stereo_extension );
            return true;
        }
    }
    return false;
}


stream_profiles software_sensor::init_stream_profiles()
{
    // By default the raw profiles we're given is what we output
    return _sw_profiles;
}


void software_sensor::open( const stream_profiles & requests )
{
    if( _is_streaming )
        throw wrong_api_call_sequence_exception( "open(...) failed. Software device is streaming!" );
    else if( _is_opened )
        throw wrong_api_call_sequence_exception( "open(...) failed. Software device is already opened!" );
    _is_opened = true;
    set_active_streams( requests );
}


void software_sensor::close()
{
    if( _is_streaming )
        throw wrong_api_call_sequence_exception( "close() failed. Software device is streaming!" );
    else if( ! _is_opened )
        throw wrong_api_call_sequence_exception( "close() failed. Software device was not opened!" );
    _is_opened = false;
    set_active_streams( {} );
}


void software_sensor::start( frame_callback_ptr callback )
{
    if( _is_streaming )
        throw wrong_api_call_sequence_exception( "start_streaming(...) failed. Software device is already streaming!" );
    else if( ! _is_opened )
        throw wrong_api_call_sequence_exception( "start_streaming(...) failed. Software device was not opened!" );
    _source.get_published_size_option()->set( 0 );
    _source.init( _metadata_parsers );
    _source.set_sensor( this->shared_from_this() );
    _source.set_callback( callback );
    _is_streaming = true;
    raise_on_before_streaming_changes( true );
}


void software_sensor::stop()
{
    if( ! _is_streaming )
        throw wrong_api_call_sequence_exception( "stop_streaming() failed. Software device is not streaming!" );

    _is_streaming = false;
    raise_on_before_streaming_changes( false );
    _source.flush();
    _source.reset();
}


void software_sensor::set_metadata( rs2_frame_metadata_value key, rs2_metadata_type value )
{
    _metadata_map[key] = { true, value };
}


void software_sensor::erase_metadata( rs2_frame_metadata_value key )
{
    _metadata_map[key].is_valid = false;
}


frame_interface * software_sensor::allocate_new_frame( rs2_extension extension,
                                                       stream_profile_interface * profile,
                                                       frame_additional_data && data )
{
    auto frame_number = data.frame_number; // For logging
    auto frame = _source.alloc_frame( { profile->get_stream_type(), profile->get_stream_index(), extension },
                                      0,
                                      std::move( data ),
                                      false );
    if( ! frame )
    {
        LOG_WARNING( "Failed to allocate frame " << frame_number << " type " << profile->get_stream_type() );
    }
    else
    {
        frame->set_stream( std::dynamic_pointer_cast< stream_profile_interface >( profile->shared_from_this() ) );
    }
    return frame;
}


frame_interface * software_sensor::allocate_new_video_frame( video_stream_profile_interface * profile,
                                                             int stride,
                                                             int bpp,
                                                             frame_additional_data && data )
{
    auto frame = allocate_new_frame( profile->get_stream_type() == RS2_STREAM_DEPTH ? RS2_EXTENSION_DEPTH_FRAME
                                                                                    : RS2_EXTENSION_VIDEO_FRAME,
                                     profile,
                                     std::move( data ) );
    if( frame )
    {
        auto vid_frame = dynamic_cast< video_frame * >( frame );

        if (!vid_frame)
            throw std::runtime_error("Frame is not video frame");

        vid_frame->assign( profile->get_width(), profile->get_height(), stride, bpp * 8 );
        auto sd = dynamic_cast< software_device * >( _owner );
        if (!sd)
            throw std::runtime_error("Owner is not a software device");

        sd->register_extrinsic( *profile );
    }
    return frame;
}


void software_sensor::invoke_new_frame( frame_holder && frame, void const * pixels, std::function< void() > on_release )
{
    // The frame pixels/data are stored in the continuation object!
    if( pixels )
        frame->attach_continuation( frame_continuation( on_release, pixels ) );
    _source.invoke_callback( std::move( frame ) );
}


void software_sensor::on_video_frame( rs2_software_video_frame const & software_frame )
{
    deferred on_release( [deleter = software_frame.deleter, data = software_frame.pixels]() { deleter( data ); } );

    stream_profile_interface * profile = software_frame.profile->profile;
    auto vid_profile = dynamic_cast< video_stream_profile_interface * >( profile );
    if( ! vid_profile )
        throw invalid_value_exception( "Non-video profile provided to on_video_frame" );

    if( ! _is_streaming )
        return;

    frame_additional_data data( _metadata_map );
    data.timestamp = software_frame.timestamp;
    data.timestamp_domain = software_frame.domain;
    data.frame_number = software_frame.frame_number;

    // Depth units, if not provided by the user, should default to the current sensor value of DEPTH_UNITS:
    if( vid_profile->get_stream_type() != RS2_STREAM_DEPTH )
        data.depth_units = 0;
    else if( software_frame.depth_units )
        data.depth_units = software_frame.depth_units;
    else if( auto opt = get_option_handler( RS2_OPTION_DEPTH_UNITS ) )
        data.depth_units = opt->query();
    else
        data.depth_units = 0.f;

    auto frame = allocate_new_video_frame( vid_profile, software_frame.stride, software_frame.bpp, std::move( data ) );
    if( frame )
        invoke_new_frame( frame, software_frame.pixels, on_release.detach() );
}


void software_sensor::on_motion_frame( rs2_software_motion_frame const & software_frame )
{
    deferred on_release( [deleter = software_frame.deleter, data = software_frame.data]() { deleter( data ); } );
    if( ! _is_streaming )
        return;

    frame_additional_data data( _metadata_map );
    data.timestamp = software_frame.timestamp;
    data.timestamp_domain = software_frame.domain;
    data.frame_number = software_frame.frame_number;

    auto frame = allocate_new_frame( RS2_EXTENSION_MOTION_FRAME, software_frame.profile->profile, std::move( data ) );
    if( frame )
        invoke_new_frame( frame, software_frame.data, on_release.detach() );
}


void software_sensor::on_pose_frame( rs2_software_pose_frame const & software_frame )
{
    deferred on_release( [deleter = software_frame.deleter, data = software_frame.data]() { deleter( data ); } );
    if( ! _is_streaming )
        return;

    frame_additional_data data( _metadata_map );
    data.timestamp = software_frame.timestamp;
    data.timestamp_domain = software_frame.domain;
    data.frame_number = software_frame.frame_number;

    auto frame = allocate_new_frame( RS2_EXTENSION_POSE_FRAME, software_frame.profile->profile, std::move( data ) );
    if( frame )
        invoke_new_frame( frame, software_frame.data, on_release.detach() );
}


void software_sensor::on_notification( rs2_software_notification const & notif )
{
    notification n{ notif.category, notif.type, notif.severity, notif.description };
    n.serialized_data = notif.serialized_data;
    _notifications_processor->raise_notification( n );
}


void software_sensor::add_read_only_option( rs2_option option, float val )
{
    register_option( option, std::make_shared< const_value_option >( "bypass sensor read only option", val ) );
}


void software_sensor::update_read_only_option( rs2_option option, float val )
{
    if( auto opt = dynamic_cast< readonly_float_option * >( &get_option( option ) ) )
        opt->update( val );
    else
        throw invalid_value_exception( rsutils::string::from() << "option " << get_string( option )
                                                               << " is not read-only or is deprecated type" );
}


void software_sensor::add_option( rs2_option option, option_range range, bool is_writable )
{
    register_option( option,
                     ( is_writable ? std::make_shared< float_option >( range )
                                   : std::make_shared< readonly_float_option >( range ) ) );
}


void software_sensor::add_processing_block( std::shared_ptr< processing_block_interface > const & block )
{
    if( ! block )
        throw invalid_value_exception( "trying to add an empty software processing block" );

    _pbs.push_back( block );
}


std::vector< rs2_option > software_sensor::get_supported_options() const
{
    // Override the default options_container behavior: software sensors return the options in the same order they were
    // registered!
    return _ordered_options;
}


}  // namespace librealsense
