// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#pragma once
#include <core/serialization.h>
#include <rosbag2_storage/storage_interfaces/read_only_interface.hpp>
#include <rosbag2_storage/serialized_bag_message.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

#include <media/ros/ros_file_format.h>
#include <src/core/frame-interface.h>
#include <src/core/stream-profile-interface.h>
#include <src/option.h>

namespace librealsense {

class frame_source; // forward declaration

class ros2_reader : public device_serializer::reader
{
public:
    explicit ros2_reader( std::shared_ptr< rosbag2_storage::storage_interfaces::ReadOnlyInterface > storage );

    device_serializer::device_snapshot query_device_description( const device_serializer::nanoseconds & ) override;
    std::shared_ptr< device_serializer::serialized_data > read_next_data() override;
    void seek_to_time( const device_serializer::nanoseconds & ) override; // linear seek
    device_serializer::nanoseconds query_duration() const override { return _duration; }
    void reset() override; // rewind to beginning
    void enable_stream( const std::vector< device_serializer::stream_identifier > & ) override {}
    void disable_stream( const std::vector< device_serializer::stream_identifier > & ) override {}
    const std::string & get_file_name() const override { return _file; }
    std::vector< std::shared_ptr< device_serializer::serialized_data > > fetch_last_frames( const device_serializer::nanoseconds & ) override { return {}; }

private:
    std::shared_ptr< device_serializer::serialized_data > parse_message( const rosbag2_storage::SerializedBagMessage & );
    std::shared_ptr< device_serializer::serialized_data > parse_frame( std::string const & topic, const rosbag2_storage::SerializedBagMessage & );
    std::shared_ptr< device_serializer::serialized_data > parse_option( std::string const & topic, const rosbag2_storage::SerializedBagMessage & );
    std::shared_ptr< device_serializer::serialized_data > parse_notification( std::string const & topic, const rosbag2_storage::SerializedBagMessage & );

    device_serializer::nanoseconds _duration{};
    std::shared_ptr< rosbag2_storage::storage_interfaces::ReadOnlyInterface > _storage;
    std::string _file;
    bool _initialized = false;
    std::vector< rosbag2_storage::SerializedBagMessage > _messages; // in-memory list (simple implementation) 
    size_t _cursor = 0;
};

}
