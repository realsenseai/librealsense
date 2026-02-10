// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "proc/decimation-filter.h"
#include "proc/rotation-filter.h"
#include "proc/threshold.h"
#include "proc/disparity-transform.h"
#include "proc/spatial-filter.h"
#include "proc/temporal-filter.h"
#include "proc/hole-filling-filter.h"
#include "proc/hdr-merge.h"
#include "proc/sequence-id-filter.h"
#include "ros2_writer.h"
#include "core/pose-frame.h"
#include "core/motion-frame.h"
#include <src/core/sensor-interface.h>
#include <src/core/device-interface.h>
#include <src/core/depth-frame.h>
#include <src/points.h>
#include <src/labeled-points.h>

#include <rsutils/string/from.h>
#include <fstream>   // for std::ifstream

namespace librealsense
{
    using namespace device_serializer;

    ros2_writer::ros2_writer(const std::string& file, bool compress_while_record) : m_file_path(file)
    {
        LOG_INFO("Compression while record is set to " << (compress_while_record ? "ON" : "OFF"));
        _storage = std::make_shared< rosbag2_storage_plugins::SqliteStorage >();
        // check if file exists, if so, delete it to record - rosbag2 sqlite plugin doesn't overwrite existing files
        std::ifstream f(file + ".db3");
        if (f.good())
        {
            f.close();
            if (std::remove((file + ".db3").c_str()) != 0)
            {
                throw std::runtime_error(rsutils::string::from() << "Failed to remove existing rosbag2 storage file '" << file << "'");
            }
        }

        _storage->open(file, rosbag2_storage::storage_interfaces::IOFlag::READ_WRITE);
        m_file_path += ".db3"; // rosbag2 sqlite plugin appends .db3 internally, so this is for consistency
        if (!_storage)
            throw std::runtime_error(rsutils::string::from() << "Failed to open rosbag2 storage for uri '" << file
                << "' using storage id 'sqlite3'");

        if (compress_while_record)
        {
            // TODO: implement
        }

        write_file_version();
    }

    // TODO: All topic names need to be changed to have the writer play natively on ROS2
    void ros2_writer::ensure_topic(const std::string& name, const std::string& type)
    {
        if (_topics.find(name) != _topics.end())
            return;
        rosbag2_storage::TopicMetadata md;
        md.name = name;
        md.type = type;
        md.serialization_format = "cdr"; // placeholder; we store raw bytes
        _storage->create_topic(md);
        _topics.emplace(name, md);
    }

    std::shared_ptr<rcutils_uint8_array_t> ros2_writer::create_buffer(const void* data, size_t size)
    {
        auto buffer = std::shared_ptr<rcutils_uint8_array_t>(new rcutils_uint8_array_t(),
            [](rcutils_uint8_array_t* arr) {
                if (arr) {
                    rcutils_ret_t ret = rcutils_uint8_array_fini(arr);
                    (void)ret; // Cast to void to suppress unusued warning
                } 
                delete arr;
            });

        // Initialize the array with the allocator
        rcutils_allocator_t alloc = rcutils_get_default_allocator();
        auto ret = rcutils_uint8_array_init(buffer.get(), size, &alloc);
        if (ret != RCUTILS_RET_OK)
            throw std::runtime_error("Failed to initialize rosbag2 buffer");

        // Now copy the data
        std::memcpy(buffer->buffer, data, size);
        buffer->buffer_length = size;

        return buffer;
    }


    void ros2_writer::write_string(std::string const& topic, const nanoseconds& ts, std::string const& payload)
    {
        ensure_topic(topic, "librealsense/string");
        auto buffer = create_buffer(payload.data(), payload.size());

        auto msg = std::make_shared< rosbag2_storage::SerializedBagMessage >();
        msg->serialized_data = buffer;
        msg->time_stamp = static_cast<rcutils_time_point_value_t>(ts.count());
        msg->topic_name = topic;
        _storage->write(msg);
    }

    

