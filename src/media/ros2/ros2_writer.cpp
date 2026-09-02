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
#include "ros2_image_codec.h"
#include <sensor_msgs/msg/CompressedImage.h>
#include <tf2_msgs/msg/TFMessage.h>
#include <zstd.h>
#include "media/ros_factory.h"
#include "core/motion-frame.h"
#include <sstream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <cstring>
#include <src/core/sensor-interface.h>
#include <src/core/device-interface.h>
#include <src/core/depth-frame.h>
#include <src/points.h>
#include <src/labeled-points.h>
#include <src/object-detection-frame.h>

#include <fstream>

namespace librealsense
{
    using namespace device_serializer;

    static std::string strip_db3_extension(const std::string& file)
    {
        if (!is_db3_file(file))
            throw std::runtime_error("Output file must have .db3 extension: '" + file + "'");
        return file.substr(0, file.size() - 4);
    }

    ros2_writer::ros2_writer(const std::string& file, bool compress_while_record) : m_file_path(file)
    {
        LOG_INFO("Compression while record is set to " << (compress_while_record ? "ON" : "OFF"));
        _storage = std::make_shared< rosbag2_storage_plugins::SqliteStorage >();

        // rosbag2 sqlite plugin appends .db3 internally, so pass the stem
        auto base_path = strip_db3_extension(file);

        // Remove existing file — sqlite plugin doesn't overwrite
        std::ifstream f(file);
        if (f.good())
        {
            f.close();
            if (std::remove(file.c_str()) != 0)
            {
                throw std::runtime_error(rsutils::string::from() << "Failed to remove existing file '" << file << "'");
            }
        }

        _storage->open(base_path, rosbag2_storage::storage_interfaces::IOFlag::READ_WRITE);
        if (!_storage)
            throw std::runtime_error(rsutils::string::from() << "Failed to open rosbag2 storage for uri '" << file
                << "' using storage id 'sqlite3'");

        _compress = compress_while_record;

        write_file_version();
    }

    void ros2_writer::ensure_topic(const std::string& name, const std::string& type, const std::string& offered_qos)
    {
        if (_topics.find(name) != _topics.end())
            return;
        rosbag2_storage::TopicMetadata md;
        md.name = name;
        md.type = type;
        md.serialization_format = "cdr";
        md.offered_qos_profiles = offered_qos;
        _storage->create_topic(md);
        _topics.emplace(name, md);
    }

    void ros2_writer::compress_zstd(const uint8_t* data, size_t size, std::vector<uint8_t>& out)
    {
        out.resize(ZSTD_compressBound(size));

        // Level 1 is the fastest zstd level with good-enough ratio; comparable in speed to LZ4 used by rosbag1
        auto compressed_size = ZSTD_compress(out.data(), out.size(), data, size, 1);
        if (ZSTD_isError(compressed_size))
            throw std::runtime_error(rsutils::string::from() << "Zstd compression failed: " << ZSTD_getErrorName(compressed_size));
        out.resize(compressed_size);
    }

    // Decoded PNGs surface as BGR-ordered mats on the ROS side (cv::imdecode); the format
    // string's second encoding advertises that, per the compressed_image_transport convention.
    static const char* decoded_png_encoding(const ros2_image_codec::png_layout& layout)
    {
        if (layout.num_channels == 3) return "bgr8";
        if (layout.num_channels == 4) return "bgra8";
        return layout.bytes_per_channel == 2 ? "mono16" : "mono8";
    }

