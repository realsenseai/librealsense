// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019-24 RealSense, Inc. All Rights Reserved.

#include "ros_reader.h"
#include "ds/ds-device-common.h"
#include "ds/d400/d400-private.h"
#include "ds/d500/d500-private.h"
#include "std_msgs/Float32MultiArray.h"
#include <src/depth-sensor.h>
#include <src/core/pose-frame.h>
#include <src/core/motion-frame.h>
#include <src/core/video-frame.h>
#include <src/color-sensor.h>
#include <src/safety-sensor.h>
#include <src/depth-mapping-sensor.h>
#include <src/points.h>
#include <src/labeled-points.h>
#include <src/context.h>

#include <rsutils/string/from.h>
#include <cstring>


namespace librealsense
{
    using namespace device_serializer;

    ros_reader::ros_reader(const std::string& file, const std::shared_ptr<context>& ctx) :
        m_metadata_parser_map(md_constant_parser::create_metadata_parser_map()),
        m_total_duration(0),
        m_file_path(file),
        m_context(ctx),
        m_version(0),
        m_legacy_depth_units(0)
    {
        try
        {
            reset(); //Note: calling a virtual function inside c'tor, safe while base function is pure virtual
            m_total_duration = get_file_duration(m_file, m_version);
        }
        catch (const std::exception& e)
        {
            //Rethrowing with better clearer message
            throw io_exception( rsutils::string::from() << "Failed to create ros reader: " << e.what() );
        }
    }

    device_snapshot ros_reader::query_device_description(const nanoseconds& time)
    {
        return read_device_description(time);
    }

    std::shared_ptr<serialized_data> ros_reader::read_next_data()
    {
        if (m_samples_view == nullptr || m_samples_itrator == m_samples_view->end())
        {
            LOG_DEBUG("End of file reached");
            return std::make_shared<serialized_end_of_file>();
        }

        rosbag::MessageInstance next_msg = *m_samples_itrator;
        ++m_samples_itrator;

        if (next_msg.isType<sensor_msgs::Image>()
            || next_msg.isType<sensor_msgs::Imu>()
            || next_msg.isType<realsense_legacy_msgs::pose>()
            || next_msg.isType<geometry_msgs::Transform>())
        {
            LOG_DEBUG("Next message is a frame");
            return create_frame(next_msg);
        }

        if (m_version >= 3)
        {
            if (next_msg.isType<std_msgs::Float32>())
            {
                LOG_DEBUG("Next message is an option");
                auto timestamp = to_nanoseconds(next_msg.getTime());
                auto sensor_id = ros_topic::get_sensor_identifier(next_msg.getTopic());
                auto option = create_option(m_file, next_msg);
                return std::make_shared<serialized_option>(timestamp, sensor_id, option.first, option.second);
            }

            if (next_msg.isType<realsense_msgs::Notification>())
            {
                LOG_DEBUG("Next message is a notification");
                auto timestamp = to_nanoseconds(next_msg.getTime());
                auto sensor_id = ros_topic::get_sensor_identifier(next_msg.getTopic());
                auto notification = create_notification(m_file, next_msg);
                return std::make_shared<serialized_notification>(timestamp, sensor_id, notification);
            }
        }

        std::string err_msg = rsutils::string::from() << "Unknown message type: " << next_msg.getDataType()
                                                      << "(Topic: " << next_msg.getTopic() << ")";
        LOG_ERROR(err_msg);
        throw invalid_value_exception(err_msg);
    }

    void ros_reader::seek_to_time(const nanoseconds& seek_time)
    {
        if (seek_time > m_total_duration)
        {
            throw invalid_value_exception( rsutils::string::from()
                                           << "Requested time is out of playback length. (Requested = "
                                           << seek_time.count() << ", Duration = " << m_total_duration.count() << ")" );
        }
        auto seek_time_as_secs = std::chrono::duration_cast<std::chrono::duration<double>>(seek_time);
        auto seek_time_as_rostime = rs2rosinternal::Time(seek_time_as_secs.count());

        m_samples_view.reset(new rosbag::View(m_file, FalseQuery()));

        //Using cached topics here and not querying them (before reseting) since a previous call to seek
        // could have changed the view and some streams that should be streaming were dropped.
        //E.g:  Recording Depth+Color, stopping Depth, starting IR, stopping IR and Color. Play IR+Depth: will play only depth, then only IR, then we seek to a point only IR was streaming, and then to 0.
        for (auto topic : m_enabled_streams_topics)
        {
            m_samples_view->addQuery(m_file, rosbag::TopicQuery(topic), seek_time_as_rostime);
        }
        m_samples_itrator = m_samples_view->begin();
    }

    std::vector<std::shared_ptr<serialized_data>> ros_reader::fetch_last_frames(const nanoseconds& seek_time)
    {
        std::vector<std::shared_ptr<serialized_data>> result;
        rosbag::View view(m_file, FalseQuery());
        auto as_rostime = to_rostime(seek_time);
        auto start_time = to_rostime(get_static_file_info_timestamp());

        for (auto topic : m_enabled_streams_topics)
        {
            view.addQuery(m_file, rosbag::TopicQuery(topic), start_time, as_rostime);
        }
        std::map<device_serializer::stream_identifier, rs2rosinternal::Time> last_frames;
        for (auto&& m : view)
        {
            if (m.isType<sensor_msgs::Image>() || m.isType<sensor_msgs::Imu>())
            {
                auto id = ros_topic::get_stream_identifier(m.getTopic());
                last_frames[id] = m.getTime();
            }
        }
        for (auto&& kvp : last_frames)
        {
            auto topic = ros_topic::frame_data_topic(kvp.first);
            rosbag::View view(m_file, rosbag::TopicQuery(topic), kvp.second, kvp.second);
            auto msg = view.begin();
            auto new_frame = create_frame(*msg);
            result.push_back(new_frame);
        }
        return result;
    }
    nanoseconds ros_reader::query_duration() const
    {
        return m_total_duration;
    }

    void ros_reader::reset()
    {
        m_file.close();
        m_file.open(m_file_path, rosbag::BagMode::Read);
        m_version = read_file_version(m_file);
        m_samples_view = nullptr;
        m_frame_source = std::make_shared<frame_source>(m_version == 1 ? 128 : 32);
        m_frame_source->init(m_metadata_parser_map);
        m_initial_device_description = read_device_description(get_static_file_info_timestamp(), true);
    }

    void ros_reader::enable_stream(const std::vector<device_serializer::stream_identifier>& stream_ids)
    {
        rs2rosinternal::Time start_time = rs2rosinternal::TIME_MIN + rs2rosinternal::Duration{ 0, 1 }; //first non 0 timestamp and afterward
        if (m_samples_view == nullptr) //Starting to stream
        {
            m_samples_view = std::unique_ptr<rosbag::View>(new rosbag::View(m_file, FalseQuery()));
            m_samples_view->addQuery(m_file, OptionsQuery(), start_time);
            m_samples_view->addQuery(m_file, NotificationsQuery(), start_time);
            m_samples_itrator = m_samples_view->begin();
        }
        else //Already streaming
        {
            if (m_samples_itrator != m_samples_view->end())
            {
                rosbag::MessageInstance sample_msg = *m_samples_itrator;
                start_time = sample_msg.getTime();
            }
        }
        auto currently_streaming = get_topics(m_samples_view);
        //empty the view
        m_samples_view = std::unique_ptr<rosbag::View>(new rosbag::View(m_file, FalseQuery()));

        for (auto&& stream_id : stream_ids)
        {
            //add new stream to view
            if (m_version == legacy_file_format::file_version())
            {
                m_samples_view->addQuery(m_file, legacy_file_format::StreamQuery(stream_id), start_time);
            }
            else
            {
                m_samples_view->addQuery(m_file, StreamQuery(stream_id), start_time);
            }
        }

        //add already existing streams
        for (auto topic : currently_streaming)
        {
            m_samples_view->addQuery(m_file, rosbag::TopicQuery(topic), start_time);
        }
        m_samples_itrator = m_samples_view->begin();
        m_enabled_streams_topics = get_topics(m_samples_view);
    }

