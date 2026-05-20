// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "ros2_native_reader.h"

#include <src/sensor.h>            // pulls in frame_source, info_container, options_container
#include <src/depth-sensor.h>
#include <src/core/video-frame.h>
#include <src/core/motion-frame.h>
#include <src/points.h>
#include <src/labeled-points.h>
#include <src/context.h>
#include "image.h"

#include <sensor_msgs/msg/Image.h>
#include <sensor_msgs/msg/Imu.h>
#include <sensor_msgs/msg/CameraInfo.h>

#include <rsutils/string/string-utilities.h>


namespace librealsense
{
    using namespace device_serializer;

    namespace {
        constexpr const char* MSG_TYPE_IMAGE       = "sensor_msgs/msg/Image";
        constexpr const char* MSG_TYPE_CAMERA_INFO = "sensor_msgs/msg/CameraInfo";
        constexpr const char* MSG_TYPE_IMU         = "sensor_msgs/msg/Imu";
        constexpr float       DEFAULT_DEPTH_UNITS  = 0.001f;  // 1mm, matches typical RealSense

        class native_depth_sensor_snapshot
            : public virtual depth_sensor
            , public extension_snapshot
        {
        public:
            explicit native_depth_sensor_snapshot(float units) : m_units(units) {}
            float get_depth_scale() const override { return m_units; }
            void update(std::shared_ptr<extension_snapshot>) override {}
        private:
            float m_units;
        };
    }

    ros2_native_reader::ros2_native_reader(const std::string& file, const std::shared_ptr<context> ctx)
        : ros2_reader_base(file, ctx)
    {
    }

    std::vector<std::string> ros2_native_reader::get_stream_topics() const
    {
        std::vector<std::string> topics;
        topics.reserve(_topic_to_stream_id.size());
        for (const auto& kv : _topic_to_stream_id) topics.push_back(kv.first);
        return topics;
    }

    std::vector<std::string> ros2_native_reader::get_option_topics() const { return {}; }
    std::vector<std::string> ros2_native_reader::get_notification_topics() const { return {}; }

    void ros2_native_reader::reset()
    {
        // Synthesized frame numbers must restart on replay/seek (native bags don't carry the original frame_id).
        _native_frame_counters.clear();
        ros2_reader_base::reset();
    }

    void ros2_native_reader::setup_frame(frame_interface* frame_ptr, const stream_identifier& sid) const
    {
        auto it = _profile_by_stream.find(sid);
        if (it == _profile_by_stream.end())
            throw std::runtime_error("Failed to setup frame: stream profile not found");

        frame_ptr->set_stream(it->second);

        auto vsp = std::dynamic_pointer_cast<video_stream_profile>(it->second);
        if (!vsp) return;

        auto vf = dynamic_cast<video_frame*>(frame_ptr);
        if (!vf)
        {
            if (dynamic_cast<labeled_points*>(frame_ptr)) return;
            throw std::runtime_error("Profile is video stream but frame is not video frame");
        }

        int width  = vsp->get_width();
        int height = vsp->get_height();
        int bpp    = get_image_bpp(vsp->get_format());
        vf->assign(width, height, width * bpp / 8, bpp);
    }

    device_snapshot ros2_native_reader::read_device_description(const nanoseconds&, bool)
    {
        if (_initialized) return m_initial_device_description;

        _topics_cache = _storage->get_all_topics_and_types();

        m_initial_device_description = read_native_device_description();
        _initialized = true;

        prepare_for_streaming();

        return m_initial_device_description;
    }

    std::shared_ptr<serialized_data> ros2_native_reader::read_next_data()
    {
        while (has_next_cached())
        {
            auto msg = read_next_cached();
            if (!msg)
            {
                LOG_ERROR("read_next_data: invalid message");
                continue;
            }
            auto it = _topic_to_stream_id.find(msg->topic_name);
            if (it == _topic_to_stream_id.end()) continue;
            if (!_enabled_streams.empty() && !_enabled_streams.count(it->second)) continue;
            return create_frame(msg);
        }
        return std::make_shared<serialized_end_of_file>();
    }

