// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#pragma once

#include <core/serialization.h>

#include <rosbag2_storage/storage_interfaces/read_write_interface.hpp>
#include <rosbag2_storage/serialized_bag_message.hpp>
#include <rosbag2_storage/topic_metadata.hpp>
#include <rosbag2_storage_default_plugins/sqlite/sqlite_storage.hpp>

#include "ros2_file_format.h" // local ros2-native copy with ROS2 topic helpers

//#include <realdds/topics/ros2/sensor_msgs/msg/Image.h>
#include "ros2-msg-types/sensor_msgs/msg/Image.h"
//#include <realdds/topics/ros2/sensor_msgs/msg/Imu.h>
#include "ros2-msg-types/sensor_msgs/msg/Imu.h"
#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>

#include <rsutils/string/from.h>


namespace librealsense
{
    using namespace device_serializer;

    class context;
    class frame_source;
    class info_container;
    class options_interface;
    class options_container;
    class processing_block_interface;
    class recommended_proccesing_blocks_snapshot;

    class ros2_reader : public reader
    {
    public:
        ros2_reader(const std::string& file, const std::shared_ptr<context> ctx);
        device_snapshot query_device_description(const nanoseconds& time) override;
        std::shared_ptr<serialized_data> read_next_data() override;
        void seek_to_time(const nanoseconds& seek_time) override;
        std::vector<std::shared_ptr<serialized_data>> fetch_last_frames(const nanoseconds& seek_time) override;
        nanoseconds query_duration() const override;
        void reset() override;
        virtual void enable_stream(const std::vector<device_serializer::stream_identifier>& stream_ids) override;
        virtual void disable_stream(const std::vector<device_serializer::stream_identifier>& stream_ids) override;
        const std::string& get_file_name() const override;

    private:
        // We use a simple caching mechanism to have a lookahead functionality
        // needed in some cases to tell what is the next message without missing it
        bool has_next_cached() const;
        std::shared_ptr<rosbag2_storage::SerializedBagMessage> read_next_cached();
        std::shared_ptr<rosbag2_storage::SerializedBagMessage> peek_next_cached();

        static std::vector<std::string> split_string(const std::string& s, char delimiter);
        static std::string get_value(const std::map<std::string, std::string>& kv, const std::string& key);
        std::vector<std::string> filter_topics_by_regex(const std::regex& re) const;
        static std::map< std::string, std::string > parse_msg_payload(const std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg);
        static void register_camera_infos(std::shared_ptr<info_container> infos, const std::map<std::string, std::string>& kv);
        static std::string read_string(const std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg);

        nanoseconds get_file_duration();

        uint32_t read_file_version();
        bool try_read_stream_extrinsic(const stream_identifier& stream_id, uint32_t& group_id, rs2_extrinsics& extrinsic);
        std::shared_ptr<recommended_proccesing_blocks_snapshot> update_proccesing_blocks(uint32_t sensor_index, std::shared_ptr<options_container> sensor_options);
        void add_sensor_extension(snapshot_collection& sensor_extensions, const std::string& sensor_name);
       
        static bool is_depth_sensor(const std::string& sensor_name);
        static bool is_stereo_depth_sensor(const std::string& sensor_name);
        static bool is_color_sensor(const std::string& sensor_name);
        static bool is_motion_module_sensor(const std::string& sensor_name);
        static bool is_fisheye_module_sensor(const std::string& sensor_name);
        static bool is_safety_module_sensor(const std::string& sensor_name);
        static bool is_depth_mapping_sensor(const std::string& sensor_name);

        std::shared_ptr<recommended_proccesing_blocks_snapshot> read_proccesing_blocks(device_serializer::sensor_identifier sensor_id,
            std::shared_ptr<options_interface> options);
        device_snapshot read_device_description(const nanoseconds& time, bool reset = false);
        void prepare_for_streaming();

        // Topic parsing helpers
        static bool is_stream_topic(const std::string& topic, stream_identifier& id);
        std::string read_option_description(const uint32_t sensor_index, const rs2_option& id);
        std::shared_ptr<info_container> read_info_snapshot(const std::string& topic);
        std::shared_ptr<stream_profile_interface> read_next_stream_profile();
        std::set<uint32_t> read_sensor_indices(uint32_t device_index) const;

        // Stream profile parsing helpers
        rs2_motion_device_intrinsic parse_motion_intrinsics(const std::map<std::string, std::string>& kv) const;
        std::shared_ptr<motion_stream_profile> create_motion_profile(const stream_identifier& stream_id, rs2_format format,
            uint32_t fps, const std::map<std::string, std::string>& intrinsics_kv) const;
        static std::shared_ptr<video_stream_profile> create_video_stream_profile(const stream_identifier& stream_id, rs2_format format,
            uint32_t fps, const std::map<std::string, std::string>& intrinsics_kv);


        // Frame setup helpers
        void read_frame_metadata(frame_additional_data& additional_data);
        void setup_frame(frame_interface* frame_ptr, const stream_identifier& sid) const;
        
        // Allocates a frame of the given extension type, copies raw data, and sets up the stream profile
        frame_holder alloc_and_fill_frame(const uint8_t* data, size_t data_size,
            const stream_identifier& stream_id, frame_additional_data additional_data) const;

        std::pair<rs2_option, std::shared_ptr<librealsense::option>> create_option(const std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg);
        std::shared_ptr< serialized_frame > create_frame(const std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg);

        std::shared_ptr< processing_block_interface >
            create_processing_block(const std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg,
                                 bool & depth_to_disparity,
                                 std::shared_ptr< options_interface > options );

        notification create_notification(const std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg) const;
        std::shared_ptr<options_container> read_sensor_options(device_serializer::sensor_identifier sensor_id);

        std::shared_ptr< rosbag2_storage::storage_interfaces::ReadWriteInterface > _storage;

        std::shared_ptr<metadata_parser_map>    m_metadata_parser_map;
        device_snapshot                         m_initial_device_description;
        nanoseconds                             m_total_duration;
        std::string                             m_file_path;
        std::shared_ptr<frame_source>           m_frame_source;
        std::vector< rosbag2_storage::TopicMetadata > _topics_cache;
        std::shared_ptr<context>                m_context;
        std::map<uint32_t, std::map<rs2_option, std::string>> m_read_options_descriptions;

        // State management
        bool _initialized = false;
        std::set< stream_identifier > _enabled_streams;

        // Cache to support fetch_last_frames logic
        // Maps stream ID to the last frame data seen
        std::map< stream_identifier, std::shared_ptr<serialized_data> > _last_frame_cache;

        std::map< stream_identifier, std::pair< uint32_t, rs2_extrinsics > > m_extrinsics_map;

        std::shared_ptr<rosbag2_storage::SerializedBagMessage> _cached_message;
        bool _cache_valid = false;  // true means _cached_message contains valid unconsumed data

        // Filter topics for streaming - reapplied on reset() if set
        std::vector<std::string> _streaming_filter_topics;

    };
}