    void ros_reader::disable_stream(const std::vector<device_serializer::stream_identifier>& stream_ids)
    {
        if (m_samples_view == nullptr)
        {
            return;
        }
        rs2rosinternal::Time curr_time;
        if (m_samples_itrator == m_samples_view->end())
        {
            curr_time = m_samples_view->getEndTime();
        }
        else
        {
            rosbag::MessageInstance sample_msg = *m_samples_itrator;
            curr_time = sample_msg.getTime();
        }
        auto currently_streaming = get_topics(m_samples_view);
        m_samples_view = std::unique_ptr<rosbag::View>(new rosbag::View(m_file, FalseQuery()));
        for (auto topic : currently_streaming)
        {
            //Find if this topic is one of the streams that should be disabled
            auto it = std::find_if(stream_ids.begin(), stream_ids.end(), [&topic](const device_serializer::stream_identifier& s) {
                //return topic.starts_with(s);
                return topic.find(ros_topic::stream_full_prefix(s)) != std::string::npos;
            });
            bool should_topic_remain = (it == stream_ids.end());
            if (should_topic_remain)
            {
                m_samples_view->addQuery(m_file, rosbag::TopicQuery(topic), curr_time);
            }
        }
        m_samples_itrator = m_samples_view->begin();
        m_enabled_streams_topics = get_topics(m_samples_view);
    }

    const std::string& ros_reader::get_file_name() const
    {
        return m_file_path;
    }

    std::shared_ptr<serialized_frame> ros_reader::create_frame(const rosbag::MessageInstance& msg)
    {
        auto next_msg_topic = msg.getTopic();
        auto next_msg_time = msg.getTime();

        nanoseconds timestamp = to_nanoseconds(next_msg_time);
        stream_identifier stream_id;
        if (m_version == legacy_file_format::file_version())
        {
            stream_id = legacy_file_format::get_stream_identifier(next_msg_topic);
        }
        else
        {
            stream_id = ros_topic::get_stream_identifier(next_msg_topic);
        }
        frame_holder frame{ nullptr };
        if (msg.isType<sensor_msgs::Image>())
        {
            frame = create_image_from_message(msg);
        }
        else if (msg.isType<sensor_msgs::Imu>())
        {
            frame = create_motion_sample(msg);
        }
        else if (msg.isType<realsense_legacy_msgs::pose>() || msg.isType<geometry_msgs::Transform>())
        {
            frame = create_pose_sample(msg);
        }
        else
        {
            std::string err_msg = rsutils::string::from()
                               << "Unknown frame type: " << msg.getDataType() << "(Topic: " << next_msg_topic << ")";
            LOG_ERROR(err_msg);
            throw invalid_value_exception(err_msg);
        }

        if (frame.frame == nullptr)
        {
            return std::make_shared<serialized_invalid_frame>(timestamp, stream_id);
        }
        return std::make_shared<serialized_frame>(timestamp, stream_id, std::move(frame));
    }

    nanoseconds ros_reader::get_file_duration(const rosbag::Bag& file, uint32_t version)
    {
        std::function<bool(rosbag::ConnectionInfo const* info)> query;
        if (version == legacy_file_format::file_version())
            query = legacy_file_format::FrameQuery();
        else
            query = FrameQuery();
        rosbag::View all_frames_view(file, query);
        auto streaming_duration = all_frames_view.getEndTime() - all_frames_view.getBeginTime();
        return nanoseconds(streaming_duration.toNSec());
    }

    void ros_reader::get_legacy_frame_metadata(const rosbag::Bag& bag,
        const device_serializer::stream_identifier& stream_id,
        const rosbag::MessageInstance &msg,
        frame_additional_data& additional_data)
    {
        uint32_t total_md_size = 0;
        rosbag::View frame_metadata_view(bag, legacy_file_format::FrameInfoExt(stream_id), msg.getTime(), msg.getTime());
        assert(frame_metadata_view.size() <= 1);
        for (auto message_instance : frame_metadata_view)
        {
            auto info = instantiate_msg<realsense_legacy_msgs::frame_info>(message_instance);
            for (auto&& fmd : info->frame_metadata)
            {
                if (fmd.type == legacy_file_format::SYSTEM_TIMESTAMP)
                {
                    additional_data.system_time = *reinterpret_cast<const int64_t*>(fmd.data.data());
                }
                else
                {
                    rs2_frame_metadata_value type;
                    if (!legacy_file_format::convert_metadata_type(fmd.type, type))
                    {
                        continue;
                    }
                    rs2_metadata_type value = *reinterpret_cast<const rs2_metadata_type*>(fmd.data.data());
                    auto size_of_enum = sizeof(rs2_frame_metadata_value);
                    auto size_of_data = sizeof(rs2_metadata_type);
                    if (total_md_size + size_of_enum + size_of_data > 255)
                    {
                        continue; //stop adding metadata to frame
                    }
                    memcpy(additional_data.metadata_blob.data() + total_md_size, &type, size_of_enum);
                    total_md_size += static_cast<uint32_t>(size_of_enum);
                    memcpy(additional_data.metadata_blob.data() + total_md_size, &value, size_of_data);
                    total_md_size += static_cast<uint32_t>(size_of_data);
                }
            }

            try
            {
                additional_data.timestamp_domain = legacy_file_format::convert(info->time_stamp_domain);
            }
            catch (const std::exception& e)
            {
                LOG_WARNING("Failed to get timestamp_domain. Error: " << e.what());
            }
        }
    }

    std::map<std::string, std::string> ros_reader::get_frame_metadata(const rosbag::Bag& bag,
        const std::string& topic,
        const device_serializer::stream_identifier& stream_id,
        const rosbag::MessageInstance &msg,
        frame_additional_data& additional_data)
    {
        uint32_t total_md_size = 0;
        std::map<std::string, std::string> remaining;
        rosbag::View frame_metadata_view(bag, rosbag::TopicQuery(topic), msg.getTime(), msg.getTime());

        for (auto message_instance : frame_metadata_view)
        {
            auto key_val_msg = instantiate_msg<diagnostic_msgs::KeyValue>(message_instance);
            if (key_val_msg->key == TIMESTAMP_DOMAIN_MD_STR)
            {
                if (!safe_convert(key_val_msg->value, additional_data.timestamp_domain))
                {
                    remaining[key_val_msg->key] = key_val_msg->value;
                }
            }
            else if (key_val_msg->key == SYSTEM_TIME_MD_STR)
            {
                if (!safe_convert(key_val_msg->value, additional_data.system_time))
                {
                    remaining[key_val_msg->key] = key_val_msg->value;
                }
            }
            else
            {
                rs2_frame_metadata_value type{};
                if (!safe_convert(key_val_msg->key, type))
                {
                    remaining[key_val_msg->key] = key_val_msg->value;
                    continue;
                }
                rs2_metadata_type md;
                if (!safe_convert(key_val_msg->value, md))
                {
                    remaining[key_val_msg->key] = key_val_msg->value;
                    continue;
                }
                auto size_of_enum = sizeof(rs2_frame_metadata_value);
                auto size_of_data = sizeof(rs2_metadata_type);
               
                memcpy(additional_data.metadata_blob.data() + total_md_size, &type, size_of_enum);
                total_md_size += static_cast<uint32_t>(size_of_enum);
                memcpy(additional_data.metadata_blob.data() + total_md_size, &md, size_of_data);
                total_md_size += static_cast<uint32_t>(size_of_data);
            }
        }
        additional_data.metadata_size = total_md_size;
        return remaining;
    }