    void ros2_writer::write_device_description(const librealsense::device_snapshot& device_description)
    {
        for (auto&& device_extension_snapshot : device_description.get_device_extensions_snapshots().get_snapshots())
        {
            write_extension_snapshot(get_device_index(), get_static_file_info_timestamp(), device_extension_snapshot.first, device_extension_snapshot.second);
        }

        for (auto&& sensors_snapshot : device_description.get_sensors_snapshots())
        {
            for (auto&& sensor_extension_snapshot : sensors_snapshot.get_sensor_extensions_snapshots().get_snapshots())
            {
                write_extension_snapshot(get_device_index(), sensors_snapshot.get_sensor_index(), get_static_file_info_timestamp(), sensor_extension_snapshot.first, sensor_extension_snapshot.second);
            }
        }
    }

    void ros2_writer::write_frame(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_holder&& frame)
    {
        if (!frame || !frame.frame)
            return;

        if (Is<video_frame>(frame.frame))
        {
            write_video_frame(stream_id, timestamp, std::move(frame));
            return;
        }

        if (Is<motion_frame>(frame.frame))
        {
            write_motion_frame(stream_id, timestamp, std::move(frame));
            return;
        }

        /*if (Is<pose_frame>(frame.frame))
        {
            write_pose_frame(stream_id, timestamp, std::move(frame));
            return;
        }*/

        if (Is<labeled_points>(frame.frame))
        {
            write_labeled_points_frame(stream_id, timestamp, std::move(frame));
            return;
        }
    }

    void ros2_writer::write_snapshot(uint32_t device_index, const nanoseconds& timestamp, rs2_extension type, const std::shared_ptr<extension_snapshot>& snapshot)
    {
        write_extension_snapshot(device_index, -1, timestamp, type, snapshot);
    }

    void ros2_writer::write_snapshot(const sensor_identifier& sensor_id, const nanoseconds& timestamp, rs2_extension type, const std::shared_ptr<extension_snapshot>& snapshot)
    {
        write_extension_snapshot(sensor_id.device_index, sensor_id.sensor_index, timestamp, type, snapshot);
    }

    const std::string& ros2_writer::get_file_name() const 
    {
        return m_file_path;
    }

    void ros2_writer::write_file_version()
    {
        auto file_version_topic = ros_topic::file_version_topic();
        ensure_topic(file_version_topic, "librealsense/file_version"); // this is how we give the topic a type - it indicates what kind of data is stored there
        write_string(file_version_topic, nanoseconds{ 0 }, std::to_string(get_file_version()));
    }

    void ros2_writer::write_frame_metadata(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_interface* frame)
    {
        std::string system_time = std::to_string(frame->get_frame_system_time());
        std::string timestamp_domain = librealsense::get_string(frame->get_frame_timestamp_domain());
        std::string frame_number = std::to_string(frame->get_frame_number());
        std::string ts = std::to_string(frame->get_frame_timestamp());

        std::string metadata_payload = rsutils::string::from() << FRAME_NUMBER_MD_STR << "=" << frame_number << ";" 
                                                                << TIMESTAMP_DOMAIN_MD_STR << "=" << timestamp_domain << ";" 
                                                                << SYSTEM_TIME_MD_STR << "=" << system_time << ";"
                                                                << TIMESTAMP_MD_STR << "="  << ts << ";";
        for (int i = 0; i < RS2_FRAME_METADATA_COUNT; i++)
        {
            rs2_frame_metadata_value type = static_cast<rs2_frame_metadata_value>(i);
            rs2_metadata_type md;
            if (frame->find_metadata(type, &md))
            {
                std::string md_value = std::to_string(md);
                metadata_payload += librealsense::get_string(type) + "=" + md_value + ";";
            }
        }

        auto metadata_topic = ros_topic::frame_metadata_topic(stream_id);
        ensure_topic(metadata_topic, "librealsense/frame_metadata");
        write_string(metadata_topic, timestamp, metadata_payload);
    }

