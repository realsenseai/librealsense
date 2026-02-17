// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023-4 RealSense, Inc. All Rights Reserved.

#include "hid-sensor.h"
#include "device.h"
#include "stream.h"
#include "image.h"
#include "global_timestamp_reader.h"
#include "metadata.h"
#include "platform/stream-profile-impl.h"
#include <src/metadata-parser.h>
#include <src/core/time-service.h>

#include <rsutils/type/fourcc.h>
using rsutils::type::fourcc;


namespace librealsense {


static const std::map< rs2_stream, fourcc::value_type > stream_and_fourcc
    = { { RS2_STREAM_GYRO,  fourcc( 'G', 'Y', 'R', 'O' ) },
        { RS2_STREAM_ACCEL, fourcc( 'A', 'C', 'C', 'L' ) },
        { RS2_STREAM_GPIO,  fourcc( 'G', 'P', 'I', 'O' ) } };

/*For gyro sensitivity - FW gets 0 for 61 millidegree/s/LSB resolution
 0.1 for 30.5 millidegree/s/LSB 
 0.2 for 15.3 millidegree/s/LSB 
 0.3 for 7.6 millidegree/s/LSB 
 0.4 for 3.8 millidegree/s/LSB 
 Currently it is intended for D400 devices, when this feature will be added to D500 the convert needs to be checked*/
static const std::map< float, double > gyro_sensitivity_convert
    = { { 0.0f, 0 }, { 1.0f, 0.1 }, { 2.0f, 0.2 }, { 3.0f, 0.3 }, { 4.0f, 0.4 } };

