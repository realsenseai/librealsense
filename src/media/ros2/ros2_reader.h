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
    using namespace device_serializer;
    class context;

    class ros2_reader : public reader
    {
    public:
        ros2_reader(const std::string& file_path, const std::shared_ptr<context>& ctx);
        virtual ~ros2_reader() = default;

        // Interface Implementations
        device_snapshot query_device_description(const nanoseconds& time) override;
        std::shared_ptr< serialized_data > read_next_data() override;
        void seek_to_time(const nanoseconds& time) override;
        nanoseconds query_duration() const override;
        void reset() override;
        void enable_stream(const std::vector<stream_identifier>& stream_ids) override;
        void disable_stream(const std::vector<stream_identifier>& stream_ids) override;
        const std::string& get_file_name() const override;
        std::vector<std::shared_ptr<serialized_data>> fetch_last_frames(const nanoseconds& seek_time) override;

    private:
        // Helper to parse "key=value;key2=val2" format used by writer
        std::map< std::string, std::string > parse_key_value_string(const std::string& payload) const;
        std::map< std::string, std::string > parse_msg_payload(const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg) const;

        // Topic parsing helpers
        bool is_stream_topic(const std::string& topic, stream_identifier& id) const;
        bool is_option_topic(const std::string& topic, sensor_identifier& sid, rs2_option& opt) const;
        std::shared_ptr<info_container> read_info_snapshot(const std::string& topic) const;
        stream_profiles read_stream_info(uint32_t device_index, uint32_t sensor_index);
        std::set<uint32_t> read_sensor_indices(uint32_t device_index);

        // Stream profile parsing helpers
        rs2_motion_device_intrinsic parse_motion_intrinsics(const std::map<std::string, std::string>& kv) const;
        rs2_intrinsics parse_video_intrinsics(const std::map<std::string, std::string>& kv) const;
        std::shared_ptr<motion_stream_profile> create_motion_profile(const stream_identifier& stream_id, rs2_format format,
            uint32_t fps, const std::map<std::string, std::string>& intrinsics_kv) const;
        std::shared_ptr<video_stream_profile> create_video_profile(const stream_identifier& stream_id, rs2_format format,
            uint32_t fps, const std::map<std::string, std::string>& intrinsics_kv) const;

        // Frame setup helpers
        void read_frame_metadata(const stream_identifier& sid, int64_t timestamp, frame_additional_data& additional_data) const;
        void setup_video_frame(frame_interface* frame_ptr, const stream_identifier& sid) const;
        void setup_motion_frame(frame_interface* frame_ptr, const stream_identifier& sid) const;
        static rs2_extension get_frame_extension(rs2_stream stream_type);
        
        std::shared_ptr< serialized_data > read_frame_data(const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg, const stream_identifier& sid);
        frame_holder allocate_frame(const stream_identifier& sid, const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg, const frame_additional_data& additional_data);

        std::shared_ptr< rosbag2_storage::storage_interfaces::ReadWriteInterface > _storage;
        std::string _file_path;
        std::vector< rosbag2_storage::TopicMetadata > _topics_cache;
        std::shared_ptr<context> _context;

        // State management
        device_snapshot _initial_snapshot;
        bool _initialized = false;
        std::set< stream_identifier > _enabled_streams;

        // Cache to support fetch_last_frames logic
        // Maps stream ID to the last frame data seen
        std::map< stream_identifier, std::shared_ptr<serialized_data> > _last_frame_cache;

        // Frame source for allocating frames
        std::shared_ptr<frame_source> _frame_source;
    };
}