    void ros2_writer::write_extrinsics(const stream_identifier& stream_id, frame_interface* frame)
    {
        if (m_extrinsics_msgs.find(stream_id) != m_extrinsics_msgs.end())
        {
            return; //already wrote it
        }
        auto& dev = frame->get_sensor()->get_device();
        uint32_t reference_id = 0;
        rs2_extrinsics ext;
        std::tie(reference_id, ext) = dev.get_extrinsics(*frame->get_stream());
        
        // Serialize extrinsics as string: rotation (9 floats) and translation (3 floats)
        std::string payload = "rotation=";
        for (int i = 0; i < 9; ++i)
        {
            payload += std::to_string(ext.rotation[i]);
            if (i < 8) payload += ",";
        }
        payload += ";translation=";
        for (int i = 0; i < 3; ++i)
        {
            payload += std::to_string(ext.translation[i]);
            if (i < 2) payload += ",";
        }
        
        auto topic = ros_topic::stream_extrinsic_topic(stream_id, reference_id);
        ensure_topic(topic, "librealsense/extrinsics");
        write_string(topic, get_static_file_info_timestamp(), payload);
        m_extrinsics_msgs.insert(stream_id);
    }

    void ros2_writer::write_notification(const sensor_identifier& sensor_id, const nanoseconds& ts, const notification& n)
    {
        std::string topic = ros_topic::notification_topic(sensor_id, n.category);
        std::string payload = rsutils::string::from() << "category=" << rs2_notification_category_to_string(n.category)
            << ";severity=" << rs2_log_severity_to_string(n.severity)
            << ";description=" << n.description
            << ";timestamp=" << n.timestamp
            << ";data=" << n.serialized_data;
        write_string(topic, ts, payload);
    }


    void ros2_writer::write_additional_frame_messages(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_interface* frame)
    {
        try
        {
            write_frame_metadata(stream_id, timestamp, frame);
        }
        catch (std::exception const& e)
        {
            LOG_WARNING("Failed to write frame metadata for " << stream_id.stream_type << ". Exception: " << e.what());
        }

        try
        {
            write_extrinsics(stream_id, frame);
        }
        catch (std::exception const& e)
        {
            LOG_WARNING("Failed to write stream extrinsics for " << stream_id.stream_type << ". Exception: " << e.what());
        }
    }

    void ros2_writer::write_video_frame(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_holder&& frame)
    {
        auto vid_frame = dynamic_cast<librealsense::video_frame*>(frame.frame);
        if (!vid_frame)
            throw std::runtime_error("Frame is not video frame");
        auto size = vid_frame->get_stride() * vid_frame->get_height();
        auto p_data = vid_frame->get_frame_data();
        auto buffer = create_buffer(p_data, size);
        auto image_topic = ros_topic::frame_data_topic(stream_id);
        ensure_topic(image_topic, "librealsense/raw_frame");
        auto msg = std::make_shared< rosbag2_storage::SerializedBagMessage >();
        msg->serialized_data = buffer;
        msg->time_stamp = static_cast<rcutils_time_point_value_t>(timestamp.count());
        msg->topic_name = image_topic;
        _storage->write(msg);
        write_additional_frame_messages(stream_id, timestamp, frame);
    }

    void ros2_writer::write_motion_frame(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_holder&& frame)
    {
        auto motion_frame = dynamic_cast<librealsense::motion_frame*>(frame.frame);
        if (!motion_frame)
            throw std::runtime_error("Frame is not motion frame");
        auto fi = motion_frame;
        auto topic = ros_topic::frame_data_topic(stream_id);
        ensure_topic(topic, "librealsense/raw_motion_frame");
        auto data_ptr = reinterpret_cast<const float*>(frame.frame->get_frame_data());
        auto size = (stream_id.stream_type == RS2_STREAM_MOTION) ? sizeof(rs2_combined_motion) : 3 * sizeof(float);
        auto buffer = create_buffer(data_ptr, size);
        auto msg = std::make_shared< rosbag2_storage::SerializedBagMessage >();
        msg->serialized_data = buffer;
        msg->time_stamp = static_cast<rcutils_time_point_value_t>(timestamp.count());
        msg->topic_name = topic;
        _storage->write(msg);
        write_additional_frame_messages(stream_id, timestamp, frame);
    }