    frame_holder ros_reader::create_image_from_message(const rosbag::MessageInstance &image_data) const
    {
        LOG_DEBUG("Trying to create an image frame from message");
        auto msg = instantiate_msg<sensor_msgs::Image>(image_data);
        frame_additional_data additional_data{};
        std::chrono::duration<double, std::milli> timestamp_ms(std::chrono::duration<double>(msg->header.stamp.toSec()));
        additional_data.timestamp = timestamp_ms.count();
        additional_data.frame_number = msg->header.seq;
        additional_data.fisheye_ae_mode = false;
        if (msg->depth_units)
            additional_data.depth_units = msg->depth_units;
        else
            additional_data.depth_units = m_legacy_depth_units; // for old rosbag

        stream_identifier stream_id;
        if (m_version == legacy_file_format::file_version())
        {
            //Version 1 legacy
            stream_id = legacy_file_format::get_stream_identifier(image_data.getTopic());
            get_legacy_frame_metadata(m_file, stream_id, image_data, additional_data);
        }
        else
        {
            //Version 2 and above
            stream_id = ros_topic::get_stream_identifier(image_data.getTopic());
            auto info_topic = ros_topic::frame_metadata_topic(stream_id);
            get_frame_metadata(m_file, info_topic, stream_id, image_data, additional_data);
        }

        frame_interface * frame = m_frame_source->alloc_frame(
            { stream_id.stream_type, stream_id.stream_index, frame_source::stream_to_frame_types( stream_id.stream_type ) },
            msg->data.size(),
            std::move( additional_data ),
            true );

        if (frame == nullptr)
        {
            LOG_WARNING("Failed to allocate new frame");
            return nullptr;
        }
        if (stream_id.stream_type != RS2_STREAM_LABELED_POINT_CLOUD)
        {
            librealsense::video_frame* video_frame = static_cast<librealsense::video_frame*>(frame);
            video_frame->assign(msg->width, msg->height, msg->step, msg->step / msg->width * 8);
            rs2_format stream_format;
            convert(msg->encoding, stream_format);
            //attaching a temp stream to the frame. Playback sensor should assign the real stream
            frame->set_stream( std::make_shared< video_stream_profile >() );
            frame->get_stream()->set_format(stream_format);
            frame->get_stream()->set_stream_index(int(stream_id.stream_index));
            frame->get_stream()->set_stream_type(stream_id.stream_type);
            video_frame->data = std::move(msg->data);
            librealsense::frame_holder fh{ video_frame };
            LOG_DEBUG("Created image frame: " << stream_id << " " << video_frame->get_width() << "x" << video_frame->get_height() << " " << stream_format);

            return fh;
        }
        else
        {
            librealsense::labeled_points* lab_points = static_cast<librealsense::labeled_points*>(frame);
            rs2_format stream_format;
            convert(msg->encoding, stream_format);
            //attaching a temp stream to the frame. Playback sensor should assign the real stream
            frame->set_stream( std::make_shared< video_stream_profile >() );
            frame->get_stream()->set_format(stream_format);
            frame->get_stream()->set_stream_index(int(stream_id.stream_index));
            frame->get_stream()->set_stream_type(stream_id.stream_type);
            lab_points->data = std::move(msg->data);
            librealsense::frame_holder fh{ lab_points };
            LOG_DEBUG("Created image frame: " << stream_id << " " << stream_format);

            return fh;
        }
    }

    frame_holder ros_reader::create_motion_sample(const rosbag::MessageInstance &motion_data) const
    {
        LOG_DEBUG("Trying to create a motion frame from message");

        auto msg = instantiate_msg<sensor_msgs::Imu>(motion_data);

        frame_additional_data additional_data{};
        std::chrono::duration<double, std::milli> timestamp_ms(std::chrono::duration<double>(msg->header.stamp.toSec()));
        additional_data.timestamp = timestamp_ms.count();
        additional_data.frame_number = msg->header.seq;
        additional_data.fisheye_ae_mode = false; //TODO: where should this come from?

        stream_identifier stream_id;
        if (m_version == legacy_file_format::file_version())
        {
            //Version 1 legacy
            stream_id = legacy_file_format::get_stream_identifier(motion_data.getTopic());
            get_legacy_frame_metadata(m_file, stream_id, motion_data, additional_data);
        }
        else
        {
            //Version 2 and above
            stream_id = ros_topic::get_stream_identifier(motion_data.getTopic());
            auto info_topic = ros_topic::frame_metadata_topic(stream_id);
            get_frame_metadata(m_file, info_topic, stream_id, motion_data, additional_data);
        }

        size_t size_of_imu_data = (stream_id.stream_type == RS2_STREAM_MOTION) ? sizeof(rs2_combined_motion) : 3 * sizeof(float);

        frame_interface * frame = m_frame_source->alloc_frame(
            { stream_id.stream_type, stream_id.stream_index, RS2_EXTENSION_MOTION_FRAME },
            size_of_imu_data,
            std::move( additional_data ),
            true );
        if (frame == nullptr)
        {
            LOG_WARNING("Failed to allocate new frame");
            return nullptr;
        }
        librealsense::motion_frame* motion_frame = static_cast<librealsense::motion_frame*>(frame);
        //attaching a temp stream to the frame. Playback sensor should assign the real stream
        frame->set_stream( std::make_shared< motion_stream_profile >() );
        frame->get_stream()->set_format(RS2_FORMAT_MOTION_XYZ32F);
        frame->get_stream()->set_stream_index(stream_id.stream_index);
        frame->get_stream()->set_stream_type(stream_id.stream_type);
        if (stream_id.stream_type == RS2_STREAM_ACCEL)
        {
            auto data = reinterpret_cast<float*>(motion_frame->data.data());
            data[0] = static_cast<float>(msg->linear_acceleration.x);
            data[1] = static_cast<float>(msg->linear_acceleration.y);
            data[2] = static_cast<float>(msg->linear_acceleration.z);
            LOG_DEBUG("RS2_STREAM_ACCEL " << motion_frame);
        }
        else if (stream_id.stream_type == RS2_STREAM_GYRO)
        {
            auto data = reinterpret_cast<float*>(motion_frame->data.data());
            data[0] = static_cast<float>(msg->angular_velocity.x);
            data[1] = static_cast<float>(msg->angular_velocity.y);
            data[2] = static_cast<float>(msg->angular_velocity.z);
            LOG_DEBUG("RS2_STREAM_GYRO " << motion_frame);
        }
        else if (stream_id.stream_type == RS2_STREAM_MOTION)
        {
            auto data = reinterpret_cast<rs2_combined_motion*>(motion_frame->data.data());
            // orientation part
            data->orientation.x = msg->orientation.x;
            data->orientation.y = msg->orientation.y;
            data->orientation.z = msg->orientation.z;
            data->orientation.w = msg->orientation.w;
            // GYRO part
            data->angular_velocity.x = msg->angular_velocity.x;
            data->angular_velocity.y = msg->angular_velocity.y;
            data->angular_velocity.z = msg->angular_velocity.z;
            // ACCEL part
            data->linear_acceleration.x = msg->linear_acceleration.x;
            data->linear_acceleration.y = msg->linear_acceleration.y;
            data->linear_acceleration.z = msg->linear_acceleration.z;
            LOG_DEBUG("RS2_STREAM_MOTION " << motion_frame);
        }
        else
        {
            throw io_exception( rsutils::string::from() << "Unsupported stream type " << stream_id.stream_type );
        }
        librealsense::frame_holder fh{ motion_frame };
        LOG_DEBUG("Created motion frame: " << stream_id);

        return std::move(fh);
    }

    inline float3 ros_reader::to_float3(const geometry_msgs::Vector3& v)
    {
        float3 f{};
        f.x = v.x;
        f.y = v.y;
        f.z = v.z;
        return f;
    }

    inline float4 ros_reader::to_float4(const geometry_msgs::Quaternion& q)
    {
        float4 f{};
        f.x = q.x;
        f.y = q.y;
        f.z = q.z;
        f.w = q.w;
        return f;
    }