    void ros2_writer::write_compressed_video_frame(const stream_identifier& stream_id,
                                                   const nanoseconds& timestamp,
                                                   frame_interface* frame)
    {
        const uint8_t* pixels;
        uint32_t width, height, stride;
        rs2_format format;
        if (auto vid_frame = As<video_frame>(frame))
        {
            pixels = vid_frame->get_frame_data();
            width = vid_frame->get_width();
            height = vid_frame->get_height();
            stride = vid_frame->get_stride();
            format = vid_frame->get_stream()->get_format();
        }
        else if (auto lp = As<labeled_points>(frame))
        {
            // labeled points have no 2D geometry — encode as a single row of bytes
            auto data_size = static_cast<uint32_t>(lp->get_vertex_count() * lp->get_bpp() / 8);
            pixels = lp->get_frame_data();
            width = stride = data_size;
            height = 1;
            format = lp->get_stream()->get_format();
        }
        else
        {
            throw invalid_value_exception("write_compressed_video_frame: unsupported frame type");
        }

        auto stream_name = ros2_topic::stream_name(stream_id.stream_type, stream_id.stream_index);
        auto ns_count = timestamp.count();
        sensor_msgs::msg::CompressedImage msg;
        msg.header().stamp().sec(static_cast<int32_t>(ns_count / 1000000000LL));
        msg.header().stamp().nanosec(static_cast<uint32_t>(ns_count % 1000000000LL));
        msg.header().frame_id(stream_name);

        ros2_image_codec::png_layout layout{};
        bool png_ok = ros2_image_codec::png_layout_for_format(format, layout);
        _frame_buf.clear();
        if (png_ok)
        {
            if (layout.is_depth)
            {
                // compressed_depth_image_transport expects a ConfigHeader before the PNG
                ros2_image_codec::config_header hdr;
                _frame_buf.resize(sizeof(hdr));
                std::memcpy(_frame_buf.data(), &hdr, sizeof(hdr));
                msg.format(rsutils::string::from() << layout.ros_encoding << "; compressedDepth png");
            }
            else
            {
                msg.format(rsutils::string::from() << layout.ros_encoding << "; png compressed "
                                                   << decoded_png_encoding(layout));
            }
            ros2_image_codec::encode_png(layout, pixels, width, height, stride, _frame_buf);
        }
        else
        {
            // Formats PNG can't represent losslessly (YUYV-class packed formats):
            // zstd the raw pixels; the format string carries the layout for external consumers.
            compress_zstd(pixels, size_t(stride) * height, _frame_buf);
            msg.format(rsutils::string::from() << rs2_format_to_string(format) << "; zstd; "
                                               << width << "x" << height << " step=" << stride);
        }
        msg.data(std::move(_frame_buf));

        write_message(ros2_topic::compressed_frame_data_topic(stream_id, png_ok && layout.is_depth),
                      "sensor_msgs/msg/CompressedImage", timestamp, msg);
        _frame_buf = std::move(msg.data()); // reclaim the buffer's capacity for the next frame
        write_additional_frame_messages(stream_id, timestamp, frame);
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

            // Bag-to-db3 conversion only: the ROS1 reader provides stream profiles via
            // get_stream_profiles() rather than as VIDEO_PROFILE/MOTION_PROFILE extensions
            sensor_identifier sensor_id{ get_device_index(), sensors_snapshot.get_sensor_index() };
            for (auto&& profile : sensors_snapshot.get_stream_profiles())
            {
                auto vid = std::dynamic_pointer_cast<video_stream_profile_interface>(profile);
                auto mot = std::dynamic_pointer_cast<motion_stream_profile_interface>(profile);
                if (vid)
                    write_streaming_info(get_static_file_info_timestamp(), sensor_id, vid);
                else if (mot)
                    write_streaming_info(get_static_file_info_timestamp(), sensor_id, mot);
                else
                    write_stream_info(get_static_file_info_timestamp(), sensor_id, profile);
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
            if (_compress)
            {
                write_compressed_video_frame(stream_id, timestamp, frame.frame);
                return;
            }
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
            if (_compress)
            {
                write_compressed_video_frame(stream_id, timestamp, frame.frame);
                return;
            }
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
        else if (Is<object_detection_frame>(frame.frame))
        {
            // Re-encode the binary object-detection payload back to the same JSON format the DDS server sends,
            // so that playback can reconstruct the frame using the same parse path.
            auto od = As<object_detection_frame>(frame.frame);
            auto const payload = od->get_payload_header();
            auto const n = od->get_detection_count();

            std::ostringstream json;
            json << "{"
                 << "\"frame_id\":" << payload.frame_id << ","
                 << "\"number_of_detections\":" << n << ","
                 << "\"detections\":[";
            for (uint16_t i = 0; i < n; ++i)
            {
                auto const e = od->get_detection(i);
                if (i) json << ",";
                json << "{"
                     << "\"class_id\":"    << static_cast<int>(e.detection_type) << ","
                     << "\"confidence\":"  << static_cast<int>(e.confidence) << ","
                     << "\"x1\":"          << e.top_left_x << ","
                     << "\"y1\":"          << e.top_left_y << ","
                     << "\"x2\":"          << e.bottom_right_x << ","
                     << "\"y2\":"          << e.bottom_right_y << ","
                     << "\"distance\":"    << e.distance;
                if( e.com_valid )
                    json << ",\"world_pos\":{"
                         << "\"x\":" << e.world_position.x << ","
                         << "\"y\":" << e.world_position.y << ","
                         << "\"z\":" << e.world_position.z << "},"
                         << "\"image_pos\":{"
                         << "\"x\":" << e.image_x << ","
                         << "\"y\":" << e.image_y << "}";
                json << "}";
            }
            json << "],"
                 << "\"source_frame_id\":" << payload.source_frame_id << ","
                 << "\"version\":1,"
                 << "\"od_version\":"      << od->get_version() << ","
                 << "\"timestamp_us\":"    << (payload.timestamp_ms * MILLISEC_TO_MICROSEC)
                 << "}";

            write_string(ros2_topic::frame_data_topic(stream_id), timestamp, json.str());
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
        std::string depth_units;
        if (auto df = dynamic_cast<librealsense::depth_frame*>(frame))
        {
            auto units = df->get_units();
            if (units > 0)
                depth_units = std::to_string(units);
        }

        std::string metadata_payload = rsutils::string::from() << FRAME_NUMBER_MD_STR << "=" << frame_number << ";" 
                                                                << TIMESTAMP_DOMAIN_MD_STR << "=" << timestamp_domain << ";" 
                                                                << SYSTEM_TIME_MD_STR << "=" << system_time << ";"
                                                                << TIMESTAMP_MD_STR << "="  << ts << ";";
        if (!depth_units.empty())
            metadata_payload += rsutils::string::from() << DEPTH_UNITS_MD_STR << "=" << depth_units << ";";

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

    // rs2_extrinsics rotation is a column-major 3x3 matrix; standard trace-based conversion
    static geometry_msgs::msg::Quaternion rotation_matrix_to_quaternion(const float r[9])
    {
        auto R = [&](int row, int col) { return double(r[col * 3 + row]); };
        double w, x, y, z;
        double trace = R(0, 0) + R(1, 1) + R(2, 2);
        if (trace > 0.0)
        {
            double s = std::sqrt(trace + 1.0) * 2.0;
            w = 0.25 * s;
            x = (R(2, 1) - R(1, 2)) / s;
            y = (R(0, 2) - R(2, 0)) / s;
            z = (R(1, 0) - R(0, 1)) / s;
        }
        else if (R(0, 0) > R(1, 1) && R(0, 0) > R(2, 2))
        {
            double s = std::sqrt(1.0 + R(0, 0) - R(1, 1) - R(2, 2)) * 2.0;
            w = (R(2, 1) - R(1, 2)) / s;
            x = 0.25 * s;
            y = (R(0, 1) + R(1, 0)) / s;
            z = (R(0, 2) + R(2, 0)) / s;
        }
        else if (R(1, 1) > R(2, 2))
        {
            double s = std::sqrt(1.0 + R(1, 1) - R(0, 0) - R(2, 2)) * 2.0;
            w = (R(0, 2) - R(2, 0)) / s;
            x = (R(0, 1) + R(1, 0)) / s;
            y = 0.25 * s;
            z = (R(1, 2) + R(2, 1)) / s;
        }
        else
        {
            double s = std::sqrt(1.0 + R(2, 2) - R(0, 0) - R(1, 1)) * 2.0;
            w = (R(1, 0) - R(0, 1)) / s;
            x = (R(0, 2) + R(2, 0)) / s;
            y = (R(1, 2) + R(2, 1)) / s;
            z = 0.25 * s;
        }
        geometry_msgs::msg::Quaternion q;
        q.x(x); q.y(y); q.z(z); q.w(w);
        return q;
    }

    // Matches rosbag2's Rosbag2QoS YAML (rosbag2_transport/src/rosbag2_transport/qos.cpp,
    // enums are rmw integers): reliable + transient_local, so /tf_static replays latched
    static const char* TF_STATIC_QOS_YAML =
        "- history: 1\n"
        "  depth: 1\n"
        "  reliability: 1\n"
        "  durability: 1\n"
        "  deadline:\n"
        "    sec: 0\n"
        "    nsec: 0\n"
        "  lifespan:\n"
        "    sec: 0\n"
        "    nsec: 0\n"
        "  liveliness: 0\n"
        "  liveliness_lease_duration:\n"
        "    sec: 0\n"
        "    nsec: 0\n"
        "  avoid_ros_namespace_conventions: false";

    void ros2_writer::write_extrinsics(const stream_identifier& stream_id, uint32_t reference_id, const rs2_extrinsics& ext)
    {
        if (m_extrinsics_msgs.find(stream_id) != m_extrinsics_msgs.end())
        {
            return; //already wrote it
        }

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

        // Also publish as a standard /tf_static transform so ROS2 tools resolve the frames
        // natively. The stored extrinsics map reference->stream (device::get_extrinsics fetches
        // from the pin stream), while TF parent->child needs the child pose in the parent frame
        // (stream->reference) — so invert: R' = R^T, t' = -R^T * t.
        float inv_rotation[9];
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                inv_rotation[col * 3 + row] = ext.rotation[row * 3 + col];
        float inv_translation[3];
        for (int row = 0; row < 3; ++row)
            inv_translation[row] = -(inv_rotation[0 * 3 + row] * ext.translation[0]
                                   + inv_rotation[1 * 3 + row] * ext.translation[1]
                                   + inv_rotation[2 * 3 + row] * ext.translation[2]);

        geometry_msgs::msg::TransformStamped ts;
        ts.header().frame_id("ref_" + std::to_string(reference_id));
        ts.child_frame_id(ros2_topic::stream_name(stream_id.stream_type, stream_id.stream_index));
        ts.transform().translation().x(inv_translation[0]);
        ts.transform().translation().y(inv_translation[1]);
        ts.transform().translation().z(inv_translation[2]);
        ts.transform().rotation(rotation_matrix_to_quaternion(inv_rotation));

        // Each message carries the cumulative transform set: the latch (transient_local,
        // depth 1) delivers only the LAST message to late joiners like rviz, so every
        // message must be self-sufficient — mirroring tf2's StaticTransformBroadcaster.
        m_tf_transforms.push_back(std::move(ts));
        tf2_msgs::msg::TFMessage tf_msg;
        tf_msg.transforms(m_tf_transforms);
        write_message("/tf_static", "tf2_msgs/msg/TFMessage", get_static_file_info_timestamp(), tf_msg, TF_STATIC_QOS_YAML);

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


    void ros2_writer::write_camera_info(const stream_identifier& stream_id, const nanoseconds& timestamp, frame_interface* frame)
    {
        auto it = m_camera_info_msgs.find(stream_id);
        if (it == m_camera_info_msgs.end())
        {
            auto profile = std::dynamic_pointer_cast<video_stream_profile_interface>(frame->get_stream());
            if (!profile)
                return;
            auto intr = profile->get_intrinsics(); // may throw — caller logs and skips

            sensor_msgs::msg::CameraInfo ci;
            ci.header().frame_id(ros2_topic::stream_name(stream_id.stream_type, stream_id.stream_index));
            ci.width(profile->get_width());
            ci.height(profile->get_height());
            ci.distortion_model(intr.model == RS2_DISTORTION_KANNALA_BRANDT4 ? "equidistant" : "plumb_bob");
            size_t num_coeffs = intr.model == RS2_DISTORTION_KANNALA_BRANDT4 ? 4 : 5;
            std::vector<double> d(num_coeffs, 0.0);
            for (size_t i = 0; i < num_coeffs; ++i)
                d[i] = intr.coeffs[i];
            ci.d(std::move(d));
            ci.k({ intr.fx, 0.0, intr.ppx, 0.0, intr.fy, intr.ppy, 0.0, 0.0, 1.0 });
            ci.r({ 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 });
            ci.p({ intr.fx, 0.0, intr.ppx, 0.0, 0.0, intr.fy, intr.ppy, 0.0, 0.0, 0.0, 1.0, 0.0 });
            it = m_camera_info_msgs.emplace(stream_id, std::move(ci)).first;
        }

        auto ns_count = timestamp.count();
        it->second.header().stamp().sec(static_cast<int32_t>(ns_count / 1000000000LL));
        it->second.header().stamp().nanosec(static_cast<uint32_t>(ns_count % 1000000000LL));
        write_message(ros2_topic::frame_camera_info_topic(stream_id), "sensor_msgs/msg/CameraInfo", timestamp, it->second);
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
            write_camera_info(stream_id, timestamp, frame);
        }
        catch (std::exception const& e)
        {
            LOG_DEBUG("Failed to write camera info for " << stream_id.stream_type << ". Exception: " << e.what());
        }

        // Perception streams don't participate in the extrinsics map
        if (stream_id.stream_type != RS2_STREAM_OBJECT_DETECTION)
        {
            try
            {
                auto sensor = frame->get_sensor();
                if (sensor)
                {
                    auto& dev = sensor->get_device();
                    uint32_t reference_id = 0;
                    rs2_extrinsics ext;
                    std::tie(reference_id, ext) = dev.get_extrinsics(*frame->get_stream());
                    write_extrinsics(stream_id, reference_id, ext);
                }
            }
            catch (std::exception const& e)
            {
                LOG_WARNING("Failed to write stream extrinsics for " << stream_id.stream_type << ". Exception: " << e.what());
            }
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
        // full round-trip precision; the default ostream precision truncates intrinsics
        auto full_precision = []( float v ) {
            std::ostringstream o;
            o << std::setprecision( std::numeric_limits<float>::max_digits10 ) << v;
            return o.str();
        };
        std::string payload = rsutils::string::from()
            << "width=" << profile->get_width() << ";"
            << "height=" << profile->get_height() << ";"
            << "fx=" << full_precision( intrinsics.fx ) << ";"
            << "ppx=" << full_precision( intrinsics.ppx ) << ";"
            << "fy=" << full_precision( intrinsics.fy ) << ";"
            << "ppy=" << full_precision( intrinsics.ppy ) << ";"
            << "model=" << librealsense::get_string(intrinsics.model) << ";"
            << "coeffs=";

        auto num_coeffs = sizeof(intrinsics.coeffs) / sizeof(intrinsics.coeffs[0]);
        for (size_t i = 0; i < num_coeffs; ++i)
        {
            payload += full_precision(intrinsics.coeffs[i]);
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
        case RS2_EXTENSION_PERCEPTION_PROFILE:
        {
            auto profile = SnapshotAs<RS2_EXTENSION_PERCEPTION_PROFILE>(snapshot);
            write_stream_info(timestamp, { device_id, sensor_id }, profile);
            break;
        }
        case RS2_EXTENSION_RECOMMENDED_FILTERS:
        {
            auto filters = SnapshotAs<RS2_EXTENSION_RECOMMENDED_FILTERS>(snapshot);
            write_sensor_processing_blocks({ device_id, sensor_id }, timestamp, filters);
            break;
        }
        default:
            // Sensor-type extensions (DEPTH_SENSOR, COLOR_SENSOR, etc.) are not serialized —
            // they are reconstructed by the reader from sensor info. Skip them silently.
            LOG_DEBUG("Skipping extension snapshot \"" << librealsense::get_string(type) << "\" (reconstructed by reader)");
            break;
        }

    }

    void ros2_writer::write_vendor_info(const std::string& topic, nanoseconds timestamp, std::shared_ptr< info_interface > info_snapshot)
    {
        // Pack all info key=value pairs into a single semicolon-delimited message
        // so third-party tools (e.g. Foxglove) can display all info in one entry
        std::ostringstream oss;
        bool first = true;
        for (uint32_t i = 0; i < static_cast<uint32_t>(RS2_CAMERA_INFO_COUNT); i++)
        {
            auto camera_info = static_cast<rs2_camera_info>(i);
            if (info_snapshot->supports_info(camera_info))
            {
                if (!first) oss << ";";
                oss << rs2_camera_info_to_string(camera_info) << "=" << info_snapshot->get_info(camera_info);
                first = false;
            }
        }
        write_string(topic, timestamp, oss.str());
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
        // Pack all processing block names into a single semicolon-delimited message
        std::ostringstream oss;
        bool first = true;
        for (auto block : proccesing_blocks->get_recommended_processing_blocks())
        {
            std::string name = get_processing_block_extension_name(block);
            if (name.empty())
            {
                LOG_WARNING("Failed to get recommended processing block name for sensor " << sensor_id.sensor_index);
                continue;
            }
            if (!first) oss << ";";
            oss << name;
            first = false;
        }
        if (!first)
        {
            try
            {
                write_string(ros2_topic::post_processing_blocks_topic(sensor_id), timestamp, oss.str());
            }
            catch (std::exception& e)
            {
                LOG_WARNING("Failed to write processing blocks for sensor " << sensor_id.sensor_index
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
