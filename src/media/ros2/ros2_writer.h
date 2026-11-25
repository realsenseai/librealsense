// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#pragma once
#include <rosbag2_storage/serialized_bag_message.hpp>
#include <rosbag2_storage/topic_metadata.hpp>
#include <rosbag2_storage_default_plugins/sqlite/sqlite_storage.hpp>
#include <sensor_msgs/msg/CameraInfo.h>
#include <geometry_msgs/msg/TransformStamped.h>
#include <sensor_msgs/msg/CompressedImage.h>
#include <deque>
#include <future>
#include <memory>

#include "ros2_file_format.h"


namespace librealsense
{
    using namespace device_serializer;

    class info_interface;
    class options_interface;
    class recommended_proccesing_blocks_interface;

    class ros2_writer: public writer
    {
    public:
        explicit ros2_writer( const std::string& file, bool compress_while_record);
        ~ros2_writer() override;
        void write_device_description(const librealsense::device_snapshot& device_description) override;
        void write_frame(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_holder&& frame) override;
        void write_snapshot(uint32_t device_index, const nanoseconds& timestamp, rs2_extension type, const std::shared_ptr<extension_snapshot>& snapshot) override;
        void write_snapshot(const sensor_identifier& sensor_id, const nanoseconds& timestamp, rs2_extension type, const std::shared_ptr<extension_snapshot>& snapshot) override;
        const std::string& get_file_name() const override;

    private:
        void write_file_version();
        void write_frame_metadata(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_interface* frame);
        void write_camera_info(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_interface* frame);
        void write_string( std::string const & topic, const device_serializer::nanoseconds & ts, std::string const & payload );
        void ensure_topic( const std::string & name, const std::string & type, const std::string & offered_qos = "" );

        void write_notification(const sensor_identifier& sensor_id, const nanoseconds& timestamp, const notification& n) override;
        void write_extrinsics(const stream_identifier& stream_id, uint32_t reference_id, const rs2_extrinsics& ext) override;
        void write_additional_frame_messages(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_interface* frame);
        void write_stream_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<stream_profile_interface> profile);
        void write_streaming_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<video_stream_profile_interface> profile);
        void write_streaming_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<motion_stream_profile_interface> profile);
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

        // CDR encapsulation header: 2 bytes representation identifier + 2 bytes options
        static constexpr size_t CDR_HEADER_SIZE = 4;

        template<typename T>
        void write_message(const std::string& topic, const std::string& msg_type, const nanoseconds& timestamp, const T& data,
                           const std::string& offered_qos = "")
        {
            // Serialize into reusable CDR buffer — avoids per-message malloc on the hot path
            auto total_size = T::getCdrSerializedSize(data) + CDR_HEADER_SIZE;
            auto& buffer = ensure_buffer_capacity(_cdr_buf, total_size);
            eprosima::fastcdr::FastBuffer fb(reinterpret_cast<char*>(buffer->buffer), total_size);
            eprosima::fastcdr::Cdr cdr(fb, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::Cdr::DDS_CDR);
            cdr.serialize_encapsulation();
            data.serialize(cdr);
            buffer->buffer_length = static_cast<size_t>(cdr.getSerializedDataLength());

            // Write to storage
            ensure_topic(topic, msg_type, offered_qos);
            auto msg = std::make_shared<rosbag2_storage::SerializedBagMessage>();
            msg->serialized_data = buffer;
            msg->time_stamp = static_cast<rcutils_time_point_value_t>(timestamp.count());
            msg->topic_name = topic;
            _storage->write(msg);
        }

        // Compression happens inside CompressedImage payloads (PNG or zstd) so the bag
        // stays natively playable by ROS2; whole-CDR compression is playback-breaking.
        // Encoding runs on worker threads so a slow encoder does not hold the recorded frames, and with them
        // the sensor's frame pool; storage is still only touched by the calling thread, in submission order.
        struct encoded_frame
        {
            std::string topic;
            sensor_msgs::msg::CompressedImage msg;
        };
        struct pending_frame
        {
            device_serializer::stream_identifier stream_id;
            nanoseconds timestamp;
            frame_holder frame;
            std::future< encoded_frame > encoded;
        };
        class encode_pool;

        static encoded_frame encode_compressed_frame( const device_serializer::stream_identifier & stream_id,
                                                      const nanoseconds & timestamp,
                                                      frame_interface * frame );
        void queue_compressed_frame( const device_serializer::stream_identifier & stream_id,
                                     const nanoseconds & timestamp,
                                     frame_holder && frame );
        // Writes encoded frames in order: those already done, and the oldest ones while more than max_pending are queued
        void commit_encoded_frames( size_t max_pending );

        static uint8_t is_big_endian();
        std::string m_file_path;
        bool _compress = false;
        // Reused across calls. Safe only while _storage->write() stays synchronous
        // (sqlite binds SQLITE_STATIC and drops the ref in execute_and_reset).
        std::shared_ptr<rcutils_uint8_array_t> _cdr_buf;
        std::unique_ptr< encode_pool > _encoders;
        std::deque< pending_frame > _pending;
        std::map< std::string, rosbag2_storage::TopicMetadata > _topics; // created topics cache
        std::shared_ptr< rosbag2_storage::storage_interfaces::ReadWriteInterface > _storage;
        std::map<uint32_t, std::set<rs2_option>> m_written_options_descriptions;
        std::set<device_serializer::stream_identifier> m_extrinsics_msgs;
        // Per-stream CameraInfo templates — intrinsics are static, only the stamp changes per frame
        std::map<device_serializer::stream_identifier, sensor_msgs::msg::CameraInfo> m_camera_info_msgs;
        // All static transforms written so far; every /tf_static message republishes the full set
        std::vector<geometry_msgs::msg::TransformStamped> m_tf_transforms;
    };
}