    frame_holder ros_reader::create_pose_sample(const rosbag::MessageInstance &msg) const
    {
        LOG_DEBUG("Trying to create a pose frame from message");

        pose_frame::pose_info pose{};
        std::chrono::duration<double, std::milli> timestamp_ms;
        size_t frame_size = sizeof(pose);
        rs2_extension frame_type = RS2_EXTENSION_POSE_FRAME;
        frame_additional_data additional_data{};

        additional_data.fisheye_ae_mode = false;

        stream_identifier stream_id;

        if (m_version == legacy_file_format::file_version())
        {
            auto pose_msg = instantiate_msg<realsense_legacy_msgs::pose>(msg);
            pose.rotation = to_float4(pose_msg->rotation);
            pose.translation = to_float3(pose_msg->translation);
            pose.angular_acceleration = to_float3(pose_msg->angular_acceleration);
            pose.acceleration = to_float3(pose_msg->acceleration);
            pose.angular_velocity = to_float3(pose_msg->angular_velocity);
            pose.velocity = to_float3(pose_msg->velocity);
            //pose.confidence = not supported in legacy format
            timestamp_ms = std::chrono::duration<double, std::milli>(static_cast<double>(pose_msg->timestamp));
        }
        else
        {
            assert(msg.getTopic().find("pose/transform") != std::string::npos); // Assuming that the samples iterator of the reader only reads the pose/transform topic
                                                                                // and we will be reading the rest in here (see ../readme.md#Topics under Pose-Data for more information
            auto transform_msg = instantiate_msg<geometry_msgs::Transform>(msg);

            auto stream_id = ros_topic::get_stream_identifier(msg.getTopic());
            std::string accel_topic = ros_topic::pose_accel_topic(stream_id);
            rosbag::View accel_view(m_file, rosbag::TopicQuery(accel_topic), msg.getTime(), msg.getTime());
            assert(accel_view.size() == 1);
            auto accel_msg = instantiate_msg<geometry_msgs::Accel>(*accel_view.begin());

            std::string twist_topic = ros_topic::pose_twist_topic(stream_id);
            rosbag::View twist_view(m_file, rosbag::TopicQuery(twist_topic), msg.getTime(), msg.getTime());
            assert(twist_view.size() == 1);
            auto twist_msg = instantiate_msg<geometry_msgs::Twist>(*twist_view.begin());

            pose.rotation = to_float4(transform_msg->rotation);
            pose.translation = to_float3(transform_msg->translation);
            pose.angular_acceleration = to_float3(accel_msg->angular);
            pose.acceleration = to_float3(accel_msg->linear);
            pose.angular_velocity = to_float3(twist_msg->angular);
            pose.velocity = to_float3(twist_msg->linear);

        }

        if (m_version == legacy_file_format::file_version())
        {
            //Version 1 legacy
            stream_id = legacy_file_format::get_stream_identifier(msg.getTopic());
            get_legacy_frame_metadata(m_file, stream_id, msg, additional_data);
        }
        else
        {
            //Version 2 and above
            stream_id = ros_topic::get_stream_identifier(msg.getTopic());
            auto info_topic = ros_topic::frame_metadata_topic(stream_id);
            auto remaining = get_frame_metadata(m_file, info_topic, stream_id, msg, additional_data);
            for (auto&& kvp : remaining)
            {
                if (kvp.first == MAPPER_CONFIDENCE_MD_STR)
                {
                    pose.mapper_confidence = std::stoul(kvp.second);
                }
                else if (kvp.first == FRAME_TIMESTAMP_MD_STR)
                {
                    std::istringstream iss(kvp.second);
                    double ts = std::strtod(iss.str().c_str(), NULL);
                    timestamp_ms = std::chrono::duration<double, std::milli>(ts);
                }
                else if (kvp.first == TRACKER_CONFIDENCE_MD_STR)
                {
                    pose.tracker_confidence = std::stoul(kvp.second);
                }
                else if (kvp.first == FRAME_NUMBER_MD_STR)
                {
                    additional_data.frame_number = std::stoul(kvp.second);
                }
            }
        }

        additional_data.timestamp = timestamp_ms.count();

        frame_interface * new_frame = m_frame_source->alloc_frame(
            { stream_id.stream_type, stream_id.stream_index, frame_type },
            frame_size,
            std::move( additional_data ),
            true );

        if (new_frame == nullptr)
        {
            LOG_WARNING("Failed to allocate new frame");
            return nullptr;
        }
        librealsense::pose_frame* pose_frame = static_cast<librealsense::pose_frame*>(new_frame);
        //attaching a temp stream to the frame. Playback sensor should assign the real stream
        new_frame->set_stream( std::make_shared< pose_stream_profile >() );
        new_frame->get_stream()->set_format(RS2_FORMAT_6DOF);
        new_frame->get_stream()->set_stream_index(int(stream_id.stream_index));
        new_frame->get_stream()->set_stream_type(stream_id.stream_type);
        uint8_t * data = pose_frame->data.data();
        memcpy(data, &pose, frame_size);
        frame_holder fh{ new_frame };
        LOG_DEBUG("Created new frame " << frame_type);
        return fh;
    }

    uint32_t ros_reader::read_file_version(const rosbag::Bag& file)
    {
        auto version_topic = ros_topic::file_version_topic();
        rosbag::View view(file, rosbag::TopicQuery(version_topic));

        auto legacy_version_topic = legacy_file_format::file_version_topic();
        rosbag::View legacy_view(file, rosbag::TopicQuery(legacy_version_topic));
        if (legacy_view.size() == 0 && view.size() == 0)
        {
            throw io_exception( rsutils::string::from()
                                << "Invalid file format, file does not contain topic \"" << version_topic << "\" nor \""
                                << legacy_version_topic << "\"" );
        }
        assert((view.size() + legacy_view.size()) == 1); //version message is expected to be a single one
        if (view.size() != 0)
        {
            auto item = *view.begin();
            auto msg = instantiate_msg<std_msgs::UInt32>(item);
            if (msg->data < get_minimum_supported_file_version())
            {
                throw std::runtime_error( rsutils::string::from()
                                          << "Unsupported file version \"" << msg->data << "\"" );
            }
            return msg->data;
        }
        else if (legacy_view.size() != 0)
        {
            auto item = *legacy_view.begin();
            auto msg = instantiate_msg<std_msgs::UInt32>(item);
            if (msg->data > legacy_file_format::get_maximum_supported_legacy_file_version())
            {
                throw std::runtime_error( rsutils::string::from()
                                          << "Unsupported legacy file version \"" << msg->data << "\"" );
            }
            return msg->data;
        }
        throw std::logic_error("Unreachable code path");
    }
    bool ros_reader::try_read_legacy_stream_extrinsic(const stream_identifier& stream_id, uint32_t& group_id, rs2_extrinsics& extrinsic) const
    {
        std::string topic;
        if (stream_id.stream_type == RS2_STREAM_ACCEL || stream_id.stream_type == RS2_STREAM_GYRO)
        {
            topic = rsutils::string::from() << "/camera/rs_motion_stream_info/" << stream_id.sensor_index;
        }
        else if (legacy_file_format::is_camera(stream_id.stream_type))
        {
            topic = rsutils::string::from() << "/camera/rs_stream_info/" << stream_id.sensor_index;
        }
        else
        {
            return false;
        }
        rosbag::View extrinsics_view(m_file, rosbag::TopicQuery(topic));
        if (extrinsics_view.size() == 0)
        {
            return false;
        }
        for (auto&& msg : extrinsics_view)
        {
            if (msg.isType<realsense_legacy_msgs::motion_stream_info>())
            {
                auto msi_msg = instantiate_msg<realsense_legacy_msgs::motion_stream_info>(msg);
                auto parsed_stream_id = legacy_file_format::parse_stream_type(msi_msg->motion_type);
                if (stream_id.stream_type != parsed_stream_id.type || stream_id.stream_index != static_cast<uint32_t>(parsed_stream_id.index))
                {
                    continue;
                }
                std::copy(std::begin(msi_msg->stream_extrinsics.extrinsics.rotation),
                    std::end(msi_msg->stream_extrinsics.extrinsics.rotation),
                    std::begin(extrinsic.rotation));
                std::copy(std::begin(msi_msg->stream_extrinsics.extrinsics.translation),
                    std::end(msi_msg->stream_extrinsics.extrinsics.translation),
                    std::begin(extrinsic.translation));
                group_id = static_cast<uint32_t>(msi_msg->stream_extrinsics.reference_point_id);
                return true;
            }
            else if (msg.isType<realsense_legacy_msgs::stream_info>())
            {
                auto si_msg = instantiate_msg<realsense_legacy_msgs::stream_info>(msg);
                auto parsed_stream_id = legacy_file_format::parse_stream_type(si_msg->stream_type);
                if (stream_id.stream_type != parsed_stream_id.type || stream_id.stream_index != static_cast<uint32_t>(parsed_stream_id.index))
                {
                    continue;
                }
                std::copy(std::begin(si_msg->stream_extrinsics.extrinsics.rotation),
                    std::end(si_msg->stream_extrinsics.extrinsics.rotation),
                    std::begin(extrinsic.rotation));
                std::copy(std::begin(si_msg->stream_extrinsics.extrinsics.translation),
                    std::end(si_msg->stream_extrinsics.extrinsics.translation),
                    std::begin(extrinsic.translation));
                group_id = static_cast<uint32_t>(si_msg->stream_extrinsics.reference_point_id);
                return true;
            }
            else
            {
                throw io_exception( rsutils::string::from()
                                    << "Expected either \"realsense_legacy_msgs::motion_stream_info\" or "
                                       "\"realsense_legacy_msgs::stream_info\", but got "
                                    << msg.getDataType() );
            }
        }
        return false;
    }
    bool ros_reader::try_read_stream_extrinsic(const stream_identifier& stream_id, uint32_t& group_id, rs2_extrinsics& extrinsic) const
    {
        if (m_version == legacy_file_format::file_version())
        {
            return try_read_legacy_stream_extrinsic(stream_id, group_id, extrinsic);
        }
        rosbag::View tf_view(m_file, ExtrinsicsQuery(stream_id));
        if (tf_view.size() == 0)
        {
            return false;
        }
        assert(tf_view.size() == 1); //There should be 1 message per stream
        auto msg = *tf_view.begin();
        auto tf_msg = instantiate_msg<geometry_msgs::Transform>(msg);
        group_id = ros_topic::get_extrinsic_group_index(msg.getTopic());
        convert(*tf_msg, extrinsic);
        return true;
    }

