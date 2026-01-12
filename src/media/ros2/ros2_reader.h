// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#pragma once

#include <core/serialization.h>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <set>

// Reuse core interfaces
#include <src/core/info-interface.h>
#include <src/core/options-interface.h>
#include <src/core/frame-interface.h>
#include <src/source.h>

// rosbag2 storage headers
#include <rosbag2_storage/storage_interfaces/read_write_interface.hpp>
#include <rosbag2_storage/serialized_bag_message.hpp>
#include <rosbag2_storage/topic_metadata.hpp>

#include <media/ros/ros_file_format.h> // helpers for topic names
#include <src/core/info.h>

namespace librealsense
{
    class context;

    class ros2_reader : public device_serializer::reader
    {
    public:
        ros2_reader(const std::string& file_path, const std::shared_ptr<context>& ctx);
        virtual ~ros2_reader() = default;

        // Interface Implementations
        device_serializer::device_snapshot query_device_description(const device_serializer::nanoseconds& time) override;
        std::shared_ptr< device_serializer::serialized_data > read_next_data() override;
        void seek_to_time(const device_serializer::nanoseconds& time) override;
        device_serializer::nanoseconds query_duration() const override;
        void reset() override;
        void enable_stream(const std::vector<device_serializer::stream_identifier>& stream_ids) override;
        void disable_stream(const std::vector<device_serializer::stream_identifier>& stream_ids) override;
        const std::string& get_file_name() const override;
        std::vector<std::shared_ptr<device_serializer::serialized_data>> fetch_last_frames(const device_serializer::nanoseconds& seek_time) override;

    private:
        // Helper to parse "key=value;key2=val2" format used by writer
        std::map< std::string, std::string > parse_key_value_string(const std::string& payload) const;

        // Topic parsing helpers
        bool is_stream_topic(const std::string& topic, device_serializer::stream_identifier& id) const;
        bool is_option_topic(const std::string& topic, device_serializer::sensor_identifier& sid, rs2_option& opt) const;
        std::shared_ptr<info_container> read_info_snapshot(const std::string& topic) const;
        stream_profiles read_stream_info(uint32_t device_index, uint32_t sensor_index);
        std::set<uint32_t> read_sensor_indices(uint32_t device_index);

        std::shared_ptr< rosbag2_storage::storage_interfaces::ReadWriteInterface > _storage;
        std::string _file_path;
        std::vector< rosbag2_storage::TopicMetadata > _topics_cache;
        std::shared_ptr<context> _context;

        // State management
        device_serializer::device_snapshot _initial_snapshot;
        bool _initialized = false;
        std::set< device_serializer::stream_identifier > _enabled_streams;

        // Cache to support fetch_last_frames logic
        // Maps stream ID to the last frame data seen
        std::map< device_serializer::stream_identifier, std::shared_ptr<device_serializer::serialized_data> > _last_frame_cache;

        // Frame source for allocating frames
        std::shared_ptr<frame_source> _frame_source;
    };
}