    rs2_stream ros2_native_reader::native_stream_type_from_topic(const std::string& topic)
    {
        // Substring match covers both `/camera/color/...` and `/realsense/<dev>_Color`.
        std::string lower = rsutils::string::to_lower(topic);
        auto has = [&](const char* needle){ return lower.find(needle) != std::string::npos; };
        if (has("color") || has("rgb"))                      return RS2_STREAM_COLOR;
        if (has("depth"))                                    return RS2_STREAM_DEPTH;
        if (has("infra"))                                    return RS2_STREAM_INFRARED;
        // accel/gyro must come before imu/motion so split-IMU bags don't fall through to MOTION.
        if (has("accel"))                                    return RS2_STREAM_ACCEL;
        if (has("gyro"))                                     return RS2_STREAM_GYRO;
        if (has("imu") || has("motion"))                     return RS2_STREAM_MOTION;
        return RS2_STREAM_ANY;
    }

    rs2_format ros2_native_reader::native_format_from_image_encoding(const std::string& encoding, rs2_stream stream_type)
    {
        if (encoding == "rgb8")                                return RS2_FORMAT_RGB8;
        if (encoding == "bgr8")                                return RS2_FORMAT_BGR8;
        if (encoding == "rgba8")                               return RS2_FORMAT_RGBA8;
        if (encoding == "bgra8")                               return RS2_FORMAT_BGRA8;
        if (encoding == "yuv422" || encoding == "yuv422_yuy2") return RS2_FORMAT_YUYV;
        if (encoding == "mono8"  || encoding == "8UC1")        return RS2_FORMAT_Y8;
        // 16-bit single-channel: Z16 for depth (realsense2_camera default), Y16 elsewhere (IR).
        if (encoding == "mono16" || encoding == "16UC1")
            return (stream_type == RS2_STREAM_DEPTH) ? RS2_FORMAT_Z16 : RS2_FORMAT_Y16;
        return RS2_FORMAT_ANY;
    }

