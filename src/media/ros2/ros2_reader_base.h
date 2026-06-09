// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once
#include <rosbag2_storage/serialized_bag_message.hpp>
#include <rosbag2_storage/topic_metadata.hpp>
#include <rosbag2_storage_default_plugins/sqlite/sqlite_storage.hpp>
#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>

#include "ros2_file_format.h"


namespace librealsense
{
    using namespace device_serializer;

    class context;
    class frame_source;

    class ros2_reader_base : public reader
    {
    public:
        ros2_reader_base(const std::string& file, const std::shared_ptr<context> ctx);

        device_snapshot query_device_description(const nanoseconds& time) override;
        nanoseconds query_duration() const override;
        void reset() override;
        void seek_to_time(const nanoseconds& seek_time) override;
        virtual void enable_stream(const std::vector<device_serializer::stream_identifier>& stream_ids) override;
        virtual void disable_stream(const std::vector<device_serializer::stream_identifier>& stream_ids) override;
        std::vector<std::shared_ptr<serialized_data>> fetch_last_frames(const nanoseconds& seek_time) override;
        const std::string& get_file_name() const override;

        static bool is_zstd_compressed(const uint8_t* src, size_t src_size);
        static void decompress_if_needed(std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg);

        template<typename T>
        static T deserialize_message(const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg)
        {
            if (!msg || !msg->serialized_data || !msg->serialized_data->buffer || msg->serialized_data->buffer_length == 0)
                throw std::runtime_error("Invalid message for deserialize, expected non-empty payload");
            eprosima::fastcdr::FastBuffer fb(
                reinterpret_cast<char*>(msg->serialized_data->buffer),
                msg->serialized_data->buffer_length);
            eprosima::fastcdr::Cdr cdr(fb, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::Cdr::DDS_CDR);
            cdr.read_encapsulation();
            T data{};
            data.deserialize(cdr);
            return data;
        }

    protected:
        nanoseconds get_file_duration();

        std::shared_ptr<rosbag2_storage_plugins::SqliteStorage> as_sqlite_storage();

        virtual device_snapshot read_device_description(const nanoseconds& time, bool reset = false) = 0;

        // Each subclass MUST implement all three topic getters (empty list if a
        // category doesn't apply) so omissions are intentional and visible.
        void prepare_for_streaming();
        virtual std::vector<std::string> get_stream_topics() const = 0;
        virtual std::vector<std::string> get_option_topics() const = 0;
        virtual std::vector<std::string> get_notification_topics() const = 0;

        // Lookahead cache. Both readers' read_next_data go through here; ros2_reader also uses peek
        // for metadata-follows-data.
        bool                                                  has_next_cached() const;
        std::shared_ptr<rosbag2_storage::SerializedBagMessage> read_next_cached();
        std::shared_ptr<rosbag2_storage::SerializedBagMessage> peek_next_cached();

        // Allocate a frame from `m_frame_source`, move `data` into it, and call
        // `setup_frame` for stream-specific metadata (dimensions for video frames).
        frame_holder alloc_and_move_frame(std::vector<uint8_t>&& data,
            const stream_identifier& stream_id, frame_additional_data additional_data) const;

        // Subclass-specific: associate the frame with its stream profile and (for video
        // frames) set width/height/stride/bpp. The two readers locate the profile
        // differently (device snapshot scan vs. cached map), hence pure virtual.
        virtual void setup_frame(frame_interface* frame_ptr, const stream_identifier& sid) const = 0;

        // True if `topic` carries frame data (not metadata/option/notification); fills `sid`.
        virtual bool is_stream_topic(const std::string& topic, stream_identifier& sid) const = 0;
        virtual std::shared_ptr<serialized_frame> create_frame(
            const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg) = 0;

        std::shared_ptr<rosbag2_storage::storage_interfaces::ReadWriteInterface> _storage;
        std::shared_ptr<metadata_parser_map>          m_metadata_parser_map;
        device_snapshot                               m_initial_device_description;
        nanoseconds                                   m_total_duration;
        std::string                                   m_file_path;
        std::shared_ptr<frame_source>                 m_frame_source;
        std::vector<rosbag2_storage::TopicMetadata>   _topics_cache;
        std::shared_ptr<context>                      m_context;

        bool                                          _initialized = false;
        std::set<stream_identifier>                   _enabled_streams;

        // Filter applied to `_storage` for streaming (data + option + notification topics
        // for ros2_reader; image + IMU stream topics for native). Re-applied on reset().
        std::vector<std::string>                      _streaming_filter_topics;

        std::shared_ptr<rosbag2_storage::SerializedBagMessage> _cached_message;
        bool                                                  _cache_valid = false;

        int64_t                                               _first_timestamp_ns = 0;
    };
}