    namespace {

    class depth_sensor_snapshot
        : public virtual depth_sensor
        , public extension_snapshot
    {
    public:
        depth_sensor_snapshot( float depth_units )
            : m_depth_units( depth_units )
        {
        }
        float get_depth_scale() const override { return m_depth_units; }

        void update( std::shared_ptr< extension_snapshot > ext ) override
        {
            if( auto api = As< depth_sensor >( ext ) )
            {
                m_depth_units = api->get_depth_scale();
            }
        }

    protected:
        float m_depth_units;
    };

    class depth_stereo_sensor_snapshot
        : public depth_stereo_sensor
        , public depth_sensor_snapshot
    {
    public:
        depth_stereo_sensor_snapshot( float depth_units, float stereo_bl_mm )
            : depth_sensor_snapshot( depth_units )
            , m_stereo_baseline_mm( stereo_bl_mm )
        {
        }

        float get_stereo_baseline_mm() const override { return m_stereo_baseline_mm; }

        void update( std::shared_ptr< extension_snapshot > ext ) override
        {
            depth_sensor_snapshot::update( ext );

            if( auto api = As< depth_stereo_sensor >( ext ) )
            {
                m_stereo_baseline_mm = api->get_stereo_baseline_mm();
            }
        }

    private:
        float m_stereo_baseline_mm;
    };

    }  // namespace

    void ros_reader::update_sensor_options(const rosbag::Bag& file, uint32_t sensor_index, const nanoseconds& time, uint32_t file_version, snapshot_collection& sensor_extensions, uint32_t version)
    {
        if (version == legacy_file_format::file_version())
        {
            LOG_DEBUG("Not updating options from legacy files");
            return;
        }
        auto sensor_options = read_sensor_options(file, { get_device_index(), sensor_index }, time, file_version);
        sensor_extensions[RS2_EXTENSION_OPTIONS] = sensor_options;

        if (sensor_options->supports_option(RS2_OPTION_DEPTH_UNITS))
        {
            auto&& dpt_opt = sensor_options->get_option(RS2_OPTION_DEPTH_UNITS);
            sensor_extensions[RS2_EXTENSION_DEPTH_SENSOR] = std::make_shared<depth_sensor_snapshot>(dpt_opt.query());

            if (sensor_options->supports_option(RS2_OPTION_STEREO_BASELINE))
            {
                auto&& bl_opt = sensor_options->get_option(RS2_OPTION_STEREO_BASELINE);
                sensor_extensions[RS2_EXTENSION_DEPTH_STEREO_SENSOR] = std::make_shared<depth_stereo_sensor_snapshot>(dpt_opt.query(), bl_opt.query());
            }
        }
    }

    void ros_reader::update_proccesing_blocks(const rosbag::Bag& file, uint32_t sensor_index, const nanoseconds& time, uint32_t file_version, snapshot_collection& sensor_extensions, uint32_t version, std::string pid, std::string sensor_name)
    {
        if (version == legacy_file_format::file_version())
        {
            LOG_DEBUG("Legacy file - processing blocks are not supported");
            return;
        }

        // find depth units from old rosbag
        auto depth_sensor = As<librealsense::depth_sensor>(sensor_extensions.find(RS2_EXTENSION_DEPTH_SENSOR)).get();
        if(depth_sensor)
            m_legacy_depth_units = depth_sensor->get_depth_scale();

        auto options_snapshot = sensor_extensions.find(RS2_EXTENSION_OPTIONS);
        if (options_snapshot == nullptr)
        {
            LOG_WARNING("Recorded file does not contain sensor options");
        }
        auto options_api = As<options_interface>(options_snapshot);
        if (options_api == nullptr)
        {
            throw invalid_value_exception("Failed to get options interface from sensor snapshots");
        }
        auto proccesing_blocks = read_proccesing_blocks( file,
                                                         { get_device_index(), sensor_index },
                                                         time,
                                                         options_api,
                                                         file_version,
                                                         pid,
                                                         sensor_name );
        sensor_extensions[RS2_EXTENSION_RECOMMENDED_FILTERS] = proccesing_blocks;
    }

    namespace {

    class color_sensor_snapshot
        : public virtual color_sensor
        , public extension_snapshot
    {
    public:
        void update( std::shared_ptr< extension_snapshot > ext ) override {}
    };

    class motion_sensor_snapshot
        : public virtual motion_sensor
        , public extension_snapshot
    {
    public:
        void update( std::shared_ptr< extension_snapshot > ext ) override {}
    };

    class fisheye_sensor_snapshot
        : public virtual fisheye_sensor
        , public extension_snapshot
    {
    public:
        void update( std::shared_ptr< extension_snapshot > ext ) override {}
    };

    class safety_sensor_snapshot
        : public virtual safety_sensor
        , public extension_snapshot
    {
    public:
        void update(std::shared_ptr< extension_snapshot > ext) override {}
        std::string get_safety_preset(int index) const override { return ""; }
        void set_safety_preset(int index, const std::string& sp_json_str) const override {}
        std::string get_safety_interface_config(rs2_calib_location loc) const override {return ""; };
        void set_safety_interface_config(const std::string& sic_json_str) const override {};
        std::string get_application_config() const override { return ""; }
        void set_application_config(const std::string& application_config_json_str) const override {}

    };

    class depth_mapping_sensor_snapshot
        : public virtual depth_mapping_sensor
        , public extension_snapshot
    {
    public:
        void update(std::shared_ptr< extension_snapshot > ext) override {}
    };

    }  // namespace


    void ros_reader::add_sensor_extension(snapshot_collection & sensor_extensions, std::string sensor_name)
    {
        if (is_color_sensor(sensor_name))
        {
            sensor_extensions[RS2_EXTENSION_COLOR_SENSOR] = std::make_shared<color_sensor_snapshot>();
        }
        else if( is_motion_module_sensor( sensor_name ) )
        {
            sensor_extensions[RS2_EXTENSION_MOTION_SENSOR] = std::make_shared<motion_sensor_snapshot>();
        }
        else if( is_fisheye_module_sensor( sensor_name ) )
        {
            sensor_extensions[RS2_EXTENSION_FISHEYE_SENSOR] = std::make_shared<fisheye_sensor_snapshot>();
        }
        else if( is_depth_sensor( sensor_name ) )
        {
            if( sensor_extensions.find( RS2_EXTENSION_DEPTH_SENSOR ) == nullptr )
            {
                float depth_units = 0.01f; // Default to 1mm for devices that don't have this option implemented
                sensor_extensions[RS2_EXTENSION_DEPTH_SENSOR] = std::make_shared< depth_sensor_snapshot >( depth_units );

                if( is_stereo_depth_sensor( sensor_name ) ) // Need both extensions
                {
                    if( sensor_extensions.find( RS2_EXTENSION_DEPTH_STEREO_SENSOR ) == nullptr )
                    {
                        float baseline = 0.095f; // Default for D555 (and D455 but D400 have baseline option implemented and won't need this)
                        for( auto & ext : m_extrinsics_map ) // Get real value from extrinsics data, if exists
                        {
                            if( ext.first.stream_type == RS2_STREAM_INFRARED && ext.first.stream_index == 2 )
                                baseline = ext.second.second.translation[0];
                        }
                        sensor_extensions[RS2_EXTENSION_DEPTH_STEREO_SENSOR] = std::make_shared< depth_stereo_sensor_snapshot >( depth_units, baseline );
                    }
                }
            }
        }
        else if (is_safety_module_sensor(sensor_name))
        {
            sensor_extensions[RS2_EXTENSION_SAFETY_SENSOR] = std::make_shared<safety_sensor_snapshot>();
        }
        else if (is_depth_mapping_sensor(sensor_name))
        {
            sensor_extensions[RS2_EXTENSION_DEPTH_MAPPING_SENSOR] = std::make_shared<depth_mapping_sensor_snapshot>();
        }
    }


    bool ros_reader::is_depth_sensor(const std::string& sensor_name)
    {
        return (sensor_name.compare("Stereo Module") == 0 || sensor_name.compare("Coded-Light Depth Sensor") == 0);
    }

    bool ros_reader::is_stereo_depth_sensor( std::string sensor_name )
    {
        return sensor_name.compare( "Stereo Module" ) == 0;
    }

    bool ros_reader::is_color_sensor(const std::string& sensor_name)
    {
        return sensor_name.compare( "RGB Camera" ) == 0;
    }

