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
    class processing_block_interface;

    class ros2_reader : public reader
    {
    public:
        ros2_reader(const std::string& file_path, const std::shared_ptr<context>& ctx);
        virtual ~ros2_reader() = default;

        // Interface Implementations
        device_snapshot query_device_description(const nanoseconds& time) override;
        std::shared_ptr<serialized_data> read_next_data() override;
        void seek_to_time(const nanoseconds& seek_time) override;
        std::vector<std::shared_ptr<serialized_data>> fetch_last_frames(const nanoseconds& seek_time) override;
        nanoseconds query_duration() const override;
        void reset() override;
        void enable_stream(const std::vector<stream_identifier>& stream_ids) override;
        void disable_stream(const std::vector<stream_identifier>& stream_ids) override;
        const std::string& get_file_name() const override;

        // Caching wrapper methods
        bool has_next_cached() const;
        std::shared_ptr<rosbag2_storage::SerializedBagMessage> read_next_cached();
        std::shared_ptr<rosbag2_storage::SerializedBagMessage> peek_next_cached();

        void update_last_frame_cache(std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg);
    private:
        // Helper to parse "key=value;key2=val2" format used by writer
        std::map< std::string, std::string > parse_key_value_string(const std::string& payload) const;
        std::map< std::string, std::string > parse_msg_payload(const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg) const;
        void register_camera_infos(std::shared_ptr<info_container>& infos, const std::map<std::string, std::string>& kv) const;
        nanoseconds get_file_duration();

        uint32_t read_file_version();
        bool try_read_stream_extrinsic(const stream_identifier& stream_id, uint32_t& group_id, rs2_extrinsics& extrinsic);
        void add_sensor_extension(snapshot_collection& sensor_extensions, const std::string& sensor_name);
       
        static bool is_depth_sensor(const std::string& sensor_name);
        static bool is_stereo_depth_sensor(const std::string& sensor_name);
        static bool is_color_sensor(const std::string& sensor_name);
        static bool is_motion_module_sensor(const std::string& sensor_name);
        static bool is_fisheye_module_sensor(const std::string& sensor_name);
        static bool is_safety_module_sensor(const std::string& sensor_name);
        static bool is_depth_mapping_sensor(const std::string& sensor_name);

        device_snapshot read_device_description(const nanoseconds& time, bool reset = false);

        // Topic parsing helpers
        bool is_stream_topic(const std::string& topic, stream_identifier& id) const;
        bool is_option_topic(const std::string& topic, sensor_identifier& sid, rs2_option& opt) const;
        std::shared_ptr<info_container> read_info_snapshot(const std::string& topic);
        std::shared_ptr<stream_profile_interface> read_next_stream_profile();
        std::set<uint32_t> read_sensor_indices(uint32_t device_index) const;
        std::map<uint32_t, stream_profiles> read_all_stream_profiles(uint32_t device_index);
        std::map<uint32_t, std::shared_ptr<info_container>> read_all_sensor_info(std::set<uint32_t> sensor_indices);

        // Stream profile parsing helpers
        rs2_motion_device_intrinsic parse_motion_intrinsics(const std::map<std::string, std::string>& kv) const;
        rs2_intrinsics parse_video_intrinsics(const std::map<std::string, std::string>& kv) const;
        std::shared_ptr<motion_stream_profile> create_motion_profile(const stream_identifier& stream_id, rs2_format format,
            uint32_t fps, const std::map<std::string, std::string>& intrinsics_kv) const;
        std::shared_ptr<video_stream_profile> create_video_profile(const stream_identifier& stream_id, rs2_format format,
            uint32_t fps, const std::map<std::string, std::string>& intrinsics_kv) const;


        // Frame setup helpers
        void read_frame_metadata(frame_additional_data& additional_data);
        void setup_video_frame(frame_interface* frame_ptr, const stream_identifier& sid) const;
        void setup_motion_frame(frame_interface* frame_ptr, const stream_identifier& sid) const;
        
        std::shared_ptr< serialized_data > read_frame_data(const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg, const stream_identifier& sid);
        frame_holder allocate_frame(const stream_identifier& sid, const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg, const frame_additional_data& additional_data);

        std::shared_ptr< rosbag2_storage::storage_interfaces::ReadWriteInterface > _storage;

        std::shared_ptr<metadata_parser_map>    m_metadata_parser_map;
        device_snapshot                         m_initial_device_description;
        nanoseconds                             m_total_duration;
        std::string                             m_file_path;
        std::shared_ptr<frame_source>           m_frame_source;
        std::vector< rosbag2_storage::TopicMetadata > _topics_cache;
        std::shared_ptr<context>                m_context;
        uint32_t                                m_version;
        // State management
        bool _initialized = false;
        std::set< stream_identifier > _enabled_streams;

        // Cache to support fetch_last_frames logic
        // Maps stream ID to the last frame data seen
        std::map< stream_identifier, std::shared_ptr<serialized_data> > _last_frame_cache;

        std::map< stream_identifier, std::pair< uint32_t, rs2_extrinsics > > m_extrinsics_map;

        std::shared_ptr<rosbag2_storage::SerializedBagMessage> _cached_message;
        bool _cache_valid = false;  // true means _cached_message contains valid unconsumed data
    };
}