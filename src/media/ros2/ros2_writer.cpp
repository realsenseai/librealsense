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
#include "core/motion-frame.h"
#include <src/core/sensor-interface.h>
#include <src/core/device-interface.h>
#include <src/core/depth-frame.h>
#include <src/points.h>
#include <src/labeled-points.h>

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

    void ros2_writer::ensure_topic(const std::string& name, const std::string& type)
    {
        if (_topics.find(name) != _topics.end())
            return;
        rosbag2_storage::TopicMetadata md;
        md.name = name;
        md.type = type;
        md.serialization_format = "cdr";
        _storage->create_topic(md);
        _topics.emplace(name, md);
    }

    std::shared_ptr<rcutils_uint8_array_t> ros2_writer::create_buffer(size_t size) const
    {
        auto buffer = std::shared_ptr<rcutils_uint8_array_t>(new rcutils_uint8_array_t(),
            [](rcutils_uint8_array_t* arr) {
                if (arr) {
                    rcutils_ret_t ret = rcutils_uint8_array_fini(arr);
                    (void)ret; // Cast to void to suppress unused warning
                } 
                delete arr;
            });

        rcutils_allocator_t alloc = rcutils_get_default_allocator();
        auto ret = rcutils_uint8_array_init(buffer.get(), size, &alloc);
        if (ret != RCUTILS_RET_OK)
            throw std::runtime_error("Failed to initialize rosbag2 buffer");

        return buffer;
    }


    void ros2_writer::write_string(std::string const& topic, const nanoseconds& ts, std::string const& payload)
    {
        write_message(topic, "std_msgs/msg/String", ts, cdr_string{ payload });
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

        // Build ROS2 timestamp from nanoseconds
        auto ns_count = timestamp.count();
        int32_t stamp_sec = static_cast<int32_t>(ns_count / 1000000000LL);
        uint32_t stamp_nsec = static_cast<uint32_t>(ns_count % 1000000000LL);
        auto stream_name = ros2_topic::stream_name(stream_id.stream_type, stream_id.stream_index);

        if (Is<video_frame>(frame.frame))
        {
            auto vid_frame = As<video_frame>(frame.frame);
            sensor_msgs::msg::Image img;
            img.header().stamp().sec(stamp_sec);
            img.header().stamp().nanosec(stamp_nsec);
            img.header().frame_id(stream_name);
            img.is_bigendian(is_big_endian());
            img.width(vid_frame->get_width());
            img.height(vid_frame->get_height());
            img.step(vid_frame->get_stride());

            std::string encoding;
            convert(vid_frame->get_stream()->get_format(), encoding);
            img.encoding(std::move(encoding));

            auto data_size = vid_frame->get_stride() * vid_frame->get_height();
            auto raw = vid_frame->get_frame_data();
            img.data(std::vector<uint8_t>(raw, raw + data_size));

            write_message(ros2_topic::frame_data_topic(stream_id), "sensor_msgs/msg/Image", timestamp, img);
        }
        else if (Is<motion_frame>(frame.frame))
        {
            auto motion = As<motion_frame>(frame.frame);
            sensor_msgs::msg::Imu imu;
            imu.header().stamp().sec(stamp_sec);
            imu.header().stamp().nanosec(stamp_nsec);
            imu.header().frame_id(stream_name);
            imu.orientation_covariance().fill(0.0);
            imu.angular_velocity_covariance().fill(0.0);
            imu.linear_acceleration_covariance().fill(0.0);

            if (stream_id.stream_type == RS2_STREAM_MOTION)
            {
                auto data = reinterpret_cast<const rs2_combined_motion*>(motion->get_frame_data());
                imu.orientation().x(data->orientation.x);
                imu.orientation().y(data->orientation.y);
                imu.orientation().z(data->orientation.z);
                imu.orientation().w(data->orientation.w);
                imu.angular_velocity().x(data->angular_velocity.x);
                imu.angular_velocity().y(data->angular_velocity.y);
                imu.angular_velocity().z(data->angular_velocity.z);
                imu.linear_acceleration().x(data->linear_acceleration.x);
                imu.linear_acceleration().y(data->linear_acceleration.y);
                imu.linear_acceleration().z(data->linear_acceleration.z);
            }
            else
            {
                // Per ROS convention, orientation_covariance[0] = -1 indicates orientation is unknown
                imu.orientation_covariance()[0] = -1.0;

                auto data = reinterpret_cast<const float*>(motion->get_frame_data());
                if (stream_id.stream_type == RS2_STREAM_GYRO)
                {
                    imu.angular_velocity().x(data[0]);
                    imu.angular_velocity().y(data[1]);
                    imu.angular_velocity().z(data[2]);
                }
                else // ACCEL
                {
                    imu.linear_acceleration().x(data[0]);
                    imu.linear_acceleration().y(data[1]);
                    imu.linear_acceleration().z(data[2]);
                }
            }

            write_message(ros2_topic::frame_data_topic(stream_id), "sensor_msgs/msg/Imu", timestamp, imu);
        }
        else if (Is<labeled_points>(frame.frame))
        {
            auto lp = As<labeled_points>(frame.frame);
            sensor_msgs::msg::Image img;
            img.header().stamp().sec(stamp_sec);
            img.header().stamp().nanosec(stamp_nsec);
            img.header().frame_id(stream_name);
            img.is_bigendian(is_big_endian());

            auto data_size = static_cast<uint32_t>(lp->get_vertex_count() * lp->get_bpp() / 8);
            auto raw = lp->get_frame_data();
            img.data(std::vector<uint8_t>(raw, raw + data_size));
            img.encoding(rs2_format_to_string(lp->get_stream()->get_format()));
            img.width(data_size);
            img.height(1);
            img.step(data_size);

            write_message(ros2_topic::frame_data_topic(stream_id), "sensor_msgs/msg/Image", timestamp, img);
        }
        else
        {
            LOG_WARNING("Unsupported frame type for stream " << stream_id.stream_type << ". Skipping frame.");
            return;
        }

        write_additional_frame_messages(stream_id, timestamp, frame);
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
        cdr_uint32 version_msg{ get_file_version() };
        write_message(ros2_topic::file_version_topic(), "std_msgs/msg/UInt32", nanoseconds(0), version_msg);
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

        auto metadata_topic = ros2_topic::frame_metadata_topic(stream_id);
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
        
        auto topic = ros2_topic::stream_extrinsic_topic(stream_id, reference_id);
        write_string(topic, get_static_file_info_timestamp(), payload);
        m_extrinsics_msgs.insert(stream_id);
    }

    void ros2_writer::write_notification(const sensor_identifier& sensor_id, const nanoseconds& ts, const notification& n)
    {
        std::string topic = ros2_topic::notification_topic(sensor_id, n.category);
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


    void ros2_writer::write_stream_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<stream_profile_interface> profile)
    {
        auto stream_id = device_serializer::stream_identifier{ sensor_id.device_index, sensor_id.sensor_index, profile->get_stream_type(), static_cast<uint32_t>(profile->get_stream_index()) };
        auto topic = ros2_topic::stream_info_topic(stream_id);
        std::string payload = rsutils::string::from()
            << "encoding=" << librealsense::get_string(profile->get_format()) << ";"
            << "fps=" << profile->get_framerate();
        
        write_string(topic, timestamp, payload);
    }

    void ros2_writer::write_streaming_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<video_stream_profile_interface> profile)
    {
        write_stream_info(timestamp, sensor_id, profile);
        auto topic = ros2_topic::video_stream_info_topic({ sensor_id.device_index, sensor_id.sensor_index, profile->get_stream_type(), static_cast<uint32_t>(profile->get_stream_index()) });
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

        std::string topic = ros2_topic::imu_intrinsic_topic({ sensor_id.device_index, sensor_id.sensor_index, profile->get_stream_type(), static_cast<uint32_t>(profile->get_stream_index()) });
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
                    write_vendor_info(ros2_topic::device_info_topic(device_id), timestamp, info);
                }
                else
                {
                    write_vendor_info(ros2_topic::sensor_info_topic({ device_id, sensor_id }), timestamp, info);
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
        write_string(ros2_topic::option_value_topic(sensor_id, type), timestamp, std::to_string(value));
        //Another message for description, should be written once per topic
        if (m_written_options_descriptions[sensor_id.sensor_index].find(type) == m_written_options_descriptions[sensor_id.sensor_index].end())
        {
            const char* desc = option.get_description();
            std::string description = desc ? std::string(desc) : (rsutils::string::from() << "Read only option " << librealsense::get_string(type));
            write_string(ros2_topic::option_description_topic(sensor_id, type), get_static_file_info_timestamp(), description);
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
                write_string(ros2_topic::post_processing_blocks_topic(sensor_id), timestamp, name);
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
