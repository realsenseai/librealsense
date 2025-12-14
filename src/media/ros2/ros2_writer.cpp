// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#include "ros2_writer.h"
#include <rcutils/time.h>
#include <rsutils/string/from.h>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace librealsense {
using namespace device_serializer;

static rcutils_allocator_t make_simple_allocator()
{
    rcutils_allocator_t a{};
    a.allocate = [](size_t size, void * ) -> void * { return std::malloc(size); };
    a.deallocate = [](void * p, void * ) { std::free(p); };
    a.reallocate = [](void * p, size_t size, void * ) -> void * { return std::realloc(p, size); };
    a.zero_allocate = [](size_t n, size_t size, void * ) -> void * { return std::calloc(n, size); };
    a.state = nullptr;
    return a;
}


ros2_writer::ros2_writer( const std::string & file_path, bool enable_compression, const std::string & storage_id )
{
    rosbag2_storage::StorageOptions opts;
    opts.uri = file_path;
    opts.storage_id = storage_id;
    opts.max_bagfile_size = 0;
    opts.max_bagfile_duration = 0;
    opts.max_cache_size = 0;

    rosbag2_storage::StorageFactory factory;
    _storage = factory.open_read_write( file_path, storage_id );
    if( ! _storage )
        throw std::runtime_error( rsutils::string::from() << "Failed to open rosbag2 storage for uri '" << file_path
                                                          << "' using storage id '" << storage_id << "'" );

    _file = file_path.empty() ? _storage->get_relative_file_path() : file_path;

    ensure_topic( ros_topic::file_version_topic(), "librealsense/file_version" );
    write_string( ros_topic::file_version_topic(),
                  get_static_file_info_timestamp(),
                  std::to_string( get_file_version() ) );
}

void ros2_writer::ensure_topic( const std::string & name, const std::string & type )
{
    if( _topics.find( name ) != _topics.end() )
        return;
    rosbag2_storage::TopicMetadata md;
    md.name = name;
    md.type = type;
    md.serialization_format = "cdr"; // placeholder; we store raw bytes
    _storage->create_topic( md );
    _topics.emplace( name, md );
}

void ros2_writer::write_string( std::string const & topic, const nanoseconds & ts, std::string const & payload )
{
    ensure_topic( topic, "librealsense/string" );
    auto buffer = std::make_shared< rcutils_uint8_array_t >();
    buffer->buffer_length = payload.size();
    buffer->buffer_capacity = payload.size();
    buffer->allocator = make_simple_allocator();
    buffer->buffer = static_cast< uint8_t * >( std::malloc( payload.size() ) );
    if( ! buffer->buffer )
        throw std::runtime_error( "Failed to allocate rosbag2 string buffer" );
    std::memcpy( buffer->buffer, payload.data(), payload.size() );

    auto msg = std::make_shared< rosbag2_storage::SerializedBagMessage >();
    msg->serialized_data = buffer;
    msg->time_stamp = static_cast< rcutils_time_point_value_t >( ts.count() );
    msg->topic_name = topic;
    _storage->write( msg );
}

std::string ros2_writer::make_frame_topic( const stream_identifier & id ) const
{
    return ros_topic::frame_data_topic( id ); // keep same schema as ROS1
}

void ros2_writer::write_vendor_info( std::string const & topic, const nanoseconds & ts, std::shared_ptr< info_interface > info )
{
    if( ! info ) return;
    for( uint32_t i = 0; i < static_cast< uint32_t >( RS2_CAMERA_INFO_COUNT ); ++i )
    {
        auto ci = static_cast< rs2_camera_info >( i );
        if( info->supports_info( ci ) )
        {
            std::string kv = rsutils::string::from() << rs2_camera_info_to_string( ci ) << "=" << info->get_info( ci );
            write_string( topic, ts, kv );
        }
    }
}

