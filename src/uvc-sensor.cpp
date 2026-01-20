// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023-4 RealSense, Inc. All Rights Reserved.

#include "uvc-sensor.h"
#include "device.h"
#include "stream.h"
#include "image.h"
#include "global_timestamp_reader.h"
#include "core/video-frame.h"
#include "core/notification.h"
#include "platform/uvc-option.h"
#include "platform/stream-profile-impl.h"
#include <src/metadata-parser.h>
#include <src/core/time-service.h>


namespace librealsense {


// in sensor.cpp
void log_callback_end( uint32_t fps,
                       rs2_time_t callback_start_time,
                       rs2_time_t callback_end_time,
                       rs2_stream stream_type,
                       unsigned long long frame_number );


uvc_sensor::uvc_sensor( std::string const & name,
                        std::shared_ptr< platform::uvc_device > uvc_device,
                        std::unique_ptr< frame_timestamp_reader > timestamp_reader,
                        device * dev )
    : super( name, dev )
    , _device( std::move( uvc_device ) )
    , _timestamp_reader( std::move( timestamp_reader ) )
    , _gyro_counter(0)
    , _accel_counter(0)
{
    register_metadata( RS2_FRAME_METADATA_BACKEND_TIMESTAMP,
                       make_additional_data_parser( &frame_additional_data::backend_timestamp ) );
    register_metadata( RS2_FRAME_METADATA_RAW_FRAME_SIZE,
                       make_additional_data_parser( &frame_additional_data::raw_size ) );
}


uvc_sensor::~uvc_sensor()
{
    try
    {
        if( _is_streaming )
            uvc_sensor::stop();

        if( _is_opened )
            uvc_sensor::close();
    }
    catch( ... )
    {
        LOG_ERROR( "An error has occurred while stop_streaming()!" );
    }
}

void uvc_sensor::verify_supported_requests( const stream_profiles & requests ) const
{
    // This method's aim is to send a relevant exception message when a user tries to stream
    // twice the same stream (at least) with different configurations (fps, resolution)
    std::map< rs2_stream, uint32_t > requests_map;
    for( auto && req : requests )
    {
        requests_map[req->get_stream_type()] = req->get_framerate();
    }

    if( requests_map.size() < requests.size() )
        throw( std::runtime_error( "Wrong configuration requested" ) );

    // D457 dev
    // taking into account that only with D457, streams GYRo and ACCEL will be
    // mapped as uvc instead of hid
    uint32_t gyro_fps = -1;
    uint32_t accel_fps = -1;
    for( auto it = requests_map.begin(); it != requests_map.end(); ++it )
    {
        if( it->first == RS2_STREAM_GYRO )
            gyro_fps = it->second;
        else if( it->first == RS2_STREAM_ACCEL )
            accel_fps = it->second;
        if( gyro_fps != -1 && accel_fps != -1 )
            break;
    }

    if( gyro_fps != -1 && accel_fps != -1 && gyro_fps != accel_fps )
    {
        throw( std::runtime_error(
            "Wrong configuration requested - GYRO and ACCEL streams' fps to be equal for this device" ) );
    }
}

void uvc_sensor::open( const stream_profiles & requests )
{
    std::lock_guard< std::mutex > lock( _configure_lock );
    if( _is_streaming )
        throw wrong_api_call_sequence_exception( "open(...) failed. UVC device is streaming!" );
    else if( _is_opened )
        throw wrong_api_call_sequence_exception( "open(...) failed. UVC device is already opened!" );

    auto on = std::unique_ptr< power >( new power( std::dynamic_pointer_cast< uvc_sensor >( shared_from_this() ) ) );

    _source.init( _metadata_parsers );
    _source.set_sensor( _source_owner->shared_from_this() );

    std::vector< platform::stream_profile > commited;

    verify_supported_requests( requests );

    for( auto && req_profile : requests )
    {
        auto && req_profile_base = std::dynamic_pointer_cast< stream_profile_base >( req_profile );
        try
        {
            unsigned long long last_frame_number = 0;
            rs2_time_t last_timestamp = 0;
            _device->probe_and_commit(
                req_profile_base->get_backend_profile(),
                [this, req_profile_base, req_profile, last_frame_number, last_timestamp](
                    platform::stream_profile p,
                    platform::frame_object f,
                    std::function< void() > continuation ) mutable
                {
                    const auto system_time = time_service::get_time();  // time frame was received from the backend

                    if( ! this->is_streaming() )
                    {
                        LOG_WARNING( "Frame received with streaming inactive,"
                                     << librealsense::get_string( req_profile_base->get_stream_type() )
                                     << req_profile_base->get_stream_index() << ", Arrived," << std::fixed
                                     << f.backend_time << " " << system_time );
                        return;
                    }

                    auto && fr = generate_frame_from_data( f,
                                                                 system_time,
                                                                 _timestamp_reader.get(),
                                                                 last_timestamp,
                                                                 last_frame_number,
                                                                 req_profile_base );
                    auto timestamp_domain = _timestamp_reader->get_frame_timestamp_domain( fr );
                    auto bpp = get_image_bpp( req_profile_base->get_format() );
                    auto & frame_counter = fr->additional_data.frame_number;
                    auto & timestamp = fr->additional_data.timestamp;

                    // D457 development
                    size_t expected_size;
                    auto && msp = As< motion_stream_profile, stream_profile_interface >( req_profile );
                    if( msp )
                    {
                        expected_size = 64;  // 32; // D457 - WORKAROUND - SHOULD BE REMOVED AFTER CORRECTION IN DRIVER
                        //Motion stream on uvc is used only for mipi. Stream frame number counts gyro and accel together.
                        //We override it using 2 seperate counters.
                        auto stream_type = ((uint8_t *)f.pixels)[0];
                        if( stream_type == 1 ) // 1 == Accel
                            fr->additional_data.frame_number = ++_accel_counter;
                        else if( stream_type == 2 ) // 2 == Gyro
                            fr->additional_data.frame_number = ++_gyro_counter;
                        frame_counter = fr->additional_data.frame_number;
                    }
                        

                    LOG_DEBUG( "FrameAccepted,"
                               << librealsense::get_string( req_profile_base->get_stream_type() ) << ",Counter,"
                               << std::dec << fr->additional_data.frame_number << ",Index,"
                               << req_profile_base->get_stream_index() << ",BackEndTS," << std::fixed << f.backend_time
                               << ",SystemTime," << std::fixed << system_time << " ,diff_ts[Sys-BE],"
                               << system_time - f.backend_time << ",TS," << std::fixed << timestamp << ",TS_Domain,"
                               << rs2_timestamp_domain_to_string( timestamp_domain ) << ",last_frame_number,"
                               << last_frame_number << ",last_timestamp," << last_timestamp );

                    if( frame_counter <= last_frame_number )
                        LOG_INFO( "Frame counter reset" );

                    last_frame_number = frame_counter;
                    last_timestamp = timestamp;

                    const auto && vsp = As< video_stream_profile, stream_profile_interface >( req_profile );
                    int width = vsp ? vsp->get_width() : 0;
                    int height = vsp ? vsp->get_height() : 0;

                    assert( ( width * height ) % 8 == 0 );

                    // TODO: remove when adding confidence format
                    if( req_profile->get_stream_type() == RS2_STREAM_CONFIDENCE )
                        bpp = 4;

                    if( ! msp )
                        expected_size = compute_frame_expected_size( width, height, bpp );

                    // For compressed formats copy the raw data as is
                    if( val_in_range( req_profile_base->get_format(), { RS2_FORMAT_MJPEG } ) )
                        expected_size = static_cast< int >( f.frame_size );

                    auto extension = frame_source::stream_to_frame_types( req_profile_base->get_stream_type() );
                    frame_holder fh = _source.alloc_frame(
                        { req_profile_base->get_stream_type(), req_profile_base->get_stream_index(), extension },
                        expected_size,
                        std::move( fr->additional_data ),
                        true );
                    auto diff = time_service::get_time() - system_time;
                    if( diff > 10 )
                        LOG_DEBUG( "!! Frame allocation took " << diff << " msec" );

                    if( fh.frame )
                    {
                        // method should be limited to use of MIPI - not for USB
                        // the aim is to grab the data from a bigger buffer, which is aligned to 64 bytes,
                        // when the resolution's width is not aligned to 64
                        if( ( width * bpp >> 3 ) % 64 != 0 && f.frame_size > expected_size )
                        {
                            std::vector< uint8_t > pixels = align_width_to_64( width, height, bpp, (uint8_t *)f.pixels );
                            assert( expected_size == sizeof( uint8_t ) * pixels.size() );
                            memcpy( (void *)fh->get_frame_data(), pixels.data(), expected_size );
                        }
                        else
                        {
                            // The calibration format Y12BPP is modified to be 32 bits instead of 24 due to an issue
                            // with MIPI driver and padding that occurs in transmission. For D4xx cameras the original
                            // 24 bit support is achieved by comparing actual vs expected size: when it is exactly 75%
                            // of the MIPI-generated size (24/32bpp), then 24bpp-sized image will be processed
                            if( req_profile_base->get_format() == RS2_FORMAT_Y12I )
                                if( ( ( expected_size >> 2 ) * 3 ) == sizeof( uint8_t ) * f.frame_size )
                                    expected_size = sizeof( uint8_t ) * f.frame_size;

                            assert( expected_size == sizeof( uint8_t ) * f.frame_size );
                            memcpy( (void *)fh->get_frame_data(), f.pixels, expected_size );
                        }

                        auto && video = dynamic_cast< video_frame * >( fh.frame );
                        if( video )
                        {
                            video->assign( width, height, width * bpp / 8, bpp );
                        }

                        fh->set_timestamp_domain( timestamp_domain );
                        fh->set_stream( req_profile_base );

                        diff = time_service::get_time() - system_time;
                        if (diff > 10)
                            LOG_DEBUG("!! Frame memcpy took " << diff << " msec");
                    }

                    // calling the continuation method, and releasing the backend frame buffer
                    // since the content of the OS frame buffer has been copied, it can released ASAP
                    continuation();

                    if (!fh.frame)
                    {
                        LOG_INFO("Dropped frame. alloc_frame(...) returned nullptr");
                        return;
                    }

                    if( fh->get_stream().get() )
                    {
                        // Gather info for logging the callback ended
                        auto fps = fh->get_stream()->get_framerate();
                        auto stream_type = fh->get_stream()->get_stream_type();
                        auto frame_number = fh->get_frame_number();

                        // Invoke first callback
                        auto callback_start_time = time_service::get_time();
                        auto callback = fh->get_owner()->begin_callback();
                        _source.invoke_callback( std::move( fh ) );

                        // Log callback ended
                        log_callback_end( fps, callback_start_time, time_service::get_time(), stream_type, frame_number );
                    }
                } );
        }
        catch( ... )
        {
            for( auto && commited_profile : commited )
            {
                _device->close( commited_profile );
            }
            throw;
        }
        commited.push_back( req_profile_base->get_backend_profile() );
    }

    _internal_config = commited;

    if( _on_open )
        _on_open( _internal_config );

    _power = std::move( on );
    _is_opened = true;

    try
    {
        _device->stream_on( [&]( const notification & n ) { _notifications_processor->raise_notification( n ); } );
    }
    catch( ... )
    {
        std::stringstream error_msg;
        error_msg << "\tFormats: \n";
        for( auto && profile : _internal_config )
        {
            rs2_format fmt = fourcc_to_rs2_format( profile.format );
            error_msg << "\t " << std::string( rs2_format_to_string( fmt ) ) << std::endl;
            try
            {
                _device->close( profile );
            }
            catch( ... )
            {
            }
        }
        error_msg << std::endl;
        reset_streaming();
        _power.reset();
        _is_opened = false;

        throw std::runtime_error( error_msg.str() );
    }
    if( Is< librealsense::global_time_interface >( _owner ) )
    {
        As< librealsense::global_time_interface >( _owner )->enable_time_diff_keeper( true );
    }
    set_active_streams( requests );
}

void uvc_sensor::close()
{
    std::lock_guard< std::mutex > lock( _configure_lock );
    if( _is_streaming )
        throw wrong_api_call_sequence_exception( "close() failed. UVC device is streaming!" );
    else if( ! _is_opened )
        throw wrong_api_call_sequence_exception( "close() failed. UVC device was not opened!" );

    for( auto && profile : _internal_config )
    {
        try  // Handle disconnect event
        {
            _device->close( profile );
        }
        catch( ... )
        {
        }
    }
    reset_streaming();
    if( Is< librealsense::global_time_interface >( _owner ) )
    {
        As< librealsense::global_time_interface >( _owner )->enable_time_diff_keeper( false );
    }
    _power.reset();
    _is_opened = false;
    set_active_streams( {} );
}


void uvc_sensor::register_pu( rs2_option id )
{
    register_option( id, std::make_shared< uvc_pu_option >( std::dynamic_pointer_cast< uvc_sensor >( shared_from_this() ), id ) );
}


void uvc_sensor::prepare_for_bulk_operation()
{
    acquire_power();
}

void uvc_sensor::finished_bulk_operation()
{
    release_power();
}

void uvc_sensor::register_xu( platform::extension_unit xu )
{
    _device->register_xu( std::move( xu ) );
}


void uvc_sensor::start( rs2_frame_callback_sptr callback )
{
    std::lock_guard< std::mutex > lock( _configure_lock );
    if( _is_streaming )
        throw wrong_api_call_sequence_exception( "start_streaming(...) failed. UVC device is already streaming!" );
    else if( ! _is_opened )
        throw wrong_api_call_sequence_exception( "start_streaming(...) failed. UVC device was not opened!" );

    raise_on_before_streaming_changes( true );  // Required to be just before actual start allow recording to work
    _source.set_callback( callback );
    _is_streaming = true;
    _device->start_callbacks();
}

void uvc_sensor::stop()
{
    std::lock_guard< std::mutex > lock( _configure_lock );
    if( ! _is_streaming )
        throw wrong_api_call_sequence_exception( "stop_streaming() failed. UVC device is not streaming!" );

    _is_streaming = false;
    _device->stop_callbacks();
    _timestamp_reader->reset();
    _gyro_counter = 0;
    _accel_counter = 0;
    raise_on_before_streaming_changes( false );
}

void uvc_sensor::reset_streaming()
{
    _source.flush();
    _source.reset();
    _timestamp_reader->reset();
}

void uvc_sensor::acquire_power()
{
    try
    {
        _device->set_power_state( platform::D0 );
    }
    catch( std::exception const & e )
    {
        LOG_ERROR( "acquire_power failed: " << e.what() );
        throw;
    }
    catch( ... )
    {
        LOG_ERROR( "acquire_power failed" );
        throw;
    }
}

void uvc_sensor::release_power()
{
    try
    {
        _device->set_power_state( platform::D3 );
    }
    catch( std::exception const & e )
    {
        LOG_ERROR( "release_power failed: " << e.what() );
    }
    catch( ... )
    {
        LOG_ERROR( "release_power failed" );
    }
}

stream_profiles uvc_sensor::init_stream_profiles()
{
    std::unordered_set< std::shared_ptr< video_stream_profile > > video_profiles;
    // D457 development - only via mipi imu frames com from uvc instead of hid
    std::unordered_set< std::shared_ptr< motion_stream_profile > > motion_profiles;
    power on( std::dynamic_pointer_cast< uvc_sensor >( shared_from_this() ) );

    auto uvc_profiles = _device->get_profiles();
    for( auto && p : uvc_profiles )
    {
        const auto && rs2_fmt = fourcc_to_rs2_format( p.format );
        if( rs2_fmt == RS2_FORMAT_ANY )
            continue;

        // D457 development
        if( rs2_fmt == RS2_FORMAT_MOTION_XYZ32F )
        {
            auto profile = std::make_shared< platform::stream_profile_impl< motion_stream_profile > >( p );
            if( ! profile )
                throw librealsense::invalid_value_exception( "null pointer passed for argument \"profile\"." );

            profile->set_stream_type( fourcc_to_rs2_stream( p.format ) );
            profile->set_stream_index( 0 );
            profile->set_format( rs2_fmt );
            profile->set_framerate( p.fps );
            motion_profiles.insert( profile );
        }
        else
        {
            auto profile = std::make_shared< platform::stream_profile_impl< video_stream_profile > >( p );
            if( ! profile )
                throw librealsense::invalid_value_exception( "null pointer passed for argument \"profile\"." );

            profile->set_dims( p.width, p.height );
            profile->set_stream_type( fourcc_to_rs2_stream( p.format ) );
            profile->set_stream_index( 0 );
            profile->set_format( rs2_fmt );
            profile->set_framerate( p.fps );
            video_profiles.insert( profile );
        }
    }

    stream_profiles result{ video_profiles.begin(), video_profiles.end() };
    result.insert( result.end(), motion_profiles.begin(), motion_profiles.end() );
    return result;
}


}  // namespace librealsense