    bool ros_reader::is_motion_module_sensor(const std::string& sensor_name)
    {
        return (sensor_name.compare("Motion Module") == 0);
    }

    bool ros_reader::is_fisheye_module_sensor(const std::string& sensor_name)
    {
        return (sensor_name.compare("Wide FOV Camera") == 0);
    }

    bool ros_reader::is_safety_module_sensor(const std::string& sensor_name)
    {
        return (sensor_name.compare("Safety Camera") == 0);
    }

    bool ros_reader::is_depth_mapping_sensor(const std::string& sensor_name)
    {
        return (sensor_name.compare("Depth Mapping Camera") == 0);
    }

    bool ros_reader::is_ds_PID(int pid)
    {
        using namespace ds;

        auto it5 = std::find_if(rs400_sku_pid.begin(), rs400_sku_pid.end(), [&](int d400_pid)
        {
            return pid == d400_pid;
        });

        if (it5 != rs400_sku_pid.end())
            return true;

        auto it6 = std::find_if(rs500_sku_pid.begin(), rs500_sku_pid.end(), [&](int d500_pid)
            {
                return pid == d500_pid;
            });

        return it6 != rs500_sku_pid.end();
    }

    std::shared_ptr<recommended_proccesing_blocks_snapshot> ros_reader::read_proccesing_blocks_for_version_under_4(std::string pid, std::string sensor_name, std::shared_ptr<options_interface> options)
    {
        std::stringstream ss;
        ss << pid;
        int int_pid;
        ss >> std::hex >> int_pid;

        if (is_ds_PID(int_pid))
        {
            if (is_depth_sensor(sensor_name))
            {
                return std::make_shared<recommended_proccesing_blocks_snapshot>(get_ds_depth_recommended_proccesing_blocks());
            }
            else if (is_color_sensor(sensor_name))
            {
                return std::make_shared<recommended_proccesing_blocks_snapshot>(get_color_recommended_proccesing_blocks());
            }
            else if (is_motion_module_sensor(sensor_name))
            {
                return std::make_shared<recommended_proccesing_blocks_snapshot>(processing_blocks{});
            }
            throw io_exception("Unrecognized sensor name" + sensor_name);
        }

        //Unrecognized sensor
        return std::make_shared<recommended_proccesing_blocks_snapshot>(processing_blocks{});
    }

    std::shared_ptr< recommended_proccesing_blocks_snapshot >
    ros_reader::read_proccesing_blocks( const rosbag::Bag & file,
                                        device_serializer::sensor_identifier sensor_id,
                                        const nanoseconds & timestamp,
                                        std::shared_ptr< options_interface > options,
                                        uint32_t file_version,
                                        std::string pid,
                                        std::string sensor_name )
    {
        processing_blocks blocks;
        std::shared_ptr<recommended_proccesing_blocks_snapshot> res;

        if (file_version < ROS_FILE_WITH_RECOMMENDED_PROCESSING_BLOCKS)
        {
            return read_proccesing_blocks_for_version_under_4(pid, sensor_name, options);
        }
        else
        {
            //Taking all messages from the beginning of the bag until the time point requested
            std::string proccesing_block_topic = ros_topic::post_processing_blocks_topic(sensor_id);
            rosbag::View option_view(file, rosbag::TopicQuery(proccesing_block_topic), to_rostime(get_static_file_info_timestamp()), to_rostime(timestamp));
            auto it = option_view.begin();

            auto depth_to_disparity = true;

            rosbag::View::iterator last_item;
            while (it != option_view.end())
            {
                last_item = it++;

                auto block = create_processing_block(*last_item, depth_to_disparity, options);
                if( block )
                    blocks.push_back(block);
            }

            res = std::make_shared<recommended_proccesing_blocks_snapshot>(blocks);
        }
        return res;
    }

    device_snapshot ros_reader::read_device_description(const nanoseconds& time, bool reset)
    {
        if (time == get_static_file_info_timestamp())
        {
            if (reset)
            {
                snapshot_collection device_extensions;

                auto info = read_info_snapshot(ros_topic::device_info_topic(get_device_index()));
                device_extensions[RS2_EXTENSION_INFO] = info;

                std::vector<sensor_snapshot> sensor_descriptions;
                auto sensor_indices = read_sensor_indices(get_device_index());
                m_extrinsics_map.clear();

                for (auto sensor_index : sensor_indices)
                {
                    snapshot_collection sensor_extensions;
                    auto streams_snapshots = read_stream_info(get_device_index(), sensor_index);
                    for (auto stream_profile : streams_snapshots)
                    {
                        auto stream_id = stream_identifier{ get_device_index(), sensor_index, stream_profile->get_stream_type(), static_cast<uint32_t>(stream_profile->get_stream_index()) };
                        uint32_t reference_id;
                        rs2_extrinsics stream_extrinsic;
                        if (try_read_stream_extrinsic(stream_id, reference_id, stream_extrinsic))
                        {
                            m_extrinsics_map[stream_id] = std::make_pair(reference_id, stream_extrinsic);
                        }
                    }

                    //Update infos
                    std::shared_ptr<info_container> sensor_info;
                    if (m_version == legacy_file_format::file_version())
                    {
                        sensor_info = read_legacy_info_snapshot(sensor_index);
                    }
                    else
                    {
                        sensor_info = read_info_snapshot(ros_topic::sensor_info_topic({ get_device_index(), sensor_index }));
                    }
                    sensor_extensions[RS2_EXTENSION_INFO] = sensor_info;
                    //Update options
                    update_sensor_options(m_file, sensor_index, time, m_version, sensor_extensions, m_version);

                    std::string pid = "";
                    std::string sensor_name = "";

                    if (info->supports_info(RS2_CAMERA_INFO_PRODUCT_ID))
                        pid = info->get_info(RS2_CAMERA_INFO_PRODUCT_ID);

                    if (sensor_info->supports_info(RS2_CAMERA_INFO_NAME))
                        sensor_name = sensor_info->get_info(RS2_CAMERA_INFO_NAME);

                    add_sensor_extension(sensor_extensions, sensor_name);
                    update_proccesing_blocks(m_file, sensor_index, time, m_version, sensor_extensions, m_version, pid, sensor_name);

                    sensor_descriptions.emplace_back(sensor_index, sensor_extensions, streams_snapshots);
                }

                m_initial_device_description = device_snapshot(device_extensions, sensor_descriptions, m_extrinsics_map);
            }
            return m_initial_device_description;
        }
        else
        {
            //update only:
            auto device_snapshot = m_initial_device_description;
            for (auto& sensor : device_snapshot.get_sensors_snapshots())
            {
                auto& sensor_extensions = sensor.get_sensor_extensions_snapshots();
                update_sensor_options(m_file, sensor.get_sensor_index(), time, m_version, sensor_extensions, m_version);
            }
            return device_snapshot;
        }
    }