void ros2_writer::write_device_description( const device_snapshot & dev )
{
    // Device-level extension snapshots (INFO only for now)
    auto device_exts = dev.get_device_extensions_snapshots();
    auto info_ext = device_exts.find( RS2_EXTENSION_INFO );
    auto info_api = As< info_interface >( info_ext );
    if( info_api )
        write_vendor_info( ros_topic::device_info_topic( get_device_index() ), get_static_file_info_timestamp(), info_api );

    // Sensors
    for( auto & sensor_snap : dev.get_sensors_snapshots() )
    {
        auto sensor_info_ext = sensor_snap.get_sensor_extensions_snapshots().find( RS2_EXTENSION_INFO );
        auto sensor_info_api = As< info_interface >( sensor_info_ext );
        if( sensor_info_api )
            write_vendor_info( ros_topic::sensor_info_topic( { get_device_index(), sensor_snap.get_sensor_index() } ), get_static_file_info_timestamp(), sensor_info_api );

        // Stream profiles info
        for( auto & sp : sensor_snap.get_stream_profiles() )
        {
            auto sid = stream_identifier{ get_device_index(), sensor_snap.get_sensor_index(), sp->get_stream_type(), static_cast< uint32_t >( sp->get_stream_index() ) };
            // StreamInfo: encoding + fps + is_recommended -> store as simple CSV
            std::string encoding = rs2_format_to_string( sp->get_format() );
            std::string stream_info_payload = rsutils::string::from() << "encoding=" << encoding << ";fps=" << sp->get_framerate() << ";recommended=" << ((sp->get_tag() & profile_tag::PROFILE_TAG_DEFAULT) ? 1 : 0);
            write_string( ros_topic::stream_info_topic( sid ), get_static_file_info_timestamp(), stream_info_payload );

            // Video / IMU intrinsics
            if( auto video = As< video_stream_profile_interface >( sp ) )
            {
                rs2_intrinsics intr{};
                try { intr = video->get_intrinsics(); } catch(...) {}
                std::ostringstream oss;
                oss << "width=" << video->get_width() << ";height=" << video->get_height()
                    << ";fx=" << intr.fx << ";fy=" << intr.fy << ";ppx=" << intr.ppx << ";ppy=" << intr.ppy;
                write_string( ros_topic::video_stream_info_topic( sid ), get_static_file_info_timestamp(), oss.str() );
            }
            else if( auto motion = As< motion_stream_profile_interface >( sp ) )
            {
                rs2_motion_device_intrinsic mi{}; try { mi = motion->get_intrinsics(); } catch(...) {}
                std::ostringstream oss;
                oss << "imu_bias=" << mi.bias_variances[0] << "," << mi.bias_variances[1] << "," << mi.bias_variances[2];
                write_string( ros_topic::imu_intrinsic_topic( sid ), get_static_file_info_timestamp(), oss.str() );
            }
        }

        // Options snapshot
        auto opts_ext = sensor_snap.get_sensor_extensions_snapshots().find( RS2_EXTENSION_OPTIONS );
        auto opts_api = As< options_interface >( opts_ext );
        if( opts_api )
            write_sensor_options( { get_device_index(), sensor_snap.get_sensor_index() }, get_static_file_info_timestamp(), opts_api );
    }
}

void ros2_writer::write_sensor_option( sensor_identifier const & sid, const nanoseconds & ts, rs2_option opt, const librealsense::option & o )
{
    float value = o.query();
    // value topic
    write_string( ros_topic::option_value_topic( sid, opt ), ts, std::to_string( value ) );
    // description topic written once
    if( _written_option_desc[ sid.sensor_index ].find( opt ) == _written_option_desc[ sid.sensor_index ].end() )
    {
        const char * desc = o.get_description();
        std::string description = desc ? std::string( desc ) : (rsutils::string::from() << "Read only option " << librealsense::get_string( opt ));
        write_string( ros_topic::option_description_topic( sid, opt ), get_static_file_info_timestamp(), description );
        _written_option_desc[ sid.sensor_index ].insert( opt );
    }
}

void ros2_writer::write_sensor_options( sensor_identifier const & sid, const nanoseconds & ts, std::shared_ptr< options_interface > opts )
{
    if( ! opts ) return;
    for( int i = 0; i < static_cast< int >( RS2_OPTION_COUNT ); ++i )
    {
        rs2_option opt = static_cast< rs2_option >( i );
        try
        {
            if( opts->supports_option( opt ) )
                write_sensor_option( sid, ts, opt, opts->get_option( opt ) );
        }
        catch( std::exception const & e )
        {
            LOG_WARNING( "Failed to write option " << opt << " : " << e.what() );
        }
    }
}

