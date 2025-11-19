// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#pragma once

#include <core/serialization.h>
#include <string>
#include <memory>
#include <map>
#include <set>

// Add required core interfaces used by this writer
#include <src/core/info-interface.h>
#include <src/core/options-interface.h>

// rosbag2 storage headers (vendored third-party)
#include <rosbag2_storage/serialized_bag_message.hpp>
#include <rosbag2_storage/storage_interfaces/read_write_interface.hpp>
#include <rosbag2_storage/topic_metadata.hpp>
#include <rosbag2_storage/storage_factory.hpp>   // added for internal storage creation
#include <rosbag2_storage/storage_options.hpp>  // for storage options struct

#include <media/ros/ros_file_format.h> // reuse ros_topic naming + helpers
#include <src/core/frame-interface.h>
#include <src/core/stream-profile-interface.h>
#include <src/option.h>

namespace librealsense
{
    // Minimal ROS2 bag (rosbag2) writer adapter.
    // Updated ctor: user supplies file path (and optional storage id) instead of a pre-created storage instance.
    class ros2_writer : public device_serializer::writer
    {
    public:
        explicit ros2_writer( const std::string & file_path, bool enable_compression, const std::string & storage_id = "sqlite3" );

        void write_device_description( const librealsense::device_serializer::device_snapshot & ) override;
        void write_frame( const device_serializer::stream_identifier & stream_id,
                          const device_serializer::nanoseconds & ts,
                          frame_holder && frame ) override;
        void write_snapshot( uint32_t device_index,
                             const device_serializer::nanoseconds & ts,
                             rs2_extension type,
                             const std::shared_ptr< extension_snapshot > & snapshot ) override;
        void write_snapshot( const device_serializer::sensor_identifier & sensor_id,
                             const device_serializer::nanoseconds & ts,
                             rs2_extension type,
                             const std::shared_ptr< extension_snapshot > & snapshot ) override;
        void write_notification( const device_serializer::sensor_identifier & sensor_id,
                                 const device_serializer::nanoseconds & ts,
                                 const notification & n ) override;
        const std::string & get_file_name() const override;

    private:
        // Generic write of an opaque string payload (utf8)
        void write_string( std::string const & topic, const device_serializer::nanoseconds & ts, std::string const & payload );
        void ensure_topic( const std::string & name, const std::string & type );
        std::string make_frame_topic( const device_serializer::stream_identifier & id ) const;

        // Device / sensor description helpers
        void write_vendor_info( std::string const & topic, const device_serializer::nanoseconds & ts, std::shared_ptr< info_interface > info );
        void write_sensor_option( device_serializer::sensor_identifier const & sid, const device_serializer::nanoseconds & ts, rs2_option opt, const librealsense::option & o );
        void write_sensor_options( device_serializer::sensor_identifier const & sid, const device_serializer::nanoseconds & ts, std::shared_ptr< options_interface > opts );
        void write_extension_snapshot( uint32_t device_id, uint32_t sensor_index, const device_serializer::nanoseconds & ts, rs2_extension type, std::shared_ptr< librealsense::extension_snapshot > snapshot, bool is_device );

        template< rs2_extension E >
        std::shared_ptr< typename ExtensionToType< E >::type > snapshot_as( std::shared_ptr< librealsense::extension_snapshot > s )
        {
            auto as_type = As< typename ExtensionToType< E >::type >( s );
            if( ! as_type )
                throw invalid_value_exception( rsutils::string::from() << "Failed to cast snapshot to extension " << E );
            return as_type;
        }

        std::shared_ptr< rosbag2_storage::storage_interfaces::ReadWriteInterface > _storage;
        std::string _file;
        std::map< std::string, rosbag2_storage::TopicMetadata > _topics; // created topics cache
        std::map< uint32_t, std::set< rs2_option > > _written_option_desc; // sensor_index -> option ids written 
    };
}
