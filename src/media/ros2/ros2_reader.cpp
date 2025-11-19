// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#include "ros2_reader.h"
#include <rsutils/string/from.h>
#include <algorithm>
#include <cstring>
#include <src/source.h> // for frame_source & stream_to_frame_types

namespace librealsense {
using namespace device_serializer;

static std::string to_string_buffer( const rosbag2_storage::SerializedBagMessage & msg )
{
    if( ! msg.serialized_data || ! msg.serialized_data->buffer ) return {};
    return std::string( reinterpret_cast< char * >( msg.serialized_data->buffer ), msg.serialized_data->buffer_length );
}

ros2_reader::ros2_reader( std::shared_ptr< rosbag2_storage::storage_interfaces::ReadOnlyInterface > storage )
    : _storage( std::move( storage ) )
{
    if( _storage )
        _file = _storage->get_relative_file_path();
    reset();
}

device_snapshot ros2_reader::query_device_description( const nanoseconds & )
{
    // Minimal empty snapshot; advanced device description reconstruction can be added later
    return device_snapshot();
}

void ros2_reader::reset()
{
    _messages.clear();
    _cursor = 0;
    if( ! _storage ) return;
    while( _storage->has_next() )
    {
        auto m = _storage->read_next();
        _messages.push_back( *m );
    }
    if( ! _messages.empty() )
    {
        auto first_ts = _messages.front().time_stamp;
        auto last_ts = _messages.back().time_stamp;
        if( last_ts > first_ts )
            _duration = nanoseconds( static_cast< uint64_t >( last_ts - first_ts ) );
    }
    _initialized = true;
}

void ros2_reader::seek_to_time( const nanoseconds & t )
{
    if( _messages.empty() ) return;
    int64_t target = static_cast< int64_t >( t.count() );
    _cursor = 0;
    while( _cursor < _messages.size() && _messages[ _cursor ].time_stamp < target )
        ++_cursor;
}

std::shared_ptr< serialized_data > ros2_reader::parse_frame( std::string const & topic, const rosbag2_storage::SerializedBagMessage & msg )
{
    auto sid = ros_topic::get_stream_identifier( topic );
    frame_additional_data add{};
    add.timestamp = double( msg.time_stamp ) / 1e6; // ns -> ms
    add.frame_number = 0; // will try to parse later from metadata message if present
    add.fisheye_ae_mode = false;

    auto size = msg.serialized_data->buffer_length;
    // allocate frame
    frame_source fs( 32 );
    fs.init( nullptr );
    frame_interface * fi = fs.alloc_frame( { sid.stream_type, sid.stream_index, librealsense::frame_source::stream_to_frame_types( sid.stream_type ) }, size, std::move( add ), true );
    if( ! fi )
        return std::make_shared< serialized_invalid_frame >( nanoseconds( msg.time_stamp ), sid );
    std::memcpy( const_cast< uint8_t * >( fi->get_frame_data() ), msg.serialized_data->buffer, size );
    frame_holder fh{ fi };
    return std::make_shared< serialized_frame >( nanoseconds( msg.time_stamp ), sid, std::move( fh ) );
}

std::shared_ptr< serialized_data > ros2_reader::parse_option( std::string const & topic, const rosbag2_storage::SerializedBagMessage & msg )
{
    auto sid = ros_topic::get_sensor_identifier( topic );
    std::string payload = to_string_buffer( msg );
    float value = 0.f;
    try { value = std::stof( payload ); } catch(...) {}
    // option id from topic name
    rs2_option opt_id = RS2_OPTION_COUNT;
    try
    {
        std::string opt_name = ros_topic::get_option_name( topic );
        std::replace( opt_name.begin(), opt_name.end(), '_', ' ' );
        if( ! try_parse( opt_name, opt_id ) )
            return nullptr;
    }
    catch(...) { return nullptr; }

    auto option = std::make_shared< const_value_option >( "Recorded option", value );
    return std::make_shared< serialized_option >( nanoseconds( msg.time_stamp ), sid, opt_id, option );
}

std::shared_ptr< serialized_data > ros2_reader::parse_notification( std::string const & topic, const rosbag2_storage::SerializedBagMessage & msg )
{
    auto sid = ros_topic::get_sensor_identifier( topic );
    std::string payload = to_string_buffer( msg );
    // very simple parse key=value; pairs (only category/severity/description/timestamp extracted)
    rs2_notification_category category = RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR;
    rs2_log_severity severity = RS2_LOG_SEVERITY_INFO;
    std::string description;
    uint64_t ts_val = msg.time_stamp;
    size_t pos = 0;
    while( pos < payload.size() )
    {
        auto semi = payload.find( ';', pos );
        std::string part = payload.substr( pos, semi == std::string::npos ? std::string::npos : semi - pos );
        auto eq = part.find( '=' );
        if( eq != std::string::npos )
        {
            std::string key = part.substr( 0, eq );
            std::string val = part.substr( eq + 1 );
            if( key == "category" ) try_parse( val, category );
            else if( key == "severity" ) try_parse( val, severity );
            else if( key == "description" ) description = val;
            else if( key == "timestamp" ) { try { ts_val = std::stoull( val ); } catch(...) {} }
        }
        if( semi == std::string::npos ) break;
        pos = semi + 1;
    }
    notification n( category, 0, severity, description );
    n.timestamp = ts_val;
    return std::make_shared< serialized_notification >( nanoseconds( msg.time_stamp ), sid, n );
}

std::shared_ptr< serialized_data > ros2_reader::parse_message( const rosbag2_storage::SerializedBagMessage & msg )
{
    std::string topic = msg.topic_name;
    if( topic.find( "/option/" ) != std::string::npos && topic.find( "/value" ) != std::string::npos )
        return parse_option( topic, msg );
    if( topic.find( "/notification/" ) != std::string::npos )
        return parse_notification( topic, msg );

    // frame data topics end with /data and contain raw bytes
    if( topic.find( "/data" ) != std::string::npos && topic.find( "/tf" ) == std::string::npos && topic.find( "/metadata" ) == std::string::npos )
        return parse_frame( topic, msg );

    return nullptr; // ignore others
}

std::shared_ptr< serialized_data > ros2_reader::read_next_data()
{
    if( _cursor >= _messages.size() )
        return std::make_shared< serialized_end_of_file >();
    while( _cursor < _messages.size() )
    {
        auto & m = _messages[ _cursor++ ];
        auto data = parse_message( m );
        if( data ) return data;
    }
    return std::make_shared< serialized_end_of_file >();
}

} // namespace librealsense