void ros2_writer::write_frame( const stream_identifier & stream_id,
                               const nanoseconds & ts,
                               frame_holder && frame )
{
    if( ! frame || ! frame.frame ) return;
    auto fi = frame.frame;
    auto topic = make_frame_topic( stream_id );
    ensure_topic( topic, "librealsense/raw_frame" );

    auto size = fi->get_frame_data_size();
    auto buffer = std::make_shared< rcutils_uint8_array_t >();
    buffer->buffer = static_cast< uint8_t * >( std::malloc( size ) );
    if( ! buffer->buffer )
        throw std::runtime_error( "Failed to allocate rosbag2 frame buffer" );
    std::memcpy( buffer->buffer, fi->get_frame_data(), size );
    buffer->buffer_length = size;
    buffer->buffer_capacity = size;
    buffer->allocator = make_simple_allocator();

    auto msg = std::make_shared< rosbag2_storage::SerializedBagMessage >();
    msg->serialized_data = buffer;
    msg->time_stamp = static_cast< rcutils_time_point_value_t >( ts.count() );
    msg->topic_name = topic;
    _storage->write( msg );

    // Minimal metadata: frame number + timestamp domain/system time
    std::string md_topic = ros_topic::frame_metadata_topic( stream_id );
    std::string md_payload = rsutils::string::from() << FRAME_NUMBER_MD_STR << "=" << fi->get_frame_number() << ";" << TIMESTAMP_DOMAIN_MD_STR << "=" << librealsense::get_string( fi->get_frame_timestamp_domain() ) << ";" << SYSTEM_TIME_MD_STR << "=" << fi->get_frame_system_time();
    write_string( md_topic, ts, md_payload );
}

void ros2_writer::write_snapshot( uint32_t device_index, const nanoseconds & ts, rs2_extension type, const std::shared_ptr< extension_snapshot > & snapshot )
{
    write_extension_snapshot( device_index, 0, ts, type, snapshot, true );
}

void ros2_writer::write_snapshot( const sensor_identifier & sensor_id, const nanoseconds & ts, rs2_extension type, const std::shared_ptr< extension_snapshot > & snapshot )
{
    write_extension_snapshot( sensor_id.device_index, sensor_id.sensor_index, ts, type, snapshot, false );
}

void ros2_writer::write_extension_snapshot( uint32_t device_id, uint32_t sensor_index, const nanoseconds & ts, rs2_extension type, std::shared_ptr< librealsense::extension_snapshot > snapshot, bool is_device )
{
    try
    {
        switch( type )
        {
        case RS2_EXTENSION_INFO:
        {
            auto info = snapshot_as< RS2_EXTENSION_INFO >( snapshot );
            if( is_device )
                write_vendor_info( ros_topic::device_info_topic( device_id ), ts, info );
            else
                write_vendor_info( ros_topic::sensor_info_topic( { device_id, sensor_index } ), ts, info );
            break;
        }
        case RS2_EXTENSION_OPTIONS:
        {
            auto options = snapshot_as< RS2_EXTENSION_OPTIONS >( snapshot );
            write_sensor_options( { device_id, sensor_index }, ts, options );
            break;
        }
        default:
            // Other snapshots ignored for now in minimal ROS2 writer 
            break;
        }
    }
    catch( std::exception const & e )
    {
        LOG_WARNING( "Failed to write extension snapshot: " << e.what() );
    }
}

void ros2_writer::write_notification( const sensor_identifier & sensor_id, const nanoseconds & ts, const notification & n )
{
    std::string topic = ros_topic::notification_topic( sensor_id, n.category );
    std::string payload = rsutils::string::from() << "category=" << rs2_notification_category_to_string( n.category )
                                                  << ";severity=" << rs2_log_severity_to_string( n.severity )
                                                  << ";description=" << n.description
                                                  << ";timestamp=" << n.timestamp
                                                  << ";data=" << n.serialized_data;
    write_string( topic, ts, payload );
}

const std::string & ros2_writer::get_file_name() const { return _file; }

} // namespace librealsense