    void ros2_writer::write_labeled_points_frame(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_holder&& frame)
    {
        auto labeled_points_frame = dynamic_cast<librealsense::labeled_points*>(frame.frame);
        if (!labeled_points_frame)
            throw invalid_value_exception("null pointer received from dynamic pointer casting.");

        auto size = labeled_points_frame->get_vertex_count() * labeled_points_frame->get_bpp() / 8;
        auto p_data = frame->get_frame_data();

        auto buffer = create_buffer(p_data, size);
        auto image_topic = ros_topic::frame_data_topic(stream_id);
        ensure_topic(image_topic, "librealsense/raw_frame");
        auto msg = std::make_shared< rosbag2_storage::SerializedBagMessage >();
        msg->serialized_data = buffer;
        msg->time_stamp = static_cast<rcutils_time_point_value_t>(timestamp.count());
        msg->topic_name = image_topic;
        _storage->write(msg);
        write_additional_frame_messages(stream_id, timestamp, frame);
    }

    void ros2_writer::write_stream_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<stream_profile_interface> profile)
    {
        auto topic = ros_topic::stream_info_topic({ sensor_id.device_index, sensor_id.sensor_index, profile->get_stream_type(), static_cast<uint32_t>(profile->get_stream_index()) });
        ensure_topic(topic, "librealsense/stream_info");
        std::string payload = rsutils::string::from()
            << "is_recommended=" << ((profile->get_tag() & profile_tag::PROFILE_TAG_DEFAULT) ? "true" : "false") << ";"
            << "encoding=" << librealsense::get_string(profile->get_format()) << ";"
            << "fps=" << profile->get_framerate();
        
        write_string(topic, timestamp, payload);
    }

    void ros2_writer::write_streaming_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<video_stream_profile_interface> profile)
    {
        write_stream_info(timestamp, sensor_id, profile);
        auto topic = ros_topic::video_stream_info_topic({ sensor_id.device_index, sensor_id.sensor_index, profile->get_stream_type(), static_cast<uint32_t>(profile->get_stream_index()) });
        ensure_topic(topic, "librealsense/camera_info");
        rs2_intrinsics intrinsics{};
        try {
            intrinsics = profile->get_intrinsics();
        }
        catch (...)
        {
            LOG_ERROR("Error trying to get intrinsc data for stream " << profile->get_stream_type() << ", " << profile->get_stream_index());
        }
        std::string payload = rsutils::string::from()
            << "width=" << profile->get_width() << ";"
            << "height=" << profile->get_height() << ";"
            << "fx=" << intrinsics.fx << ";"
            << "ppx=" << intrinsics.ppx << ";"
            << "fy=" << intrinsics.fy << ";"
            << "ppy=" << intrinsics.ppy << ";"
            << "model=" << librealsense::get_string(intrinsics.model) << ";"
            << "coeffs=";

        auto num_coeffs = sizeof(intrinsics.coeffs) / sizeof(intrinsics.coeffs[0]);
        for (size_t i = 0; i < num_coeffs; ++i)
        {
            payload += std::to_string(intrinsics.coeffs[i]);
            if (i < (num_coeffs - 1))
                payload += ",";
        }
        write_string(topic, timestamp, payload);
    }

    void ros2_writer::write_streaming_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<motion_stream_profile_interface> profile)
    {
        write_stream_info(timestamp, sensor_id, profile);

        rs2_motion_device_intrinsic intrinsics{};
        try {
            intrinsics = profile->get_intrinsics();
        }
        catch (...)
        {
            LOG_ERROR("Error trying to get intrinsc data for stream " << profile->get_stream_type() << ", " << profile->get_stream_index());
        }

        std::string topic = ros_topic::imu_intrinsic_topic({ sensor_id.device_index, sensor_id.sensor_index, profile->get_stream_type(), static_cast<uint32_t>(profile->get_stream_index()) });
        ensure_topic(topic, "librealsense/imu_intrinsic");
        std::string payload = "data=";
        for (size_t i = 0; i < 3; ++i)
        {
            for (size_t j = 0; j < 4; ++j)
            {
                payload += std::to_string(intrinsics.data[i][j]);
                if (i != 2 || j != 3)
                    payload += ",";
            }
        }
        payload += ";bias_variances=";
        for (size_t i = 0; i < 3; ++i)
        {
            payload += std::to_string(intrinsics.bias_variances[i]);
            if (i != 2)
                payload += ",";
        }
        payload += ";noise_variances=";
        for (size_t i = 0; i < 3; ++i)
        {
            payload += std::to_string(intrinsics.noise_variances[i]);
            if (i != 2)
                payload += ",";
        }
        write_string(topic, timestamp, payload);
    }

    void ros2_writer::write_streaming_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<pose_stream_profile_interface> profile)
    {
        write_stream_info(timestamp, sensor_id, profile);
    }
    void ros2_writer::write_extension_snapshot(uint32_t device_id, const nanoseconds& timestamp, rs2_extension type, std::shared_ptr<librealsense::extension_snapshot> snapshot)
    {
        const auto ignored = 0u;
        write_extension_snapshot(device_id, ignored, timestamp, type, snapshot, true);
    }

    void ros2_writer::write_extension_snapshot(uint32_t device_id, uint32_t sensor_id, const nanoseconds& timestamp, rs2_extension type, std::shared_ptr<librealsense::extension_snapshot> snapshot)
    {
        write_extension_snapshot(device_id, sensor_id, timestamp, type, snapshot, false);
    }

    void ros2_writer::write_extension_snapshot(uint32_t device_id, uint32_t sensor_id, const nanoseconds& timestamp, rs2_extension type, std::shared_ptr<librealsense::extension_snapshot> snapshot, bool is_device)
    {
        switch (type)
        {
        case RS2_EXTENSION_INFO:
        {
            auto info = SnapshotAs<RS2_EXTENSION_INFO>(snapshot);
            if (info)
            {
                if (is_device)
                {
                    write_vendor_info(ros_topic::device_info_topic(device_id), timestamp, info);
                }
                else
                {
                    write_vendor_info(ros_topic::sensor_info_topic({ device_id, sensor_id }), timestamp, info);
                }
            }
            break;
        }
        case RS2_EXTENSION_OPTIONS:
        {
            auto options = SnapshotAs<RS2_EXTENSION_OPTIONS>(snapshot);
            write_sensor_options({ device_id, sensor_id }, timestamp, options);
            break;
        }

        case RS2_EXTENSION_VIDEO_PROFILE:
        {
            auto profile = SnapshotAs<RS2_EXTENSION_VIDEO_PROFILE>(snapshot);
            write_streaming_info(timestamp, { device_id, sensor_id }, profile);
            break;
        }
        case RS2_EXTENSION_MOTION_PROFILE:
        {
            auto profile = SnapshotAs<RS2_EXTENSION_MOTION_PROFILE>(snapshot);
            write_streaming_info(timestamp, { device_id, sensor_id }, profile);
            break;
        }
        /*case RS2_EXTENSION_POSE_PROFILE:
        {
            auto profile = SnapshotAs<RS2_EXTENSION_POSE_PROFILE>(snapshot);
            write_streaming_info(timestamp, { device_id, sensor_id }, profile);
            break;
        }*/
        case RS2_EXTENSION_RECOMMENDED_FILTERS:
        {
            auto filters = SnapshotAs<RS2_EXTENSION_RECOMMENDED_FILTERS>(snapshot);
            write_sensor_processing_blocks({ device_id, sensor_id }, timestamp, filters);
            break;
        }
        default:
            throw invalid_value_exception( rsutils::string::from() << "Failed to Write Extension Snapshot: Unsupported extension \"" << librealsense::get_string(type) << "\"");
        }

    }

    void ros2_writer::write_vendor_info(const std::string& topic, nanoseconds timestamp, std::shared_ptr< info_interface > info_snapshot)
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(RS2_CAMERA_INFO_COUNT); i++)
        {
            auto camera_info = static_cast<rs2_camera_info>(i);
            if (info_snapshot->supports_info(camera_info))
            {
                std::string kv = rsutils::string::from() << rs2_camera_info_to_string(camera_info) << "=" << info_snapshot->get_info(camera_info);
                write_string(topic, timestamp, kv);
            }
        }
    }

    void ros2_writer::write_sensor_option(device_serializer::sensor_identifier sensor_id, const nanoseconds& timestamp, rs2_option type, const librealsense::option& option)
    {
        float value = option.query();
        //One message for value
        write_string(ros_topic::option_value_topic(sensor_id, type), timestamp, std::to_string(value));
        //Another message for description, should be written once per topic
        if (m_written_options_descriptions[sensor_id.sensor_index].find(type) == m_written_options_descriptions[sensor_id.sensor_index].end())
        {
            const char* desc = option.get_description();
            std::string description = desc ? std::string(desc) : (rsutils::string::from() << "Read only option " << librealsense::get_string(type));
            write_string(ros_topic::option_description_topic(sensor_id, type), get_static_file_info_timestamp(), description);
            m_written_options_descriptions[sensor_id.sensor_index].insert(type);
        }
    }

    void ros2_writer::write_sensor_options(device_serializer::sensor_identifier sensor_id, const nanoseconds& timestamp, std::shared_ptr<options_interface> options)
    {
        if (!options)
            return;

        for (int i = 0; i < static_cast<int>(RS2_OPTION_COUNT); i++)
        {
            auto option_id = static_cast<rs2_option>(i);
            try
            {
                if (options->supports_option(option_id))
                {
                    write_sensor_option(sensor_id, timestamp, option_id, options->get_option(option_id));
                }
            }
            catch (std::exception& e)
            {
                LOG_WARNING("Failed to get or write option " << option_id << " for sensor " << sensor_id.sensor_index << ". Exception: " << e.what());
            }
        }
    }

    static std::string get_processing_block_extension_name( const std::shared_ptr< processing_block_interface > block )
    {
        // We want to write the block name (as opposed to the extension name):
        // The block can behave differently and have a different name based on how it was created (e.g., the disparity
        // filter). This makes new rosbag files incompatible with older librealsense versions.
        if( block->supports_info( RS2_CAMERA_INFO_NAME ) )
            return block->get_info( RS2_CAMERA_INFO_NAME );

#define RETURN_IF_EXTENSION( B, E )                                                                                    \
    if( Is< ExtensionToType< E >::type >( B ) )                                                                        \
        return rs2_extension_type_to_string( E )
 
        RETURN_IF_EXTENSION(block, RS2_EXTENSION_DECIMATION_FILTER);
        RETURN_IF_EXTENSION(block, RS2_EXTENSION_THRESHOLD_FILTER);
        RETURN_IF_EXTENSION(block, RS2_EXTENSION_DISPARITY_FILTER);
        RETURN_IF_EXTENSION(block, RS2_EXTENSION_SPATIAL_FILTER);
        RETURN_IF_EXTENSION(block, RS2_EXTENSION_TEMPORAL_FILTER);
        RETURN_IF_EXTENSION(block, RS2_EXTENSION_HOLE_FILLING_FILTER);
        RETURN_IF_EXTENSION(block, RS2_EXTENSION_HDR_MERGE);
        RETURN_IF_EXTENSION(block, RS2_EXTENSION_SEQUENCE_ID_FILTER);
        RETURN_IF_EXTENSION(block, RS2_EXTENSION_ROTATION_FILTER);

#undef RETURN_IF_EXTENSION

        return {};
    }

    void ros2_writer::write_sensor_processing_blocks(device_serializer::sensor_identifier sensor_id, const nanoseconds& timestamp, std::shared_ptr<recommended_proccesing_blocks_interface> proccesing_blocks)
    {
        for (auto block : proccesing_blocks->get_recommended_processing_blocks())
        {
            std::string name = get_processing_block_extension_name(block);
            if (name.empty())
            {
                LOG_WARNING("Failed to get recommended processing block name for sensor " << sensor_id.sensor_index);
                continue;
            }
            try
            {
                write_string(ros_topic::post_processing_blocks_topic(sensor_id), timestamp, name);
            }
            catch (std::exception& e)
            {
                LOG_WARNING("Failed to write processing block '" << name << "' for sensor " << sensor_id.sensor_index
                    << ": " << e.what());
            }
        }
    }

    uint8_t ros2_writer::is_big_endian()
    {
        int num = 1;
        return (*reinterpret_cast<char*>(&num) == 1) ? 0 : 1; //Little Endian: (char)0x0001 => 0x01, Big Endian: (char)0x0001 => 0x00,
    }
}