    device_snapshot ros2_native_reader::read_native_device_description()
    {
        struct native_stream
        {
            std::string         image_topic;
            std::string         camera_info_topic;
            stream_identifier   id;
            rs2_stream          stream_type;
            bool                is_imu = false;
        };

        std::map<std::string, std::string> topic_to_type;
        for (const auto& t : _topics_cache) topic_to_type[t.name] = t.type;

        enum class sensor_kind : uint32_t { rgb, stereo, motion };
        auto kind_of = [](rs2_stream t) {
            switch (t)
            {
            case RS2_STREAM_COLOR:                                                  return sensor_kind::rgb;
            case RS2_STREAM_DEPTH: case RS2_STREAM_INFRARED:                        return sensor_kind::stereo;
            case RS2_STREAM_MOTION: case RS2_STREAM_ACCEL: case RS2_STREAM_GYRO:    return sensor_kind::motion;
            default:                                                                return sensor_kind::rgb;
            }
        };

        std::vector<native_stream> streams;
        std::map<rs2_stream, uint32_t> stream_index_counter;
        // RealSense convention: IR streams are 1-based (IR1, IR2); other types are 0-based.
        stream_index_counter[RS2_STREAM_INFRARED] = 1;

        // CameraInfo lives either as child (`<topic>/camera_info`) or sibling (`<parent>/camera_info`).
        auto find_camera_info = [&](const std::string& image_topic) -> std::string
        {
            auto check = [&](const std::string& candidate) {
                auto it = topic_to_type.find(candidate);
                return it != topic_to_type.end() && it->second == MSG_TYPE_CAMERA_INFO;
            };
            if (check(image_topic + "/camera_info"))
                return image_topic + "/camera_info";
            auto slash = image_topic.find_last_of('/');
            if (slash != std::string::npos)
            {
                auto sibling = image_topic.substr(0, slash) + "/camera_info";
                if (check(sibling))
                    return sibling;
            }
            return {};
        };

        for (const auto& t : _topics_cache)
        {
            const bool is_image = (t.type == MSG_TYPE_IMAGE);
            const bool is_imu   = (t.type == MSG_TYPE_IMU);
            if (!is_image && !is_imu) continue;

            std::string camera_info_topic;
            if (is_image)
            {
                camera_info_topic = find_camera_info(t.name);
                if (camera_info_topic.empty())
                {
                    LOG_WARNING("ros2_native_reader: no CameraInfo for image topic '" << t.name
                                << "' - stream will not be played back");
                    continue;
                }
            }

            rs2_stream stream_type = native_stream_type_from_topic(t.name);
            if (is_imu && (stream_type == RS2_STREAM_ANY)) stream_type = RS2_STREAM_MOTION;
            if (stream_type == RS2_STREAM_ANY)
            {
                LOG_WARNING("ros2_native_reader: could not infer stream type from topic '" << t.name
                            << "' - stream will not be played back");
                continue;
            }

            native_stream ns;
            ns.image_topic       = t.name;
            ns.camera_info_topic = camera_info_topic;
            ns.stream_type       = stream_type;
            ns.is_imu            = is_imu;
            ns.id.device_index   = 0;
            ns.id.stream_index   = stream_index_counter[stream_type]++;
            ns.id.stream_type    = stream_type;
            streams.push_back(ns);
        }

        if (streams.empty())
            throw io_exception("Native ROS2 file: no recognizable Image+CameraInfo topic pairs");

        // Assign contiguous 0..N-1 sensor indices in kind order (rgb < stereo < motion).
        // playback_device's frame dispatch rejects `sensor_index >= m_sensors.size()`.
        std::map<sensor_kind, uint32_t> kind_to_index;
        for (auto& s : streams) kind_to_index[kind_of(s.stream_type)];
        uint32_t next_idx = 0;
        for (auto& kv : kind_to_index) kv.second = next_idx++;
        for (auto& s : streams) s.id.sensor_index = kind_to_index[kind_of(s.stream_type)];

        std::vector<std::string> profile_topics;
        size_t video_streams_needed = 0;
        for (const auto& s : streams)
        {
            if (s.is_imu) continue;
            profile_topics.push_back(s.camera_info_topic);
            profile_topics.push_back(s.image_topic);
            ++video_streams_needed;
        }
        _storage->set_filter({ profile_topics });

        std::map<std::string, sensor_msgs::msg::CameraInfo> camera_infos;
        std::map<std::string, sensor_msgs::msg::Image>      first_images;
        while (_storage->has_next() && (camera_infos.size() < video_streams_needed || first_images.size() < video_streams_needed))
        {
            auto msg = _storage->read_next();
            if (!msg) break;

            auto type_it = topic_to_type.find(msg->topic_name);
            if (type_it == topic_to_type.end()) continue;
            try
            {
                decompress_if_needed(msg);
                if (type_it->second == MSG_TYPE_CAMERA_INFO && !camera_infos.count(msg->topic_name))
                    camera_infos[msg->topic_name] = deserialize_message<sensor_msgs::msg::CameraInfo>(msg);
                else if (type_it->second == MSG_TYPE_IMAGE && !first_images.count(msg->topic_name))
                    first_images[msg->topic_name] = deserialize_message<sensor_msgs::msg::Image>(msg);
            }
            catch (const std::exception& e)
            {
                LOG_WARNING("ros2_native_reader: failed to deserialize " << msg->topic_name << " during discovery: " << e.what());
            }
        }

        std::map<uint32_t, stream_profiles> sensor_to_streams;
        for (const auto& s : streams)
        {
            std::shared_ptr<stream_profile_interface> sp;
            if (s.is_imu)
            {
                auto msp = std::make_shared<motion_stream_profile>();
                msp->set_stream_type(s.stream_type);
                msp->set_stream_index(static_cast<int>(s.id.stream_index));
                msp->set_format(s.stream_type == RS2_STREAM_MOTION
                    ? RS2_FORMAT_COMBINED_MOTION : RS2_FORMAT_MOTION_XYZ32F);
                msp->set_framerate(0);
                sp = msp;
            }
            else
            {
                auto ci_it  = camera_infos.find(s.camera_info_topic);
                auto img_it = first_images.find(s.image_topic);
                if (ci_it == camera_infos.end() || img_it == first_images.end())
                {
                    LOG_WARNING("ros2_native_reader: missing CameraInfo or first Image for "
                                << s.image_topic << " - skipping");
                    continue;
                }
                const auto& ci  = ci_it->second;
                const auto& img = img_it->second;

                rs2_format format = native_format_from_image_encoding(img.encoding(), s.stream_type);
                if (format == RS2_FORMAT_ANY)
                {
                    LOG_WARNING("ros2_native_reader: unrecognized encoding '" << img.encoding()
                                << "' for " << s.image_topic << " - skipping");
                    continue;
                }

                auto vsp = std::make_shared<video_stream_profile>();
                vsp->set_stream_type(s.stream_type);
                vsp->set_stream_index(static_cast<int>(s.id.stream_index));
                vsp->set_format(format);
                vsp->set_framerate(0);
                vsp->set_dims(static_cast<uint32_t>(img.width()), static_cast<uint32_t>(img.height()));

                // Distortion model strings don't 1:1 map to rs2_distortion enums - leaving NONE.
                rs2_intrinsics intr{};
                intr.width  = static_cast<int>(img.width());
                intr.height = static_cast<int>(img.height());
                const auto& k = ci.k();
                intr.fx  = static_cast<float>(k[0]);
                intr.ppx = static_cast<float>(k[2]);
                intr.fy  = static_cast<float>(k[4]);
                intr.ppy = static_cast<float>(k[5]);
                intr.model = RS2_DISTORTION_NONE;
                vsp->set_intrinsics([intr]() { return intr; });
                sp = vsp;
            }

            sensor_to_streams[s.id.sensor_index].push_back(sp);
            _topic_to_stream_id[s.image_topic] = s.id;
            _profile_by_stream[s.id] = sp;
        }

        // All streams in a given sensor share a kind (see kind_to_index above), so the
        // first profile is enough to name the sensor.
        auto sensor_name_for_streams = [](const stream_profiles& profs) -> const char* {
            if (profs.empty()) return "Sensor";
            switch (profs.front()->get_stream_type())
            {
            case RS2_STREAM_DEPTH:
            case RS2_STREAM_INFRARED:                            return "Stereo Module";
            case RS2_STREAM_COLOR:                               return "RGB Camera";
            case RS2_STREAM_MOTION:
            case RS2_STREAM_ACCEL:
            case RS2_STREAM_GYRO:                                return "Motion Module";
            default:                                             return "Sensor";
            }
        };

        if (sensor_to_streams.empty())
            throw io_exception("Native ROS2 file: failed to build any stream profile - check CameraInfo/encoding warnings above");

        std::vector<sensor_snapshot> sensor_descriptions;
        for (auto& kv : sensor_to_streams)
        {
            uint32_t sensor_index = kv.first;
            snapshot_collection sensor_extensions;

            auto sensor_info = std::make_shared<info_container>();
            sensor_info->register_info(RS2_CAMERA_INFO_NAME, sensor_name_for_streams(kv.second));
            sensor_extensions[RS2_EXTENSION_INFO]    = sensor_info;
            sensor_extensions[RS2_EXTENSION_OPTIONS] = std::make_shared<options_container>();
            sensor_extensions[RS2_EXTENSION_RECOMMENDED_FILTERS]
                = std::make_shared<recommended_proccesing_blocks_snapshot>(processing_blocks{});

            const bool has_depth = std::any_of(kv.second.begin(), kv.second.end(),
                [](const auto& sp) { return sp->get_stream_type() == RS2_STREAM_DEPTH; });
            if (has_depth)
                sensor_extensions[RS2_EXTENSION_DEPTH_SENSOR]
                    = std::make_shared<native_depth_sensor_snapshot>(DEFAULT_DEPTH_UNITS);

            sensor_descriptions.emplace_back(sensor_index, sensor_extensions, kv.second);
        }

        // Don't register IP_ADDRESS / CONNECTION_TYPE / USB_TYPE_DESCRIPTOR - viewer would mis-badge.
        static const std::pair<rs2_camera_info, const char*> default_device_info[] = {
            { RS2_CAMERA_INFO_NAME,                         "Native ROS2 Recording" },
            { RS2_CAMERA_INFO_SERIAL_NUMBER,                "0000000000" },
            { RS2_CAMERA_INFO_FIRMWARE_VERSION,             "0.0.0.0" },
            { RS2_CAMERA_INFO_RECOMMENDED_FIRMWARE_VERSION, "0.0.0.0" },
            { RS2_CAMERA_INFO_PHYSICAL_PORT,                "ROS2 bag" },
            { RS2_CAMERA_INFO_DEBUG_OP_CODE,                "0" },
            { RS2_CAMERA_INFO_ADVANCED_MODE,                "NO" },
            { RS2_CAMERA_INFO_PRODUCT_ID,                   "0000" },
            { RS2_CAMERA_INFO_PRODUCT_LINE,                 "ROS2" },
            { RS2_CAMERA_INFO_CAMERA_LOCKED,                "NO" },
            { RS2_CAMERA_INFO_ASIC_SERIAL_NUMBER,           "0000000000" },
            { RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID,           "0000000000" },
            { RS2_CAMERA_INFO_DFU_DEVICE_PATH,              "N/A" },
        };
        auto device_info = std::make_shared<info_container>();
        for (const auto& kv : default_device_info)
            device_info->register_info(kv.first, kv.second);

        snapshot_collection device_extensions;
        device_extensions[RS2_EXTENSION_INFO] = device_info;
        return device_snapshot(device_extensions, sensor_descriptions, {});
    }

