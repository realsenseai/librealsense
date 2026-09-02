// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#include "proc/decimation-filter.h"
#include "proc/rotation-filter.h"
#include "proc/threshold.h"
#include "proc/disparity-transform.h"
#include "proc/spatial-filter.h"
#include "proc/temporal-filter.h"
#include "proc/hole-filling-filter.h"
#include "proc/hdr-merge.h"
#include "proc/sequence-id-filter.h"
#include "ros_writer.h"
#include "core/pose-frame.h"
#include "core/motion-frame.h"
#include <src/core/sensor-interface.h>
#include <src/core/device-interface.h>
#include <src/points.h>
#include <src/labeled-points.h>

#include <rsutils/string/from.h>

namespace librealsense
{
    using namespace device_serializer;

    ros_writer::ros_writer(const std::string& file, bool compress_while_record) : m_file_path(file)
    {
        LOG_INFO("Compression while record is set to " << (compress_while_record ? "ON" : "OFF"));
        m_bag.open(file, rosbag::BagMode::Write);
        if (compress_while_record)
        {
            m_bag.setCompression(rosbag::CompressionType::LZ4);
        }
        write_file_version();
    }

    void ros_writer::write_device_description(const librealsense::device_snapshot& device_description)
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

    void ros_writer::write_frame(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_holder&& frame)
    {
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

        if (Is<pose_frame>(frame.frame))
        {
            write_pose_frame(stream_id, timestamp, std::move(frame));
            return;
        }

        if (Is<labeled_points>(frame.frame))
        {
            write_labeled_points_frame(stream_id, timestamp, std::move(frame));
            return;
        }
    }

    void ros_writer::write_snapshot(uint32_t device_index, const nanoseconds& timestamp, rs2_extension type, const std::shared_ptr<extension_snapshot>& snapshot)
    {
        write_extension_snapshot(device_index, -1, timestamp, type, snapshot);
    }

    void ros_writer::write_snapshot(const sensor_identifier& sensor_id, const nanoseconds& timestamp, rs2_extension type, const std::shared_ptr<extension_snapshot>& snapshot)
    {
        write_extension_snapshot(sensor_id.device_index, sensor_id.sensor_index, timestamp, type, snapshot);
    }

    const std::string& ros_writer::get_file_name() const
    {
        return m_file_path;
    }

    void ros_writer::write_file_version()
    {
        std_msgs::UInt32 msg;
        msg.data = get_file_version();
        write_message(ros_topic::file_version_topic(), get_static_file_info_timestamp(), msg);
    }

    void ros_writer::write_frame_metadata(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_interface* frame)
    {
        auto metadata_topic = ros_topic::frame_metadata_topic(stream_id);
        diagnostic_msgs::KeyValue system_time;
        system_time.key = SYSTEM_TIME_MD_STR;
        system_time.value = std::to_string(frame->get_frame_system_time());
        write_message(metadata_topic, timestamp, system_time);

        diagnostic_msgs::KeyValue timestamp_domain;
        timestamp_domain.key = TIMESTAMP_DOMAIN_MD_STR;
        timestamp_domain.value = librealsense::get_string(frame->get_frame_timestamp_domain());
        write_message(metadata_topic, timestamp, timestamp_domain);

        for (int i = 0; i < static_cast<rs2_frame_metadata_value>(rs2_frame_metadata_value::RS2_FRAME_METADATA_COUNT); i++)
        {
            rs2_frame_metadata_value type = static_cast<rs2_frame_metadata_value>(i);
            rs2_metadata_type md;
            if (frame->find_metadata(type, &md))
            {
                diagnostic_msgs::KeyValue md_msg;
                md_msg.key = librealsense::get_string(type);
                md_msg.value = std::to_string(md);
                write_message(metadata_topic, timestamp, md_msg);
            }
        }
    }

    void ros_writer::write_extrinsics(const stream_identifier& stream_id, frame_interface* frame)
    {
        if (m_extrinsics_msgs.find(stream_id) != m_extrinsics_msgs.end())
        {
            return; //already wrote it
        }
        auto& dev = frame->get_sensor()->get_device();
        uint32_t reference_id = 0;
        rs2_extrinsics ext;
        std::tie(reference_id, ext) = dev.get_extrinsics(*frame->get_stream());
        geometry_msgs::Transform tf_msg;
        convert(ext, tf_msg);
        write_message(ros_topic::stream_extrinsic_topic(stream_id, reference_id), get_static_file_info_timestamp(), tf_msg);
        m_extrinsics_msgs[stream_id] = tf_msg;
    }

