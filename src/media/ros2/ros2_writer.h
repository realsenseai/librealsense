// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#pragma once
#include <rosbag2_storage/serialized_bag_message.hpp>
#include <rosbag2_storage/storage_interfaces/read_write_interface.hpp>
#include <rosbag2_storage/topic_metadata.hpp>
#include <rosbag2_storage/storage_options.hpp>
#include <rosbag2_storage_default_plugins/sqlite/sqlite_storage.hpp>

#include <media/ros/ros_file_format.h> // reuse ros_topic naming + helpers

#include <rsutils/string/from.h>


namespace librealsense
{
    using namespace device_serializer;

    class recommended_proccesing_blocks_interface;

    class ros2_writer: public writer
    {
    public:
        explicit ros2_writer( const std::string& file, bool compress_while_record);
        void write_device_description(const librealsense::device_snapshot& device_description) override;
        void write_frame(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_holder&& frame) override;
        void write_snapshot(uint32_t device_index, const nanoseconds& timestamp, rs2_extension type, const std::shared_ptr<extension_snapshot>& snapshot) override;
        void write_snapshot(const sensor_identifier& sensor_id, const nanoseconds& timestamp, rs2_extension type, const std::shared_ptr<extension_snapshot>& snapshot) override;
        const std::string& get_file_name() const override;

    private:
        void write_file_version();
        void write_frame_metadata(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_interface* frame);
        void write_extrinsics(const stream_identifier& stream_id, frame_interface* frame);
        void write_string( std::string const & topic, const device_serializer::nanoseconds & ts, std::string const & payload );
        void ensure_topic( const std::string & name, const std::string & type );
        std::shared_ptr<rcutils_uint8_array_t> create_buffer(const void* data, size_t size);

        void write_notification(const sensor_identifier& sensor_id, const nanoseconds& timestamp, const notification& n) override;
        void write_additional_frame_messages(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_interface* frame);
        void write_video_frame(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_holder&& frame);
        void write_motion_frame(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_holder&& frame);
        void write_stream_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<stream_profile_interface> profile);
        void write_streaming_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<video_stream_profile_interface> profile);
        void write_streaming_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<motion_stream_profile_interface> profile);
        void write_streaming_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<pose_stream_profile_interface> profile);
        void write_extension_snapshot(uint32_t device_id, const nanoseconds& timestamp, rs2_extension type, std::shared_ptr<librealsense::extension_snapshot> snapshot);
        void write_extension_snapshot(uint32_t device_id, uint32_t sensor_id, const nanoseconds& timestamp, rs2_extension type, std::shared_ptr<librealsense::extension_snapshot> snapshot);

        template <rs2_extension E>
        std::shared_ptr<typename ExtensionToType<E>::type> SnapshotAs(std::shared_ptr<librealsense::extension_snapshot> snapshot)
        {
            auto as_type = As<typename ExtensionToType<E>::type>(snapshot);
            if (as_type == nullptr)
            {
                throw invalid_value_exception( rsutils::string::from()
                                               << "Failed to cast snapshot to \"" << E << "\" (as \""
                                               << ExtensionToType< E >::to_string() << "\")" );
            }
            return as_type;
        }

        void write_extension_snapshot(uint32_t device_id, uint32_t sensor_id, const nanoseconds& timestamp, rs2_extension type, std::shared_ptr<librealsense::extension_snapshot> snapshot, bool is_device);
        void write_vendor_info(const std::string& topic, nanoseconds timestamp, std::shared_ptr<info_interface> info_snapshot);
        void write_sensor_option(device_serializer::sensor_identifier sensor_id, const nanoseconds& timestamp, rs2_option type, const librealsense::option& option);
        void write_sensor_options(device_serializer::sensor_identifier sensor_id, const nanoseconds& timestamp, std::shared_ptr<options_interface> options);
        void write_sensor_processing_blocks(device_serializer::sensor_identifier sensor_id, const nanoseconds& timestamp, std::shared_ptr<recommended_proccesing_blocks_interface> proccesing_blocks);

        static uint8_t is_big_endian();
        std::string m_file_path;
        std::map< std::string, rosbag2_storage::TopicMetadata > _topics; // created topics cache
        std::shared_ptr< rosbag2_storage::storage_interfaces::ReadWriteInterface > _storage;
        std::map<uint32_t, std::set<rs2_option>> m_written_options_descriptions;
        std::set<device_serializer::stream_identifier> m_extrinsics_msgs;
    };
}