    // in sensor.cpp
void log_callback_end( uint32_t fps,
                       rs2_time_t callback_start_time,
                       rs2_time_t callback_end_time,
                       rs2_stream stream_type,
                       unsigned long long frame_number );


hid_sensor::hid_sensor(
    std::shared_ptr< platform::hid_device > hid_device,
    std::unique_ptr< frame_timestamp_reader > hid_iio_timestamp_reader,
    std::unique_ptr< frame_timestamp_reader > custom_hid_timestamp_reader,
    const std::map< rs2_stream, std::map< unsigned, unsigned > > & fps_and_sampling_frequency_per_rs2_stream,
    const std::vector< std::pair< std::string, stream_profile > > & sensor_name_and_hid_profiles,
    device * dev )
    : super( "Raw Motion Module", dev )
    , _sensor_name_and_hid_profiles( sensor_name_and_hid_profiles )
    , _fps_and_sampling_frequency_per_rs2_stream( fps_and_sampling_frequency_per_rs2_stream )
    , _hid_device( std::move( hid_device ) )
    , _is_configured_stream( RS2_STREAM_COUNT )
    , _hid_iio_timestamp_reader( std::move( hid_iio_timestamp_reader ) )
    , _custom_hid_timestamp_reader( std::move( custom_hid_timestamp_reader ) )
{
    register_metadata( RS2_FRAME_METADATA_BACKEND_TIMESTAMP,
                       make_additional_data_parser( &frame_additional_data::backend_timestamp ) );

    std::map< std::string, uint32_t > frequency_per_sensor;
    for( auto && elem : sensor_name_and_hid_profiles )
        frequency_per_sensor.insert( make_pair( elem.first, elem.second.fps ) );

    std::vector< platform::hid_profile > profiles_vector;
    for( auto && elem : frequency_per_sensor )
        profiles_vector.push_back( platform::hid_profile{ elem.first, elem.second } );

    _hid_device->register_profiles( profiles_vector );
    for( auto && elem : _hid_device->get_sensors() )
        _hid_sensors.push_back( elem );
}

hid_sensor::~hid_sensor()
{
    try
    {
        if( _is_streaming )
            stop();

        if( _is_opened )
            close();
    }
    catch( ... )
    {
        LOG_ERROR( "An error has occurred while stop_streaming()!" );
    }
}

stream_profiles hid_sensor::get_sensor_profiles( std::string sensor_name ) const
{
    stream_profiles profiles{};
    for( auto && elem : _sensor_name_and_hid_profiles )
    {
        if( ! elem.first.compare( sensor_name ) )
        {
            auto && p = elem.second;
            platform::stream_profile sp{ 1, 1, p.fps, stream_to_fourcc( p.stream ) };
            auto profile = std::make_shared< platform::stream_profile_impl< motion_stream_profile > >( sp );
            profile->set_stream_index( p.index );
            profile->set_stream_type( p.stream );
            profile->set_format( p.format );
            profile->set_framerate( p.fps );
            profiles.push_back( profile );
        }
    }

    return profiles;
}

void hid_sensor::open( const stream_profiles & requests )
{
    std::lock_guard< std::mutex > lock( _configure_lock );
    if( _is_streaming )
        throw wrong_api_call_sequence_exception( "open(...) failed. Hid device is streaming!" );
    else if( _is_opened )
        throw wrong_api_call_sequence_exception( "Hid device is already opened!" );

    std::vector< platform::hid_profile > configured_hid_profiles;
    for( auto && request : requests )
    {
        auto && sensor_name = rs2_stream_to_sensor_name( request->get_stream_type() );
        _configured_profiles.insert( std::make_pair( sensor_name, request ) );
        _is_configured_stream[request->get_stream_type()] = true;
        configured_hid_profiles.push_back( platform::hid_profile{
            std::move( sensor_name ),
            fps_to_sampling_frequency(request->get_stream_type(), request->get_framerate()),
            get_imu_sensitivity_values( request->get_stream_type() ) } );
    }
    _hid_device->open( configured_hid_profiles );
    if( Is< librealsense::global_time_interface >( _owner ) )
    {
        As< librealsense::global_time_interface >( _owner )->enable_time_diff_keeper( true );
    }
    _is_opened = true;
    set_active_streams( requests );
}

void hid_sensor::close()
{
    if( _is_streaming )
        throw wrong_api_call_sequence_exception( "close() failed. Hid device is streaming!" );
    else if( ! _is_opened )
        throw wrong_api_call_sequence_exception( "close() failed. Hid device was not opened!" );

    _hid_device->close();
    {
        std::lock_guard< std::mutex > lock( _configure_lock );
        _configured_profiles.clear();
        _is_configured_stream.clear();
        _is_configured_stream.assign(RS2_STREAM_COUNT, false);
    }
    _is_opened = false;
    if( Is< librealsense::global_time_interface >( _owner ) )
    {
        As< librealsense::global_time_interface >( _owner )->enable_time_diff_keeper( false );
    }
    set_active_streams( {} );
}

// TODO:
static rs2_stream custom_gpio_to_stream_type( uint32_t custom_gpio )
{
    if( custom_gpio < 4 )
    {
        return static_cast< rs2_stream >( RS2_STREAM_GPIO );
    }
#ifndef __APPLE__
    // TODO to be refactored/tested
    LOG_ERROR( "custom_gpio " << std::to_string( custom_gpio ) << " is incorrect!" );
#endif
    return RS2_STREAM_ANY;
}

void hid_sensor::start( rs2_frame_callback_sptr callback )
{
    std::lock_guard< std::mutex > lock( _configure_lock );
    if( _is_streaming )
        throw wrong_api_call_sequence_exception( "start_streaming(...) failed. Hid device is already streaming!" );
    else if( ! _is_opened )
        throw wrong_api_call_sequence_exception( "start_streaming(...) failed. Hid device was not opened!" );

    _source.set_callback( std::move( callback ) );
    _source.init( _metadata_parsers );
    _source.set_sensor( _source_owner->shared_from_this() );

    unsigned long long last_frame_number = 0;
    rs2_time_t last_timestamp = 0;
    raise_on_before_streaming_changes( true );  // Required to be just before actual start allow recording to work

    _hid_device->start_capture(
        [this, last_frame_number, last_timestamp]( const platform::sensor_data & sensor_data ) mutable
        {
            const auto system_time = time_service::get_time();  // time frame was received from the backend
            auto timestamp_reader = _hid_iio_timestamp_reader.get();
            static const std::string custom_sensor_name = "custom";
            auto && sensor_name = sensor_data.sensor.name;
            std::shared_ptr< stream_profile_interface > request;
            {
                std::lock_guard< std::mutex > lock( _configure_lock );
                request = _configured_profiles[sensor_name];
            }
            bool is_custom_sensor = false;
            static const uint32_t custom_source_id_offset = 16;
            uint8_t custom_gpio = 0;
            auto custom_stream_type = RS2_STREAM_ANY;
            if( sensor_name == custom_sensor_name )
            {
                custom_gpio = *(
                    reinterpret_cast< uint8_t * >( (uint8_t *)( sensor_data.fo.pixels ) + custom_source_id_offset ) );
                custom_stream_type = custom_gpio_to_stream_type( custom_gpio );

                if( ! _is_configured_stream[custom_stream_type] )
                {
                    LOG_DEBUG( "Unrequested " << rs2_stream_to_string( custom_stream_type ) << " frame was dropped." );
                    return;
                }

                is_custom_sensor = true;
                timestamp_reader = _custom_hid_timestamp_reader.get();
            }

            if( ! this->is_streaming() )
            {
                auto stream_type = request->get_stream_type();
                LOG_INFO( "HID Frame received when Streaming is not active," << get_string( stream_type ) << ",Arrived,"
                                                                             << std::fixed << system_time );
                return;
            }

            const auto && fr = generate_frame_from_data( sensor_data.fo,
                                                         system_time,
                                                         timestamp_reader,
                                                         last_timestamp,
                                                         last_frame_number,
                                                         request );
            auto && frame_counter = fr->additional_data.frame_number;
            const auto && timestamp_domain = timestamp_reader->get_frame_timestamp_domain( fr );
            auto && timestamp = fr->additional_data.timestamp;
            const auto && bpp = get_image_bpp( request->get_format() );
            auto && data_size = sensor_data.fo.frame_size;

            LOG_DEBUG( "FrameAccepted," << get_string( request->get_stream_type() ) << ",Counter," << std::dec
                                        << frame_counter << ",Index,0"
                                        << ",BackEndTS," << std::fixed << sensor_data.fo.backend_time << ",SystemTime,"
                                        << std::fixed << system_time << " ,diff_ts[Sys-BE],"
                                        << system_time - sensor_data.fo.backend_time << ",TS," << std::fixed
                                        << timestamp << ",TS_Domain,"
                                        << rs2_timestamp_domain_to_string( timestamp_domain ) << ",last_frame_number,"
                                        << last_frame_number << ",last_timestamp," << last_timestamp );

            last_frame_number = frame_counter;
            last_timestamp = timestamp;
            frame_holder frame = _source.alloc_frame(
                { request->get_stream_type(), request->get_stream_index(), RS2_EXTENSION_MOTION_FRAME },
                data_size,
                std::move( fr->additional_data ),
                true );
            memcpy( (void *)frame->get_frame_data(),
                    sensor_data.fo.pixels,
                    sizeof( uint8_t ) * sensor_data.fo.frame_size );
            if( ! frame )
            {
                LOG_INFO( "Dropped frame. alloc_frame(...) returned nullptr" );
                return;
            }
            frame->set_stream( request );
            frame->set_timestamp_domain( timestamp_domain );

            // Gather info for logging the callback ended
            auto fps = frame->get_stream()->get_framerate();
            auto stream_type = frame->get_stream()->get_stream_type();
            auto frame_number = frame->get_frame_number();

            // Invoke first callback
            auto callback_start_time = time_service::get_time();
            auto callback = frame->get_owner()->begin_callback();
            _source.invoke_callback( std::move( frame ) );

            // Log callback ended
            log_callback_end( fps, callback_start_time, time_service::get_time(), stream_type, frame_number );
        } );
    _is_streaming = true;
}

void hid_sensor::stop()
{
    if( ! _is_streaming )
        throw wrong_api_call_sequence_exception( "stop_streaming() failed. Hid device is not streaming!" );

    _hid_device->stop_capture();
    _is_streaming = false;
    {
        std::lock_guard< std::mutex > lock( _configure_lock );
        _source.flush();
        _source.reset();
    }
    _hid_iio_timestamp_reader->reset();
    _custom_hid_timestamp_reader->reset();
    raise_on_before_streaming_changes( false );
}

std::vector< uint8_t > hid_sensor::get_custom_report_data( const std::string & custom_sensor_name,
                                                           const std::string & report_name,
                                                           platform::custom_sensor_report_field report_field ) const
{
    return _hid_device->get_custom_report_data( custom_sensor_name, report_name, report_field );
}

stream_profiles hid_sensor::init_stream_profiles()
{
    stream_profiles stream_requests;
    for( auto && it = _hid_sensors.rbegin(); it != _hid_sensors.rend(); ++it )
    {
        auto profiles = get_sensor_profiles( it->name );
        stream_requests.insert( stream_requests.end(), profiles.begin(), profiles.end() );
    }

    return stream_requests;
}

const std::string & hid_sensor::rs2_stream_to_sensor_name( rs2_stream stream ) const
{
    for( auto && elem : _sensor_name_and_hid_profiles )
    {
        if( stream == elem.second.stream )
            return elem.first;
    }
    throw invalid_value_exception( "rs2_stream not found!" );
}

uint32_t hid_sensor::stream_to_fourcc( rs2_stream stream ) const
{
    uint32_t fourcc;
    try
    {
        fourcc = stream_and_fourcc.at( stream );
    }
    catch( std::out_of_range )
    {
        throw invalid_value_exception( rsutils::string::from()
                                       << "fourcc of stream " << rs2_stream_to_string( stream ) << " not found!" );
    }

    return fourcc;
}

uint32_t hid_sensor::fps_to_sampling_frequency( rs2_stream stream, uint32_t fps ) const
{
    // TODO: Add log prints
    auto it = _fps_and_sampling_frequency_per_rs2_stream.find( stream );
    if( it == _fps_and_sampling_frequency_per_rs2_stream.end() )
        return fps;

    auto fps_mapping = it->second.find( fps );
    if( fps_mapping != it->second.end() )
        return fps_mapping->second;
    else
        return fps;
}
void hid_sensor::set_imu_sensitivity( rs2_stream stream, float value ) 
{
    _imu_sensitivity_per_rs2_stream[stream] = value;
}

void hid_sensor::set_gyro_scale_factor(double scale_factor) 
{
    _hid_device->set_gyro_scale_factor( scale_factor );
}

/*For gyro sensitivity - FW expects 0/0.1/0.2/0.3/0.4 we convert the values from the enum 0/1/2/3/4
the user chooses to the values FW expects using gyro_sensitivity_convert*/
double hid_sensor::get_imu_sensitivity_values( rs2_stream stream )
{
    if( _imu_sensitivity_per_rs2_stream.find( stream ) != _imu_sensitivity_per_rs2_stream.end() )
    {
        return gyro_sensitivity_convert.at( _imu_sensitivity_per_rs2_stream[stream] );
    }
    else
        //FW recieve 0.1 and adjusts the gyro's sensitivity to its default setting of 1000.
        //FW recieve 0.001 and adjusts the accel's sensitivity to its default setting of 4g.
        return stream == RS2_STREAM_GYRO ? 0.1f : 0.001f;
}

iio_hid_timestamp_reader::iio_hid_timestamp_reader()
{
    counter.resize( sensors );
    reset();
}

void iio_hid_timestamp_reader::reset()
{
    std::lock_guard< std::recursive_mutex > lock( _mtx );
    started = false;
    for( auto i = 0; i < sensors; ++i )
    {
        counter[i] = 0;
    }
}

rs2_time_t iio_hid_timestamp_reader::get_frame_timestamp( const std::shared_ptr< frame_interface > & frame )
{
    std::lock_guard< std::recursive_mutex > lock( _mtx );

    auto f = std::dynamic_pointer_cast< librealsense::frame >( frame );
    if( has_metadata( frame ) )
    {
        //  The timestamps conversions path comprise of:
        // FW TS (32bit) ->    USB Phy Layer (no changes)  -> Host Driver TS (Extend to 64bit) ->  LRS read as 64 bit
        // The flow introduces discrepancy with UVC stream which timestamps are not extended to 64 bit by host driver
        // both for Win and v4l backends. In order to allow for hw timestamp-based synchronization of Depth and IMU
        // streams the latter will be trimmed to 32 bit. To revert to the extended 64 bit TS uncomment the next line
        // instead
        // auto timestamp = *((uint64_t*)((const uint8_t*)fo.metadata));

        // The ternary operator is replaced by explicit assignment due to an issue with GCC for RaspberryPi that causes
        // segfauls in optimized build.
        auto timestamp = *( reinterpret_cast< uint32_t * >( f->additional_data.metadata_blob.data() ) );
        if( f->additional_data.metadata_size >= hid_header_size )
            timestamp = static_cast< uint32_t >(
                reinterpret_cast< const hid_header * >( f->additional_data.metadata_blob.data() )->timestamp );

        // HID timestamps are aligned to FW Default - usec units
        return static_cast< rs2_time_t >( timestamp * MICROSEC_TO_MILLISEC );
    }

    if( ! started )
    {
        LOG_WARNING( "HID timestamp not found, switching to Host timestamps." );
        started = true;
    }

    return std::chrono::duration< rs2_time_t, std::milli >( std::chrono::system_clock::now().time_since_epoch() )
        .count();
}

bool iio_hid_timestamp_reader::has_metadata( const std::shared_ptr< frame_interface > & frame ) const
{
    auto f = std::dynamic_pointer_cast< librealsense::frame >( frame );
    if( ! f )
        throw librealsense::invalid_value_exception( "null pointer recieved from dynamic pointer casting." );

    if( f->additional_data.metadata_size > 0 )
    {
        return true;
    }
    return false;
}

unsigned long long iio_hid_timestamp_reader::get_frame_counter( const std::shared_ptr< frame_interface > & frame ) const
{
    std::lock_guard< std::recursive_mutex > lock( _mtx );

    int index = 0;
    if( frame->get_stream()->get_stream_type() == RS2_STREAM_GYRO )
        index = 1;

    return ++counter[index];
}

rs2_timestamp_domain
iio_hid_timestamp_reader::get_frame_timestamp_domain( const std::shared_ptr< frame_interface > & frame ) const
{
    if( has_metadata( frame ) )
    {
        return RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK;
    }
    return RS2_TIMESTAMP_DOMAIN_SYSTEM_TIME;
}


}  // namespace librealsense