    realsense_msgs::Notification ros_writer::to_notification_msg(const notification& n)
    {
        realsense_msgs::Notification noti_msg;
        noti_msg.category = get_string(n.category);
        noti_msg.severity = get_string(n.severity);
        noti_msg.description = n.description;
        auto secs = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::duration<double, std::nano>(n.timestamp));
        noti_msg.timestamp = rs2rosinternal::Time(secs.count());
        noti_msg.serialized_data = n.serialized_data;
        return noti_msg;
    }

    void ros_writer::write_notification(const sensor_identifier& sensor_id, const nanoseconds& timestamp, const notification& n)
    {
        realsense_msgs::Notification noti_msg = to_notification_msg(n);
        write_message(ros_topic::notification_topic({ sensor_id.device_index, sensor_id.sensor_index }, n.category), timestamp, noti_msg);
    }

    void ros_writer::write_additional_frame_messages(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_interface* frame)
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

    void ros_writer::write_video_frame(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_holder&& frame)
    {
        sensor_msgs::Image image;
        auto vid_frame = dynamic_cast<librealsense::video_frame*>(frame.frame);
        if (!vid_frame)
            throw std::runtime_error("Frame is not video frame");

        image.width = static_cast<uint32_t>(vid_frame->get_width());
        image.height = static_cast<uint32_t>(vid_frame->get_height());
        image.step = static_cast<uint32_t>(vid_frame->get_stride());
        convert(vid_frame->get_stream()->get_format(), image.encoding);
        image.is_bigendian = is_big_endian();
        auto size = vid_frame->get_stride() * vid_frame->get_height();
        auto p_data = vid_frame->get_frame_data();
        image.data.assign(p_data, p_data + size);
        image.header.seq = static_cast<uint32_t>(vid_frame->get_frame_number());
        std::chrono::duration<double, std::milli> timestamp_ms(vid_frame->get_frame_timestamp());
        image.header.stamp = rs2rosinternal::Time(std::chrono::duration<double>(timestamp_ms).count());
        image.header.version = "1"; // the field is unused and therefore assigned for ROSbag versions control
        auto df = dynamic_cast<librealsense::depth_frame*>(frame.frame);
        if(df)
            image.depth_units = df->get_units();
        auto image_topic = ros_topic::frame_data_topic(stream_id);
        write_message(image_topic, timestamp, image);
        write_additional_frame_messages(stream_id, timestamp, frame);
    }

    void ros_writer::write_motion_frame(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_holder&& frame)
    {
        sensor_msgs::Imu imu_msg;
        if (!frame)
        {
            throw io_exception("Null frame passed to write_motion_frame");
        }

        imu_msg.header.seq = static_cast<uint32_t>(frame.frame->get_frame_number());
        std::chrono::duration<double, std::milli> timestamp_ms(frame.frame->get_frame_timestamp());
        imu_msg.header.stamp = rs2rosinternal::Time(std::chrono::duration<double>(timestamp_ms).count());
        imu_msg.header.version = "1"; // the field is unused and therefore assigned for ROSbag versions control
        auto data_ptr = reinterpret_cast<const float*>(frame.frame->get_frame_data());
        if (stream_id.stream_type == RS2_STREAM_ACCEL)
        {
            imu_msg.linear_acceleration.x = data_ptr[0];
            imu_msg.linear_acceleration.y = data_ptr[1];
            imu_msg.linear_acceleration.z = data_ptr[2];
        }

        else if (stream_id.stream_type == RS2_STREAM_GYRO)
        {
            imu_msg.angular_velocity.x = data_ptr[0];
            imu_msg.angular_velocity.y = data_ptr[1];
            imu_msg.angular_velocity.z = data_ptr[2];
        }
        else if (stream_id.stream_type == RS2_STREAM_MOTION)
        {
            auto data_imu = *reinterpret_cast<const rs2_combined_motion*>(frame.frame->get_frame_data());
            // orientation part
            imu_msg.orientation.x = data_imu.orientation.x;
            imu_msg.orientation.y = data_imu.orientation.y;
            imu_msg.orientation.z = data_imu.orientation.z;
            imu_msg.orientation.w = data_imu.orientation.w;
            // GYRO part
            imu_msg.angular_velocity.x = data_imu.angular_velocity.x;
            imu_msg.angular_velocity.y = data_imu.angular_velocity.y;
            imu_msg.angular_velocity.z = data_imu.angular_velocity.z;
            // ACCEL part
            imu_msg.linear_acceleration.x = data_imu.linear_acceleration.x;
            imu_msg.linear_acceleration.y = data_imu.linear_acceleration.y;
            imu_msg.linear_acceleration.z = data_imu.linear_acceleration.z;
        }
        else
        {
            throw io_exception("Unsupported stream type for a motion frame");
        }

        auto topic = ros_topic::frame_data_topic(stream_id);
        write_message(topic, timestamp, imu_msg);
        write_additional_frame_messages(stream_id, timestamp, frame);
    }

    inline geometry_msgs::Vector3 ros_writer::to_vector3(const float3& f)
    {
        geometry_msgs::Vector3 v;
        v.x = f.x;
        v.y = f.y;
        v.z = f.z;
        return v;
    }

    inline geometry_msgs::Quaternion ros_writer::to_quaternion(const float4& f)
    {
        geometry_msgs::Quaternion q;
        q.x = f.x;
        q.y = f.y;
        q.z = f.z;
        q.w = f.w;
        return q;
    }

    void ros_writer::write_pose_frame(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_holder&& frame)
    {
        auto pose = As<librealsense::pose_frame>(frame.frame);
        if (!frame)
        {
            throw io_exception("Null frame passed to write_motion_frame");
        }
        auto rotation = pose->get_rotation();

        geometry_msgs::Transform transform;
        geometry_msgs::Accel accel;
        geometry_msgs::Twist twist;

        transform.rotation = to_quaternion(pose->get_rotation());
        transform.translation = to_vector3(pose->get_translation());
        accel.angular = to_vector3(pose->get_angular_acceleration());
        accel.linear = to_vector3(pose->get_acceleration());
        twist.angular = to_vector3(pose->get_angular_velocity());
        twist.linear = to_vector3(pose->get_velocity());

        std::string transform_topic = ros_topic::pose_transform_topic(stream_id);
        std::string accel_topic = ros_topic::pose_accel_topic(stream_id);
        std::string twist_topic = ros_topic::pose_twist_topic(stream_id);

        //Write the the pose frame as 3 separate messages (each with different topic)
        write_message(transform_topic, timestamp, transform);
        write_message(accel_topic, timestamp, accel);
        write_message(twist_topic, timestamp, twist);

        // Write the pose confidence as metadata for the pose frame
        std::string md_topic = ros_topic::frame_metadata_topic(stream_id);

        diagnostic_msgs::KeyValue tracker_confidence_msg;
        tracker_confidence_msg.key = TRACKER_CONFIDENCE_MD_STR;
        tracker_confidence_msg.value = std::to_string(pose->get_tracker_confidence());
        write_message(md_topic, timestamp, tracker_confidence_msg);

        diagnostic_msgs::KeyValue mapper_confidence_msg;
        mapper_confidence_msg.key = MAPPER_CONFIDENCE_MD_STR;
        mapper_confidence_msg.value = std::to_string(pose->get_mapper_confidence());
        write_message(md_topic, timestamp, mapper_confidence_msg);

        //Write frame's timestamp as metadata
        diagnostic_msgs::KeyValue frame_timestamp_msg;
        frame_timestamp_msg.key = FRAME_TIMESTAMP_MD_STR;
        frame_timestamp_msg.value = rsutils::string::from() << std::hexfloat << std::fixed << pose->get_frame_timestamp();
        write_message(md_topic, timestamp, frame_timestamp_msg);

        //Write frame's number as external param
        diagnostic_msgs::KeyValue frame_num_msg;
        frame_num_msg.key = FRAME_NUMBER_MD_STR;
        frame_num_msg.value = rsutils::string::from( pose->get_frame_number() );
        write_message(md_topic, timestamp, frame_num_msg);

        // Write the rest of the frame metadata and stream extrinsics
        write_additional_frame_messages(stream_id, timestamp, frame);
    }

    void ros_writer::write_labeled_points_frame(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_holder&& frame)
    {
        sensor_msgs::Image image;

        auto labeled_points_frame = dynamic_cast<librealsense::labeled_points*>(frame.frame);
        if (!labeled_points_frame) 
            throw invalid_value_exception("null pointer recieved from dynamic pointer casting.");

        convert(RS2_FORMAT_Y8, image.encoding);
        image.is_bigendian = is_big_endian();
        auto size = frame->get_frame_data_size();
        auto p_data = frame->get_frame_data();
        image.data.assign(p_data, p_data + size);
        image.header.seq = static_cast<uint32_t>(frame->get_frame_number());
        std::chrono::duration<double, std::milli> timestamp_ms(frame->get_frame_timestamp());
        image.header.stamp = rs2rosinternal::Time(std::chrono::duration<double>(timestamp_ms).count());
        image.header.version = "1"; // the field is unused and therefore assigned for ROSbag versions control
        auto image_topic = ros_topic::frame_data_topic(stream_id);
        write_message(image_topic, timestamp, image);
        write_additional_frame_messages(stream_id, timestamp, frame);
    }

    void ros_writer::write_stream_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<stream_profile_interface> profile)
    {
        realsense_msgs::StreamInfo stream_info_msg;
        stream_info_msg.is_recommended = profile->get_tag() & profile_tag::PROFILE_TAG_DEFAULT;
        convert(profile->get_format(), stream_info_msg.encoding);
        stream_info_msg.fps = profile->get_framerate();
        write_message(ros_topic::stream_info_topic({ sensor_id.device_index, sensor_id.sensor_index, profile->get_stream_type(), static_cast<uint32_t>(profile->get_stream_index()) }), timestamp, stream_info_msg);
    }

    void ros_writer::write_streaming_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<video_stream_profile_interface> profile)
    {
        write_stream_info(timestamp, sensor_id, profile);

        sensor_msgs::CameraInfo camera_info_msg;
        camera_info_msg.width = profile->get_width();
        camera_info_msg.height = profile->get_height();
        rs2_intrinsics intrinsics{};
        try {
            intrinsics = profile->get_intrinsics();
        }
        catch (...)
        {
            LOG_ERROR("Error trying to get intrinsc data for stream " << profile->get_stream_type() << ", " << profile->get_stream_index());
        }
        camera_info_msg.K[0] = intrinsics.fx;
        camera_info_msg.K[2] = intrinsics.ppx;
        camera_info_msg.K[4] = intrinsics.fy;
        camera_info_msg.K[5] = intrinsics.ppy;
        camera_info_msg.K[8] = 1;
        camera_info_msg.D.assign(std::begin(intrinsics.coeffs), std::end(intrinsics.coeffs));
        camera_info_msg.distortion_model = rs2_distortion_to_string(intrinsics.model);
        write_message(ros_topic::video_stream_info_topic({ sensor_id.device_index, sensor_id.sensor_index, profile->get_stream_type(), static_cast<uint32_t>(profile->get_stream_index()) }), timestamp, camera_info_msg);
    }

    void ros_writer::write_streaming_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<motion_stream_profile_interface> profile)
    {
        write_stream_info(timestamp, sensor_id, profile);

        realsense_msgs::ImuIntrinsic motion_info_msg;

        rs2_motion_device_intrinsic intrinsics{};
        try {
            intrinsics = profile->get_intrinsics();
        }
        catch (...)
        {
            LOG_ERROR("Error trying to get intrinsc data for stream " << profile->get_stream_type() << ", " << profile->get_stream_index());
        }
        //Writing empty in case of failure
        std::copy(&intrinsics.data[0][0], &intrinsics.data[0][0] + motion_info_msg.data.size(), std::begin(motion_info_msg.data));
        std::copy(std::begin(intrinsics.bias_variances), std::end(intrinsics.bias_variances), std::begin(motion_info_msg.bias_variances));
        std::copy(std::begin(intrinsics.noise_variances), std::end(intrinsics.noise_variances), std::begin(motion_info_msg.noise_variances));

        std::string topic = ros_topic::imu_intrinsic_topic({ sensor_id.device_index, sensor_id.sensor_index, profile->get_stream_type(), static_cast<uint32_t>(profile->get_stream_index()) });
        write_message(topic, timestamp, motion_info_msg);
    }
    void ros_writer::write_streaming_info(nanoseconds timestamp, const sensor_identifier& sensor_id, std::shared_ptr<pose_stream_profile_interface> profile)
    {
        write_stream_info(timestamp, sensor_id, profile);
    }
    void ros_writer::write_extension_snapshot(uint32_t device_id, const nanoseconds& timestamp, rs2_extension type, std::shared_ptr<librealsense::extension_snapshot> snapshot)
    {
        const auto ignored = 0u;
        write_extension_snapshot(device_id, ignored, timestamp, type, snapshot, true);
    }

    void ros_writer::write_extension_snapshot(uint32_t device_id, uint32_t sensor_id, const nanoseconds& timestamp, rs2_extension type, std::shared_ptr<librealsense::extension_snapshot> snapshot)
    {
        write_extension_snapshot(device_id, sensor_id, timestamp, type, snapshot, false);
    }

    void ros_writer::write_extension_snapshot(uint32_t device_id, uint32_t sensor_id, const nanoseconds& timestamp, rs2_extension type, std::shared_ptr<librealsense::extension_snapshot> snapshot, bool is_device)
    {
        switch (type)
        {
        case RS2_EXTENSION_INFO:
        {
            auto info = SnapshotAs<RS2_EXTENSION_INFO>(snapshot);
            if (is_device)
            {
                write_vendor_info(ros_topic::device_info_topic(device_id), timestamp, info);
            }
            else
            {
                write_vendor_info(ros_topic::sensor_info_topic({ device_id, sensor_id }), timestamp, info);
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
        case RS2_EXTENSION_POSE_PROFILE:
        {
            auto profile = SnapshotAs<RS2_EXTENSION_POSE_PROFILE>(snapshot);
            write_streaming_info(timestamp, { device_id, sensor_id }, profile);
            break;
        }
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

    void ros_writer::write_vendor_info(const std::string& topic, nanoseconds timestamp, std::shared_ptr<info_interface> info_snapshot)
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(RS2_CAMERA_INFO_COUNT); i++)
        {
            auto camera_info = static_cast<rs2_camera_info>(i);
            if (info_snapshot->supports_info(camera_info))
            {
                diagnostic_msgs::KeyValue msg;
                msg.key = rs2_camera_info_to_string(camera_info);
                msg.value = info_snapshot->get_info(camera_info);
                write_message(topic, timestamp, msg);
            }
        }
    }

    void ros_writer::write_sensor_option(device_serializer::sensor_identifier sensor_id, const nanoseconds& timestamp, rs2_option type, const librealsense::option& option)
    {
        float value = option.query();
        const char* str = option.get_description();
        std::string description = str ? std::string(str) : (rsutils::string::from() << "Read only option of " << librealsense::get_string(type));

        //One message for value
        std_msgs::Float32 option_msg;
        option_msg.data = value;
        write_message(ros_topic::option_value_topic(sensor_id, type), timestamp, option_msg);

        //Another message for description, should be written once per topic

        if (m_written_options_descriptions[sensor_id.sensor_index].find(type) == m_written_options_descriptions[sensor_id.sensor_index].end())
        {
            std_msgs::String option_msg_desc;
            option_msg_desc.data = description;
            write_message(ros_topic::option_description_topic(sensor_id, type), get_static_file_info_timestamp(), option_msg_desc);
            m_written_options_descriptions[sensor_id.sensor_index].insert(type);
        }
    }

    void ros_writer::write_sensor_options(device_serializer::sensor_identifier sensor_id, const nanoseconds& timestamp, std::shared_ptr<options_interface> options)
    {
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

    void ros_writer::write_sensor_processing_blocks(device_serializer::sensor_identifier sensor_id, const nanoseconds& timestamp, std::shared_ptr<recommended_proccesing_blocks_interface> proccesing_blocks)
    {
        for (auto block : proccesing_blocks->get_recommended_processing_blocks())
        {
            std::string name = get_processing_block_extension_name( block );
            if( name.empty() )
            {
                LOG_WARNING( "Failed to get recommended processing block name for sensor " << sensor_id.sensor_index );
                continue;
            }
            try
            {
                std_msgs::String processing_block_msg;
                processing_block_msg.data = name;
                write_message( ros_topic::post_processing_blocks_topic( sensor_id ), timestamp, processing_block_msg );
            }
            catch( std::exception & e )
            {
                LOG_WARNING( "Failed to write processing block '" << name << "' for sensor " << sensor_id.sensor_index
                                                                  << ": " << e.what() );
            }
        }
    }

    uint8_t ros_writer::is_big_endian()
    {
        int num = 1;
        return (*reinterpret_cast<char*>(&num) == 1) ? 0 : 1; //Little Endian: (char)0x0001 => 0x01, Big Endian: (char)0x0001 => 0x00,
    }
}