    std::shared_ptr<info_container> ros_reader::read_legacy_info_snapshot(uint32_t sensor_index) const
    {
        std::map<rs2_camera_info, std::string> values;
        rosbag::View view( m_file, rosbag::TopicQuery( rsutils::string::from() << "/info/" << sensor_index ) );
        auto infos = std::make_shared<info_container>();
        //TODO: properly implement, currently assuming TM2 devices
        infos->register_info( RS2_CAMERA_INFO_NAME, rsutils::string::from() << "Sensor " << sensor_index );
        for (auto message_instance : view)
        {
            auto info_msg = instantiate_msg<realsense_legacy_msgs::vendor_data>(message_instance);
            try
            {
                rs2_camera_info info;
                if (legacy_file_format::info_from_string(info_msg->name, info))
                {
                    infos->register_info(info, info_msg->value);
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << e.what() << std::endl;
            }
        }

        return infos;
    }
    std::shared_ptr<info_container> ros_reader::read_info_snapshot(const std::string& topic) const
    {
        auto infos = std::make_shared<info_container>();
        if (m_version == legacy_file_format::file_version())
        {
            //TODO: properly implement, currently assuming TM2 devices and Movidius PID
            infos->register_info(RS2_CAMERA_INFO_NAME, "RealSense TM2");
            infos->register_info(RS2_CAMERA_INFO_PRODUCT_ID, "2150");
            infos->register_info(RS2_CAMERA_INFO_SERIAL_NUMBER, "N/A");
        }
        std::map<rs2_camera_info, std::string> values;
        rosbag::View view(m_file, rosbag::TopicQuery(topic));
        for (auto message_instance : view)
        {
            diagnostic_msgs::KeyValueConstPtr info_msg = instantiate_msg<diagnostic_msgs::KeyValue>(message_instance);
            try
            {
                rs2_camera_info info;
                convert(info_msg->key, info);
                infos->register_info(info, info_msg->value);
            }
            catch (const std::exception& e)
            {
                std::cerr << e.what() << std::endl;
            }
        }

        return infos;
    }

    std::set<uint32_t> ros_reader::read_sensor_indices(uint32_t device_index) const
    {
        std::set<uint32_t> sensor_indices;
        if (m_version == legacy_file_format::file_version())
        {
            rosbag::View device_info(m_file, rosbag::TopicQuery("/info/4294967295"));
            if (device_info.size() == 0)
            {
                throw io_exception("Missing sensor count message for legacy file");
            }
            for (auto info : device_info)
            {
                auto msg = instantiate_msg<realsense_legacy_msgs::vendor_data>(info);
                if (msg->name == "sensor_count")
                {
                    int sensor_count = std::stoi(msg->value);
                    while (--sensor_count >= 0)
                        sensor_indices.insert(sensor_count);
                }
            }
        }
        else
        {
            rosbag::View sensor_infos(m_file, SensorInfoQuery(device_index));
            for (auto sensor_info : sensor_infos)
            {
                auto msg = instantiate_msg<diagnostic_msgs::KeyValue>(sensor_info);
                sensor_indices.insert(static_cast<uint32_t>(ros_topic::get_sensor_index(sensor_info.getTopic())));
            }
        }
        return sensor_indices;
    }
    std::shared_ptr<pose_stream_profile> ros_reader::create_pose_profile(uint32_t stream_index, uint32_t fps)
    {
        rs2_format format = RS2_FORMAT_6DOF;
        auto profile = std::make_shared< pose_stream_profile >();
        profile->set_stream_index(stream_index);
        profile->set_stream_type(RS2_STREAM_POSE);
        profile->set_format(format);
        profile->set_framerate(fps);
        return profile;
    }

    std::shared_ptr<motion_stream_profile> ros_reader::create_motion_stream(rs2_stream stream_type, uint32_t stream_index, uint32_t fps, rs2_format format, rs2_motion_device_intrinsic intrinsics)
    {
        auto profile = std::make_shared< motion_stream_profile >();
        profile->set_stream_index(stream_index);
        profile->set_stream_type(stream_type);
        profile->set_format(format);
        profile->set_framerate(fps);
        profile->set_intrinsics([intrinsics]() {return intrinsics; });
        return profile;
    }

    std::shared_ptr<video_stream_profile> ros_reader::create_video_stream_profile(const platform::stream_profile& sp,
        const sensor_msgs::CameraInfo& ci,
        const stream_descriptor& sd)
    {
        auto profile = std::make_shared<video_stream_profile>();
        rs2_intrinsics intrinsics{};
        intrinsics.height = ci.height;
        intrinsics.width = ci.width;
        intrinsics.fx = ci.K[0];
        intrinsics.ppx = ci.K[2];
        intrinsics.fy = ci.K[4];
        intrinsics.ppy = ci.K[5];
        intrinsics.model = RS2_DISTORTION_NONE;
        for (std::underlying_type<rs2_distortion>::type i = 0; i < RS2_DISTORTION_COUNT; ++i)
        {
            if (strcmp(ci.distortion_model.c_str(), rs2_distortion_to_string(static_cast<rs2_distortion>(i))) == 0)
            {
                intrinsics.model = static_cast<rs2_distortion>(i);
                break;
            }
        }
        for (size_t i = 0; i < ci.D.size() && i < sizeof(intrinsics.coeffs)/sizeof(intrinsics.coeffs[0]); ++i)
        {
            intrinsics.coeffs[i] = ci.D[i];
        }
        profile->set_intrinsics([intrinsics]() {return intrinsics; });
        profile->set_stream_index(sd.index);
        profile->set_stream_type(sd.type);
        profile->set_dims(ci.width, ci.height);
        profile->set_format(static_cast<rs2_format>(sp.format));
        profile->set_framerate(sp.fps);
        return profile;
    }

    stream_profiles ros_reader::read_legacy_stream_info(uint32_t sensor_index) const
    {
        //legacy files have the form of "/(camera|imu)/<stream type><stream index>/(image_imu)_raw/<sensor_index>
        //6DoF streams have no streaming profiles in the file - handling them seperatly
        stream_profiles streams;
        rosbag::View stream_infos_view( m_file,
                                        RegexTopicQuery( rsutils::string::from()
                                                         << R"RRR(/camera/(rs_stream_info|rs_motion_stream_info)/)RRR"
                                                         << sensor_index ) );
        for (auto infos_msg : stream_infos_view)
        {
            if (infos_msg.isType<realsense_legacy_msgs::motion_stream_info>())
            {
                auto motion_stream_info_msg = instantiate_msg<realsense_legacy_msgs::motion_stream_info>(infos_msg);
                auto fps = motion_stream_info_msg->fps;

                std::string stream_name = motion_stream_info_msg->motion_type;
                stream_descriptor stream_id = legacy_file_format::parse_stream_type(stream_name);
                rs2_format format = RS2_FORMAT_MOTION_XYZ32F;
                rs2_motion_device_intrinsic intrinsics{};
                //TODO: intrinsics = motion_stream_info_msg->stream_intrinsics;
                auto profile = create_motion_stream(stream_id.type, stream_id.index, fps, format, intrinsics);
                streams.push_back(profile);
            }
            else if (infos_msg.isType<realsense_legacy_msgs::stream_info>())
            {
                auto stream_info_msg = instantiate_msg<realsense_legacy_msgs::stream_info>(infos_msg);
                auto fps = stream_info_msg->fps;
                rs2_format format;
                convert(stream_info_msg->encoding, format);
                std::string stream_name = stream_info_msg->stream_type;
                stream_descriptor stream_id = legacy_file_format::parse_stream_type(stream_name);
                auto profile = create_video_stream_profile(
                    platform::stream_profile{ stream_info_msg->camera_info.width, stream_info_msg->camera_info.height, fps, static_cast<uint32_t>(format) },
                    stream_info_msg->camera_info,
                    stream_id);
                streams.push_back(profile);
            }
            else
            {
                throw io_exception(
                    rsutils::string::from()
                    << "Invalid file format, expected "
                    << rs2rosinternal::message_traits::DataType< realsense_legacy_msgs::motion_stream_info >::value()
                    << " or " << rs2rosinternal::message_traits::DataType< realsense_legacy_msgs::stream_info >::value()
                    << " message but got: " << infos_msg.getDataType() << "(Topic: " << infos_msg.getTopic() << ")" );
            }
        }
        std::unique_ptr<rosbag::View> entire_bag = std::unique_ptr<rosbag::View>(new rosbag::View(m_file, rosbag::View::TrueQuery()));
        std::vector<uint32_t> indices;
        for (auto&& topic : get_topics(entire_bag))
        {
            std::regex r(R"RRR(/camera/rs_6DoF(\d+)/\d+)RRR");
            std::smatch sm;
            if (std::regex_search(topic, sm, r))
            {
                for (int i = 1; i < sm.size(); i++)
                {
                    indices.push_back(std::stoul(sm[i].str()));
                }
            }
        }
        for (auto&& index : indices)
        {
            auto profile = create_pose_profile(index, 0); //TODO: Where to read the fps from?
            streams.push_back(profile);
        }
        return streams;
    }

    stream_profiles ros_reader::read_stream_info(uint32_t device_index, uint32_t sensor_index) const
    {
        if (m_version == legacy_file_format::file_version())
        {
            return read_legacy_stream_info(sensor_index);
        }
        stream_profiles streams;
        //The below regex matches both stream info messages and also video \ imu stream info (both have the same prefix)
        auto regex_const_char = R"RRR(/([a-zA-Z0-9_ ])+_(\d)+/info)RRR";
        rosbag::View stream_infos_view(m_file, RegexTopicQuery("/device_" + std::to_string(device_index) + "/sensor_" + std::to_string(sensor_index) + regex_const_char));
        for (auto infos_view : stream_infos_view)
        {
            if (infos_view.isType<realsense_msgs::StreamInfo>() == false)
            {
                continue;
            }

            stream_identifier stream_id = ros_topic::get_stream_identifier(infos_view.getTopic());
            auto stream_info_msg = instantiate_msg<realsense_msgs::StreamInfo>(infos_view);
            //auto is_recommended = stream_info_msg->is_recommended;
            auto fps = stream_info_msg->fps;
            rs2_format format;
            convert(stream_info_msg->encoding, format);

            auto video_stream_topic = ros_topic::video_stream_info_topic(stream_id);
            rosbag::View video_stream_infos_view(m_file, rosbag::TopicQuery(video_stream_topic));
            if (video_stream_infos_view.size() > 0)
            {
                assert(video_stream_infos_view.size() == 1);
                auto video_stream_msg_ptr = *video_stream_infos_view.begin();
                auto video_stream_msg = instantiate_msg<sensor_msgs::CameraInfo>(video_stream_msg_ptr);
                auto profile = create_video_stream_profile(
                    platform::stream_profile{ video_stream_msg->width ,video_stream_msg->height, fps, static_cast<uint32_t>(format) }
                    , *video_stream_msg,
                    { stream_id.stream_type, static_cast<int>(stream_id.stream_index) });
                streams.push_back(profile);
            }

            auto imu_stream_topic = ros_topic::imu_intrinsic_topic(stream_id);
            rosbag::View imu_intrinsic_view(m_file, rosbag::TopicQuery(imu_stream_topic));
            if (imu_intrinsic_view.size() > 0)
            {
                assert(imu_intrinsic_view.size() == 1);
                auto motion_intrinsics_msg = instantiate_msg<realsense_msgs::ImuIntrinsic>(*imu_intrinsic_view.begin());
                rs2_motion_device_intrinsic intrinsics{};
                std::copy(std::begin(motion_intrinsics_msg->bias_variances), std::end(motion_intrinsics_msg->bias_variances), std::begin(intrinsics.bias_variances));
                std::copy(std::begin(motion_intrinsics_msg->noise_variances), std::end(motion_intrinsics_msg->noise_variances), std::begin(intrinsics.noise_variances));
                std::copy(std::begin(motion_intrinsics_msg->data), std::end(motion_intrinsics_msg->data), &intrinsics.data[0][0]);
                auto profile = create_motion_stream(stream_id.stream_type, stream_id.stream_index, fps, format, intrinsics);
                streams.push_back(profile);
            }

            if (stream_id.stream_type == RS2_STREAM_POSE)
            {
                auto profile = create_pose_profile(stream_id.stream_index, fps);
                streams.push_back(profile);
            }

            if (video_stream_infos_view.size() == 0 && imu_intrinsic_view.size() == 0 && stream_id.stream_type != RS2_STREAM_POSE)
            {
                throw io_exception(
                    rsutils::string::from()
                    << "Every StreamInfo is expected to have a complementary video/imu message, but none was found" );
            }
        }
        return streams;
    }

    std::string ros_reader::read_option_description(const rosbag::Bag& file, const std::string& topic)
    {
        rosbag::View option_description_view(file, rosbag::TopicQuery(topic));
        if (option_description_view.size() == 0)
        {
            LOG_ERROR("File does not contain topics for: " << topic);
            return "N/A";
        }
        assert(option_description_view.size() == 1); //There should be only 1 message for each option
        auto description_message_instance = *option_description_view.begin();
        auto option_desc_msg = instantiate_msg<std_msgs::String>(description_message_instance);
        return option_desc_msg->data;
    }

    /*Until Version 2 (including)*/
    std::pair<rs2_option, std::shared_ptr<librealsense::option>> ros_reader::create_property(const rosbag::MessageInstance& property_message_instance)
    {
        auto property_msg = instantiate_msg<diagnostic_msgs::KeyValue>(property_message_instance);
        rs2_option id;
        convert(property_msg->key, id);
        float value = std::stof(property_msg->value);
        std::string description = rsutils::string::from() << "Read only option of " << id;
        return std::make_pair(id, std::make_shared<const_value_option>(description, value));
    }

    /*Starting version 3*/
    std::pair<rs2_option, std::shared_ptr<librealsense::option>> ros_reader::create_option(const rosbag::Bag& file, const rosbag::MessageInstance& value_message_instance)
    {
        auto option_value_msg = instantiate_msg<std_msgs::Float32>(value_message_instance);
        auto value_topic = value_message_instance.getTopic();
        std::string option_name = ros_topic::get_option_name(value_topic);
        device_serializer::sensor_identifier sensor_id = ros_topic::get_sensor_identifier(value_message_instance.getTopic());
        rs2_option id;
        std::replace(option_name.begin(), option_name.end(), '_', ' ');
        convert(option_name, id);
        float value = option_value_msg->data;
        auto description_topic = value_topic.replace(value_topic.find_last_of("value") - sizeof("value") + 2, sizeof("value"), "description");
        std::string description = read_option_description(file, description_topic);
        return std::make_pair(id, std::make_shared<const_value_option>(description, value));
    }

    std::shared_ptr< librealsense::processing_block_interface >
    ros_reader::create_processing_block( const rosbag::MessageInstance & value_message_instance,
                                         bool & depth_to_disparity,
                                         std::shared_ptr< options_interface > sensor_options )
    {
        auto processing_block_msg = instantiate_msg<std_msgs::String>(value_message_instance);
        std::string name = processing_block_msg->data;
        if( name == "Disparity Filter" )
        {
            // What was recorded was the extension type (without its settings!), but we need to create different
            // variants. "Disparity Filter" gets recorded twice! This workaround ensures it's instantiated in its
            // non-default flavor the second time:
            if( depth_to_disparity )
                depth_to_disparity = false;
            else
                name = "Disparity to Depth";
        }
        try
        {
            auto block = m_context->create_pp_block( name, {} );
            if( ! block )
                LOG_DEBUG( "unknown processing block '" << name << "'; ignored" );
            return block;
        }
        catch( std::exception const & e )
        {
            LOG_DEBUG( "failed to create processing block '" << name << "': " << e.what() );
            return {};
        }
    }

    notification ros_reader::create_notification(const rosbag::Bag& file, const rosbag::MessageInstance& message_instance)
    {
        auto notification_msg = instantiate_msg<realsense_msgs::Notification>(message_instance);
        rs2_notification_category category;
        rs2_log_severity severity;
        convert(notification_msg->category, category);
        convert(notification_msg->severity, severity);
        int type = 0; //TODO: what is this for?
        notification n(category, type, severity, notification_msg->description);
        n.timestamp = to_nanoseconds(notification_msg->timestamp).count();
        n.serialized_data = notification_msg->serialized_data;
        return n;
    }

    std::shared_ptr<options_container> ros_reader::read_sensor_options(const rosbag::Bag& file, device_serializer::sensor_identifier sensor_id, const nanoseconds& timestamp, uint32_t file_version)
    {
        auto options = std::make_shared<options_container>();
        if (file_version == 2)
        {
            rosbag::View sensor_options_view(file, rosbag::TopicQuery(ros_topic::property_topic(sensor_id)));
            for (auto message_instance : sensor_options_view)
            {
                auto id_option = create_property(message_instance);
                options->register_option(id_option.first, id_option.second);
            }
        }
        else
        {
            //Taking all messages from the beginning of the bag until the time point requested
            for (int i = 0; i < static_cast<int>(RS2_OPTION_COUNT); i++)
            {
                rs2_option id = static_cast<rs2_option>(i);
                auto value_topic = ros_topic::option_value_topic(sensor_id, id);
                std::string option_name = ros_topic::get_option_name(value_topic);
                auto rs2_option_name = rs2_option_to_string(id); //option name with space seperator
                auto alternate_value_topic = value_topic;
                alternate_value_topic.replace(value_topic.find(option_name), option_name.length(), rs2_option_name);

                std::vector<std::string> option_topics{ value_topic, alternate_value_topic };
                rosbag::View option_view(file, rosbag::TopicQuery(option_topics), to_rostime(get_static_file_info_timestamp()), to_rostime(timestamp));
                auto it = option_view.begin();
                if (it == option_view.end())
                {
                    continue;
                }
                rosbag::View::iterator last_item;
                while (it != option_view.end())
                {
                    last_item = it++;
                }
                auto option = create_option(file, *last_item);
                assert(id == option.first);
                options->register_option(option.first, option.second);
            }
        }
        return options;
    }

    std::vector<std::string> ros_reader::get_topics(std::unique_ptr<rosbag::View>& view)
    {
        std::vector<std::string> topics;
        if (view != nullptr)
        {
            auto connections = view->getConnections();
            std::transform(connections.begin(), connections.end(), std::back_inserter(topics), [](const rosbag::ConnectionInfo* connection) { return connection->topic; });
        }
        return topics;
    }
}
