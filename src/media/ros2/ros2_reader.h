// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#pragma once
#include "ros2_reader_base.h"


namespace librealsense
{
    using namespace device_serializer;

    class info_container;
    class options_interface;
    class options_container;
    class processing_block_interface;
    class recommended_proccesing_blocks_snapshot;

    class ros2_reader : public ros2_reader_base
    {
    public:
        ros2_reader(const std::string& file, const std::shared_ptr<context> ctx);
        std::shared_ptr<serialized_data> read_next_data() override;

    private:
        static std::vector<std::string> split_string(const std::string& s, char delimiter);
        static std::string get_value(const std::map<std::string, std::string>& kv, const std::string& key);
        std::vector<std::string> filter_topics_by_regex(const std::regex& re) const;
        static std::map< std::string, std::string > parse_msg_payload(const std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg);
        static void register_camera_infos(std::shared_ptr<info_container> infos, const std::map<std::string, std::string>& kv);

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
        static bool is_inference_module_sensor(const std::string& sensor_name);
        static bool is_object_detection_sensor(const std::string& sensor_name);

        std::shared_ptr<recommended_proccesing_blocks_snapshot> read_proccesing_blocks(device_serializer::sensor_identifier sensor_id,
            std::shared_ptr<options_interface> options);
        device_snapshot read_device_description(const nanoseconds& time, bool reset = false) override;
        std::vector<std::string> get_stream_topics() const override;
        std::vector<std::string> get_option_topics() const override;
        std::vector<std::string> get_notification_topics() const override;

        // Topic parsing helpers
        bool is_stream_topic(const std::string& topic, stream_identifier& sid) const override;
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
        void setup_frame(frame_interface* frame_ptr, const stream_identifier& sid) const override;

        std::pair<rs2_option, std::shared_ptr<librealsense::option>> create_option(const std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg);
        std::shared_ptr< serialized_frame > create_frame(const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg) override;

        std::shared_ptr< processing_block_interface >
            create_processing_block(const std::string & name,
                                 bool & depth_to_disparity,
                                 std::shared_ptr< options_interface > options );

        notification create_notification(const std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg) const;
        std::shared_ptr<options_container> read_sensor_options(device_serializer::sensor_identifier sensor_id);

        std::map<uint32_t, std::map<rs2_option, std::string>> m_read_options_descriptions;

        std::map< stream_identifier, std::pair< uint32_t, rs2_extrinsics > > m_extrinsics_map;
    };
}