    std::shared_ptr<serialized_frame> ros2_native_reader::create_frame(
        const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg)
    {
        nanoseconds raw_ts(msg->time_stamp);
        auto sid_it = _topic_to_stream_id.find(msg->topic_name);
        if (sid_it == _topic_to_stream_id.end())
            return std::make_shared<serialized_invalid_frame>(raw_ts, stream_identifier{});
        const auto& stream_id = sid_it->second;

        nanoseconds timestamp(static_cast<int64_t>(msg->time_stamp) - _first_timestamp_ns);

        frame_additional_data additional_data{};
        const double ts_ms = static_cast<double>(timestamp.count()) / 1e6;
        additional_data.timestamp        = ts_ms;
        additional_data.system_time      = ts_ms;
        additional_data.timestamp_domain = RS2_TIMESTAMP_DOMAIN_SYSTEM_TIME;
        additional_data.frame_number     = ++_native_frame_counters[stream_id];
        additional_data.depth_units      = (stream_id.stream_type == RS2_STREAM_DEPTH) ? DEFAULT_DEPTH_UNITS : 0.f;

        const bool is_imu = (stream_id.stream_type == RS2_STREAM_MOTION
                          || stream_id.stream_type == RS2_STREAM_GYRO
                          || stream_id.stream_type == RS2_STREAM_ACCEL);
        std::vector<uint8_t> data;
        try
        {
            if (is_imu)
            {
                auto imu = deserialize_message<sensor_msgs::msg::Imu>(msg);
                if (stream_id.stream_type == RS2_STREAM_MOTION)
                {
                    rs2_combined_motion combined_motion{};
                    combined_motion.orientation = {
                        (imu.orientation().x()), (imu.orientation().y()),
                        (imu.orientation().z()), (imu.orientation().w()) };
                    combined_motion.angular_velocity = {
                        (imu.angular_velocity().x()), (imu.angular_velocity().y()),
                        (imu.angular_velocity().z()) };
                    combined_motion.linear_acceleration = {
                        (imu.linear_acceleration().x()), (imu.linear_acceleration().y()),
                        (imu.linear_acceleration().z()) };
                    auto ptr = reinterpret_cast<const uint8_t*>(&combined_motion);
                    data.assign(ptr, ptr + sizeof(combined_motion));
                }
                else
                {
                    data.resize(3 * sizeof(float));
                    auto motion_xyz = reinterpret_cast<float*>(data.data());
                    if (stream_id.stream_type == RS2_STREAM_GYRO)
                    {
                        motion_xyz[0] = static_cast<float>(imu.angular_velocity().x());
                        motion_xyz[1] = static_cast<float>(imu.angular_velocity().y());
                        motion_xyz[2] = static_cast<float>(imu.angular_velocity().z());
                    }
                    else // ACCEL
                    {
                        motion_xyz[0] = static_cast<float>(imu.linear_acceleration().x());
                        motion_xyz[1] = static_cast<float>(imu.linear_acceleration().y());
                        motion_xyz[2] = static_cast<float>(imu.linear_acceleration().z());
                    }
                }
            }
            else
            {
                auto img = deserialize_message<sensor_msgs::msg::Image>(msg);
                data = std::move(img.data());
            }
        }
        catch (const std::exception& e)
        {
            LOG_WARNING("ros2_native_reader: failed to deserialize " << msg->topic_name << ": " << e.what());
            return std::make_shared<serialized_invalid_frame>(timestamp, stream_id);
        }

        auto frame = alloc_and_move_frame(std::move(data), stream_id, std::move(additional_data));
        if (frame.frame == nullptr)
            return std::make_shared<serialized_invalid_frame>(timestamp, stream_id);

        auto result = std::make_shared<serialized_frame>(timestamp, stream_id, std::move(frame));
        _last_frame_cache[stream_id] = result;
        return result;
    }
}
