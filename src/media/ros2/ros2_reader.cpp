// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "ros2_reader.h"
#include "image.h"
#include "ds/ds-device-common.h"
#include "ds/d400/d400-private.h"
#include "ds/d500/d500-private.h"
#include <src/depth-sensor.h>
#include <src/core/pose-frame.h>
#include <src/core/motion-frame.h>
#include <src/core/video-frame.h>
#include <src/color-sensor.h>
#include <src/safety-sensor.h>
#include <src/depth-mapping-sensor.h>
#include <src/inference-sensor.h>
#include <src/points.h>
#include <src/labeled-points.h>
#include <src/context.h>
#include <src/object-detection-frame.h>
#include <rsutils/json.h>
#include <rsutils/number/crc32.h>

#include <cstring>


namespace librealsense
{
    using namespace device_serializer;

    // Basic string splitter helper
    std::vector<std::string> ros2_reader::split_string(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    std::string ros2_reader::get_value(const std::map<std::string, std::string>& kv, const std::string& key)
    {
        auto it = kv.find(key);
        if (it == kv.end())
            throw std::runtime_error(rsutils::string::from() << "Key not found: " << key);
        return it->second;
    }

    std::vector<std::string> ros2_reader::filter_topics_by_regex(const std::regex& re) const
    {
        std::vector<std::string> out;
        for (auto const& s : _topics_cache)
            if (std::regex_match(s.name, re))
                out.push_back(s.name);
        return out;
    }

    std::map< std::string, std::string > ros2_reader::parse_msg_payload(const std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg)
    {
        auto payload_str = deserialize_message<cdr_string>(msg).value;
        std::map< std::string, std::string > kv_map;
        auto pairs = split_string(payload_str, ';');
        for (const auto& pair : pairs)
        {
            auto kv = split_string(pair, '=');
            // Expect at least a key
            if (kv.size() >= 1) {
                std::string key = kv[0];
                std::string value = (kv.size() >= 2) ? kv[1] : "";
                kv_map[key] = value;
            }
        }
        return kv_map;
    }

    void ros2_reader::register_camera_infos(std::shared_ptr<info_container> infos, const std::map<std::string, std::string>& kv)
    {
        for (const auto& it : kv)
        {
            try
            {
                rs2_camera_info info;
                if (convert(it.first, info))
                {
                    infos->register_info(info, it.second);
                }
            }
            catch (const std::exception& e)
            {
                LOG_ERROR(rsutils::string::from() << "Exception in register_camera_infos: " << e.what());
            }
        }
    }

    ros2_reader::ros2_reader(const std::string& file, const std::shared_ptr<context> ctx)
        : ros2_reader_base(file, ctx)
    {
    }

    std::shared_ptr< serialized_data > ros2_reader::read_next_data()
    {
        if (!has_next_cached())
        {
            LOG_DEBUG("End of file reached");
            return std::make_shared<serialized_end_of_file>();
        }

        while (has_next_cached())
        {
            auto msg = read_next_cached();
            if (!msg)
            {
                LOG_ERROR("read_next_data: invalid message");
                continue;
            }

            std::string topic = msg->topic_name;
            nanoseconds ts(msg->time_stamp);

            // 1. Check if this is a frame data topic (e.g., /device_0/sensor_0/Depth_0/image/data)
            stream_identifier sid;
            if (is_stream_topic(topic, sid))
            {
                // Filter: if we have enabled streams and this isn't one, skip it
                if (!_enabled_streams.empty() && _enabled_streams.find(sid) == _enabled_streams.end())
                {
                    // The next message is expected to be metadata message, consume it to avoid error on it
                    read_next_cached();
                    continue;
                }
                LOG_DEBUG("Next message is a frame");
                return create_frame(msg);
            }

            // 2. Options
            if (topic.find("/option/") != std::string::npos)
            {
                LOG_DEBUG("Next message is an option");
                auto timestamp = nanoseconds(msg->time_stamp);
                auto sensor_id = ros2_topic::get_sensor_identifier(msg->topic_name);
                auto option = create_option(msg);
                return std::make_shared<serialized_option>(timestamp, sensor_id, option.first, option.second);
            }

            // 3. Notifications
            if (topic.find("/notification/") != std::string::npos)
            {
                LOG_DEBUG("Next message is a notification");
                auto timestamp = nanoseconds(msg->time_stamp);
                auto sensor_id = ros2_topic::get_sensor_identifier(msg->topic_name);
                auto notification = create_notification(msg);
                return std::make_shared<serialized_notification>(timestamp, sensor_id, notification);
            }

            LOG_ERROR("read_next_data: unknown message type on topic: " << topic);
        }
        return std::make_shared<serialized_end_of_file>();
    }

    std::shared_ptr< serialized_frame > ros2_reader::create_frame(const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg)
    {
        nanoseconds timestamp(msg->time_stamp);
        stream_identifier stream_id = ros2_topic::get_stream_identifier(msg->topic_name);

        // Read metadata from the next message (metadata immediately follows frame data)
        frame_additional_data additional_data{};
        read_frame_metadata(additional_data);

        bool is_imu_topic       = (msg->topic_name.find("/" + std::string(ros2_topic::ros_imu_type_str())              + "/") != std::string::npos);
        bool is_inference_topic = (msg->topic_name.find("/" + std::string(ros2_topic::ros_object_detection_type_str()) + "/") != std::string::npos);

        std::vector<uint8_t> data;

        if (is_inference_topic)
        {
            // Inference frames are stored as a JSON string (std_msgs/msg/String).
            // Re-parse using the same format written by ros2_writer and construct the binary payload.
            auto json_str = deserialize_message<cdr_string>(msg).value;
            auto j = rsutils::json::parse( json_str );

            auto n_detections = j.value( "number_of_detections", uint16_t(0) );

            size_t const base_size = sizeof( object_detection_frame::object_detection_payload )
                                   - sizeof( object_detection_frame::object_detection_entry );
            size_t const total_size = base_size + n_detections * sizeof( object_detection_frame::object_detection_entry );

            data.resize( total_size );
            auto * payload = reinterpret_cast< object_detection_frame::object_detection_payload * >( data.data() );

            payload->frame_id             = j.value( "frame_id", uint64_t(0) );
            payload->number_of_detections = n_detections;
            payload->source               = static_cast< uint8_t >( object_detection_frame::source::RGB );
            payload->source_frame_id      = j.value( "source_frame_id", uint32_t(0) );
            payload->timestamp_ms         = j.value( "timestamp_us", 0.0 ) * MICROSEC_TO_MILLISEC;

            auto dets_j = j.find( "detections" );
            if( dets_j != j.end() && dets_j->is_array() )
            {
                for( uint16_t i = 0; i < n_detections && i < dets_j->size(); ++i )
                {
                    auto const & det = (*dets_j)[i];
                    auto & e         = payload->detections[i];
                    e.detection_id   = i;
                    e.detection_type = det.value( "class_id",    uint8_t(0) );
                    e.confidence     = det.value( "confidence",  uint8_t(0) );
                    e.top_left_x     = det.value( "x1",          uint16_t(0) );
                    e.top_left_y     = det.value( "y1",          uint16_t(0) );
                    e.bottom_right_x = det.value( "x2",          uint16_t(0) );
                    e.bottom_right_y = det.value( "y2",          uint16_t(0) );
                    e.distance       = det.value( "distance",    0.0f );
                }
            }

            // Fill header so object_detection_frame::validate() passes
            payload->header.magic_number = object_detection_frame::MAGIC_NUMBER;
            payload->header.version      = static_cast< uint16_t >( j.value( "version", 1 ) );
            payload->header.data_type    = static_cast< uint8_t >( inference_frame::type::OBJECT_DETECTION );
            payload->header.flags        = 0;
            payload->header.spare        = 0;
            payload->header.size         = static_cast< uint32_t >( total_size - sizeof( object_detection_frame::object_detection_frame_header ) );
            uint8_t * payload_data       = reinterpret_cast< uint8_t * >( payload ) + sizeof( object_detection_frame::object_detection_frame_header );
            payload->header.crc32        = rsutils::number::calc_crc32( payload_data, payload->header.size );

            // Update additional_data fields from the JSON payload
            additional_data.frame_number = static_cast< unsigned long long >( payload->frame_id );
        }
        else if (is_imu_topic)
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

        auto frame = alloc_and_move_frame(std::move(data), stream_id, std::move(additional_data));

        if (frame.frame == nullptr)
        {
            return std::make_shared<serialized_invalid_frame>(timestamp, stream_id);
        }

        return std::make_shared<serialized_frame>(timestamp, stream_id, std::move(frame));
    }

    std::shared_ptr<stream_profile_interface> ros2_reader::read_next_stream_profile()
    {
        auto msg = read_next_cached();
        if (!msg)
            return nullptr;

        auto kv = parse_msg_payload(msg);
        auto encoding = get_value(kv, "encoding");
        auto fps = static_cast<uint32_t>(std::stoul(get_value(kv, "fps")));

        rs2_format format;
        convert(encoding, format);

        stream_identifier stream_id = ros2_topic::get_stream_identifier(msg->topic_name);

        // Peek at the next message — if it's intrinsics data, consume and use it;
        // otherwise this is a bare stream profile (e.g. inference) with no extra intrinsics.
        auto next = peek_next_cached();
        if (next && (next->topic_name.find("imu_intrinsic") != std::string::npos
                  || next->topic_name.find("camera_info")  != std::string::npos))
        {
            read_next_cached(); // consume
            auto intrinsics_kv = parse_msg_payload(next);

            if (next->topic_name.find("imu_intrinsic") != std::string::npos)
            {
                return create_motion_profile(stream_id, format, fps, intrinsics_kv);
            }
            else
            {
                return create_video_stream_profile(stream_id, format, fps, intrinsics_kv);
            }
        }

        // Bare stream profile — e.g. inference streams
        if (stream_id.stream_type == RS2_STREAM_OBJECT_DETECTION)
        {
            auto profile = std::make_shared<inference_stream_profile>();
            profile->set_framerate(fps);
            profile->set_format(format);
            profile->set_stream_index(int(stream_id.stream_index));
            profile->set_stream_type(stream_id.stream_type);
            return profile;
        }

        return nullptr;
    }

    rs2_motion_device_intrinsic ros2_reader::parse_motion_intrinsics(const std::map<std::string, std::string>& kv) const
    {
        rs2_motion_device_intrinsic intrinsics{};

        auto data_str = get_value(kv, "data");
        auto data_tokens = split_string(data_str, ',');
        for (size_t row = 0; row < 3; ++row)
        {
            for (size_t col = 0; col < 4; ++col)
            {
                intrinsics.data[row][col] = std::stof(data_tokens[row * 4 + col]);
            }
        }

        auto noise_str = get_value(kv, "noise_variances");
        auto noise_tokens = split_string(noise_str, ',');
        for (size_t i = 0; i < std::min(noise_tokens.size(), size_t(3)); ++i)
        {
            intrinsics.noise_variances[i] = std::stof(noise_tokens[i]);
        }

        auto bias_str = get_value(kv, "bias_variances");
        auto bias_tokens = split_string(bias_str, ',');
        for (size_t i = 0; i < std::min(bias_tokens.size(), size_t(3)); ++i)
        {
            intrinsics.bias_variances[i] = std::stof(bias_tokens[i]);
        }

        return intrinsics;
    }

    std::shared_ptr<motion_stream_profile> ros2_reader::create_motion_profile(const stream_identifier& stream_id, rs2_format format,
        uint32_t fps, const std::map<std::string, std::string>& intrinsics_kv) const
    {
        auto motion_profile = std::make_shared<motion_stream_profile>();
        motion_profile->set_stream_index(stream_id.stream_index);
        motion_profile->set_stream_type(stream_id.stream_type);
        motion_profile->set_format(format);
        motion_profile->set_framerate(fps);

        auto intrinsics = parse_motion_intrinsics(intrinsics_kv);
        motion_profile->set_intrinsics([intrinsics]() { return intrinsics; });

        return motion_profile;
    }

    std::pair<rs2_option, std::shared_ptr<librealsense::option>> ros2_reader::create_option(const std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg)
    {
        auto value_topic = msg->topic_name;
        std::string option_name = ros2_topic::get_option_name(value_topic);
        device_serializer::sensor_identifier sensor_id = ros2_topic::get_sensor_identifier(value_topic);
        rs2_option id;
        std::replace(option_name.begin(), option_name.end(), '_', ' ');
        convert(option_name, id);
        auto message_payload = deserialize_message<cdr_string>(msg).value;
        float value = std::stof(message_payload);
        std::string description = read_option_description(sensor_id.sensor_index, id);
        return std::make_pair(id, std::make_shared<const_value_option>(description, value));
    }

    notification ros2_reader::create_notification(const std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg) const
    {
        auto kv = parse_msg_payload(msg);
        rs2_notification_category category;
        rs2_log_severity severity;
        convert(get_value(kv, "category"), category);
        convert(get_value(kv, "severity"), severity);
        std::string description = get_value(kv, "description");
        notification n(category, 0, severity, description);
        n.timestamp = std::stod(get_value(kv, "timestamp"));
        n.serialized_data = get_value(kv, "data");
        return n;
    }

    std::shared_ptr<options_container> librealsense::ros2_reader::read_sensor_options(device_serializer::sensor_identifier sensor_id)
    {
        std::shared_ptr<options_container> sensor_options = std::make_shared<options_container>();

        // After info messages, we expect option messages
        for (int i = 0; i < static_cast<int>(RS2_OPTION_COUNT); i++)
        {
            rs2_option id = static_cast<rs2_option>(i);
            auto value_topic = ros2_topic::option_value_topic(sensor_id, id);
            std::string option_name = ros2_topic::get_option_name(value_topic);
            auto rs2_option_name = rs2_option_to_string(id); //option name with space seperator

            auto msg = peek_next_cached();
            if (msg && msg->topic_name == value_topic)
            {
                msg = read_next_cached();
                auto option = create_option(msg);
                assert(id == option.first);
                sensor_options->register_option(option.first, option.second);
            }
        }

        return sensor_options;
    }

    std::shared_ptr<processing_block_interface> ros2_reader::create_processing_block(const std::string& name_in, bool& depth_to_disparity, std::shared_ptr<options_interface> options)
    {
        std::string name = name_in;
        if (name == "Disparity Filter")
        {
            // What was recorded was the extension type (without its settings!), but we need to create different
            // variants. "Disparity Filter" gets recorded twice! This workaround ensures it's instantiated in its
            // non-default flavor the second time:
            if (depth_to_disparity)
                depth_to_disparity = false;
            else
                name = "Disparity to Depth";
        }
        try
        {
            auto block = m_context->create_pp_block(name, {});
            if (!block)
                LOG_DEBUG("unknown processing block '" << name << "'; ignored");
            return block;
        }
        catch (std::exception const& e)
        {
            LOG_DEBUG("failed to create processing block '" << name << "': " << e.what());
            return {};
        }
    }

    void ros2_reader::read_frame_metadata(frame_additional_data& additional_data)
    {
        // Read the next message which should be the metadata for this frame
        if (!has_next_cached())
            return;

        auto md_msg = peek_next_cached();
        if (!md_msg || md_msg->topic_name.find("/metadata") == std::string::npos)
            return;
        
        // Consume the metadata message
        md_msg = read_next_cached();

        auto kv = parse_msg_payload(md_msg);

        additional_data.frame_number = std::stoull(get_value(kv, FRAME_NUMBER_MD_STR));
        convert(get_value(kv, TIMESTAMP_DOMAIN_MD_STR), additional_data.timestamp_domain);  
        convert(get_value(kv, SYSTEM_TIME_MD_STR), additional_data.system_time);
        additional_data.timestamp = std::stod(get_value(kv, TIMESTAMP_MD_STR));
        additional_data.depth_units = kv.count(DEPTH_UNITS_MD_STR) ? std::stof(get_value(kv, DEPTH_UNITS_MD_STR)) : 0.f;

        // Iterate only the keys present in the map, matching them to RS2 metadata types
        uint32_t total_md_size = 0;
        constexpr uint32_t size_of_enum = sizeof(rs2_frame_metadata_value);
        constexpr uint32_t size_of_data = sizeof(rs2_metadata_type);

        // Build a static reverse lookup: metadata name string -> enum value
        static const auto& name_to_md = []() -> const std::map<std::string, rs2_frame_metadata_value>& {
            static std::map<std::string, rs2_frame_metadata_value> m;
            for (int i = 0; i < RS2_FRAME_METADATA_COUNT; i++)
            {
                auto md_type = static_cast<rs2_frame_metadata_value>(i);
                m[librealsense::get_string(md_type)] = md_type;
            }
            return m;
        }();

        for (const auto& entry : kv)
        {
            auto it = name_to_md.find(entry.first);
            if (it == name_to_md.end())
                continue;  // not a metadata key (e.g. Frame number, Timestamp Domain, etc.)

            if (total_md_size + size_of_enum + size_of_data > additional_data.metadata_blob.size())
                break;

            try
            {
                rs2_metadata_type md_value;
                convert(entry.second, md_value);

                auto md_type = it->second;
                std::memcpy(additional_data.metadata_blob.data() + total_md_size, &md_type, size_of_enum);
                total_md_size += size_of_enum;
                std::memcpy(additional_data.metadata_blob.data() + total_md_size, &md_value, size_of_data);
                total_md_size += size_of_data;
            }
            catch (const std::exception&)
            {
                continue;
            }
        }

        additional_data.metadata_size = total_md_size;
    }

    void ros2_reader::setup_frame(frame_interface* frame_ptr, const stream_identifier& sid) const
    {
        for (auto& sensor_snap : m_initial_device_description.get_sensors_snapshots())
        {
            if (sensor_snap.get_sensor_index() != sid.sensor_index)
                continue;

            for (auto& stream_profile : sensor_snap.get_stream_profiles())
            {
                if (stream_profile->get_stream_type() != sid.stream_type ||
                    stream_profile->get_stream_index() != sid.stream_index)
                    continue;

                frame_ptr->set_stream(stream_profile);

                // For video frames, set dimensions
                auto vsp = std::dynamic_pointer_cast<video_stream_profile>(stream_profile);
                if (!vsp) 
                    return; // Not a video stream

                auto video_frame_ptr = dynamic_cast<video_frame*>(frame_ptr);
                if (!video_frame_ptr)
                {
                    if (dynamic_cast<labeled_points*>(frame_ptr))
                        return;
                    throw std::runtime_error("Profile is video stream but frame is not video frame");
                }

                int width = vsp->get_width();
                int height = vsp->get_height();
                int bpp = get_image_bpp(vsp->get_format());
                int stride = width * bpp / 8;
                // derive bpp/stride from the recorded payload,
                // else use the values computed above
                auto data_size = static_cast<librealsense::frame*>(frame_ptr)->data.size();
                auto pixels = static_cast<size_t>(width) * height;
                if (pixels > 0 && data_size % pixels == 0)
                {
                    int bpp_bytes = static_cast<int>(data_size / pixels);
                    bpp = bpp_bytes * 8;
                    stride = width * bpp_bytes;
                }
                video_frame_ptr->assign(width, height, stride, bpp);
                return;
            }
        }
        
        throw std::runtime_error("Failed to setup frame: stream profile not found");
    }

    uint32_t ros2_reader::read_file_version()
    {
        auto msg = read_next_cached();
        return deserialize_message<cdr_uint32>(msg).value;
    }

    bool ros2_reader::try_read_stream_extrinsic(const stream_identifier& stream_id, uint32_t& group_id, rs2_extrinsics& extrinsic)
    {
        auto msg = peek_next_cached();
        if (!msg)
        {
            return false;
        }
        // Check if this is the extrinsic topic for the requested stream
        // Some devices might not have extrinsics for all streams, ie. software device unless explicitly set
        auto regex_str = (rsutils::string::from() << "^/device_" << stream_id.device_index <<
                                                     "/sensor_\\d+/[^/]+/tf/ref_\\d+$").str();
        auto extrinsic_topics = filter_topics_by_regex(std::regex(regex_str));
        if (std::find(extrinsic_topics.begin(), extrinsic_topics.end(), msg->topic_name) == extrinsic_topics.end())
        {
            return false;
        }
        msg = read_next_cached();
        group_id = ros2_topic::get_extrinsic_group_index(msg->topic_name);
        auto kv = parse_msg_payload(msg);

        // Parse rotation (9 floats) and translation (3 floats)
        auto rotation_it = kv.find("rotation");
        auto translation_it = kv.find("translation");

        if (rotation_it != kv.end() && translation_it != kv.end())
        {
            auto rot_tokens = split_string(rotation_it->second, ',');
            auto trans_tokens = split_string(translation_it->second, ',');

            for (int i = 0; i < 9; ++i)
            {
                extrinsic.rotation[i] = std::stof(rot_tokens[i]);
            }
            for (int i = 0; i < 3; ++i)
            {
                extrinsic.translation[i] = std::stof(trans_tokens[i]);
            }
        }
        return true;
    }

    std::shared_ptr<recommended_proccesing_blocks_snapshot> ros2_reader::update_proccesing_blocks(uint32_t sensor_index, std::shared_ptr<options_container> sensor_options)
    {
        auto options_snapshot = sensor_options;
        if (options_snapshot == nullptr)
        {
            LOG_WARNING("Recorded file does not contain sensor options");
        }
        auto options_api = As<options_interface>(options_snapshot);
        if (options_api == nullptr)
        {
            throw invalid_value_exception("Failed to get options interface from sensor snapshots");
        }
        auto proccesing_blocks = read_proccesing_blocks(
            {get_device_index(), sensor_index},
            options_api
        );
        return proccesing_blocks;
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

    class inference_sensor_snapshot
        : public virtual inference_sensor
        , public extension_snapshot
    {
    public:
        void update(std::shared_ptr< extension_snapshot > ext) override {}
    };

    class object_detection_sensor_snapshot
        : public virtual object_detection_sensor
        , public inference_sensor_snapshot
    {
    public:
        void update(std::shared_ptr< extension_snapshot > ext) override {}
    };

    }  // namespace


    void ros2_reader::add_sensor_extension(snapshot_collection& sensor_extensions, const std::string& sensor_name)
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
                float depth_units = 0.001f; // Default 1mm (0.001m) for devices that don't have this option recorded
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
        else if (is_object_detection_sensor(sensor_name))
        {
            sensor_extensions[RS2_EXTENSION_OBJECT_DETECTION_SENSOR] = std::make_shared<object_detection_sensor_snapshot>();
            sensor_extensions[RS2_EXTENSION_INFERENCE_SENSOR] = std::make_shared<inference_sensor_snapshot>();
        }
        else if (is_inference_module_sensor(sensor_name))
        {
            sensor_extensions[RS2_EXTENSION_INFERENCE_SENSOR] = std::make_shared<inference_sensor_snapshot>();
        }
    }


    bool ros2_reader::is_depth_sensor(const std::string& sensor_name)
    {
        return (sensor_name.compare("Stereo Module") == 0 || sensor_name.compare("Coded-Light Depth Sensor") == 0);
    }

    bool ros2_reader::is_stereo_depth_sensor(const std::string& sensor_name)
    {
        return sensor_name.compare( "Stereo Module" ) == 0;
    }

    bool ros2_reader::is_color_sensor(const std::string& sensor_name)
    {
        return sensor_name.compare( "RGB Camera" ) == 0;
    }

    bool ros2_reader::is_motion_module_sensor(const std::string& sensor_name)
    {
        return (sensor_name.compare("Motion Module") == 0);
    }

    bool ros2_reader::is_fisheye_module_sensor(const std::string& sensor_name)
    {
        return (sensor_name.compare("Wide FOV Camera") == 0);
    }

    bool ros2_reader::is_safety_module_sensor(const std::string& sensor_name)
    {
        return (sensor_name.compare("Safety Camera") == 0);
    }

    bool ros2_reader::is_depth_mapping_sensor(const std::string& sensor_name)
    {
        return (sensor_name.compare("Depth Mapping Camera") == 0);
    }

    bool ros2_reader::is_inference_module_sensor(const std::string& sensor_name)
    {
        return (sensor_name.compare("Inference Sensor") == 0);
    }

    bool ros2_reader::is_object_detection_sensor(const std::string& sensor_name)
    {
        return (sensor_name.compare("Object Detection Sensor") == 0);
    }

    // Helpers ---------------------------------------------------------------------

    bool ros2_reader::is_stream_topic(const std::string& topic, stream_identifier& id) const
    {
        // Format: /device_N/sensor_N/StreamType_Idx/<ros_type>/data
        if (topic.find("/device_") != 0)
            return false;

        // Frame data topics end with /data (e.g., .../image/data, .../imu/data)
        auto data_suffix = std::string("/data");
        if (topic.size() < data_suffix.size()
            || topic.compare(topic.size() - data_suffix.size(), data_suffix.size(), data_suffix) != 0)
            return false;

        try
        {
            // If stream topic, parse stream identifier
            id = ros2_topic::get_stream_identifier(topic);
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_WARNING("Failed to parse stream identifier from topic '" << topic << "': " << e.what());
            return false;
        }
    }

    std::shared_ptr<recommended_proccesing_blocks_snapshot> ros2_reader::read_proccesing_blocks(device_serializer::sensor_identifier sensor_id, std::shared_ptr<options_interface> options)
    {
        std::string proccesing_block_topic = ros2_topic::post_processing_blocks_topic(sensor_id);
        auto msg = peek_next_cached();
        processing_blocks blocks;
        auto depth_to_disparity = true;
        if (msg && msg->topic_name == proccesing_block_topic)
        {
            msg = read_next_cached();
            auto payload = deserialize_message<cdr_string>(msg).value;
            for (auto& name : split_string(payload, ';'))
            {
                auto block = create_processing_block(name, depth_to_disparity, options);
                if (block)
                    blocks.push_back(block);
            }
        }
        return std::make_shared<recommended_proccesing_blocks_snapshot>(blocks);
    }

    device_snapshot ros2_reader::read_device_description(const nanoseconds& time, bool reset)
    {
        if (_initialized) return m_initial_device_description;

        _topics_cache = _storage->get_all_topics_and_types();

        //// Read sensor indices from topics cached - does not read from storage
        std::vector<sensor_snapshot> sensor_descriptions;
        constexpr auto device_index = get_device_index();
        auto sensor_indices = read_sensor_indices(device_index);
        
        // filter all device info topics
        auto device_info_regex_str               = (rsutils::string::from() << "^/device_" << get_device_index() << "/info$").str();
        auto sensor_info_regex_str               = (rsutils::string::from() << "^/device_" << get_device_index() << "/sensor_\\d+/info$").str();
        auto sensor_option_regex_str             = (rsutils::string::from() << "^/device_" << get_device_index() << "/sensor_\\d+/option/[^/]+/value$").str();
        auto sensor_option_description_regex_str = (rsutils::string::from() << "^/device_" << get_device_index() << "/sensor_\\d+/option/[^/]+/description$").str();
        auto stream_info_regex_str               = (rsutils::string::from() << "^/device_" << get_device_index() << "/sensor_\\d+/[^/]+/info$").str();
        auto stream_info_intrinsics_regex_str    = (rsutils::string::from() << "^/device_" << get_device_index() << "/sensor_\\d+/[^/]+/(camera_info|imu_intrinsic)$").str();
        auto post_processing_blocks_regex_str    = (rsutils::string::from() << "^/device_" << get_device_index() << "/sensor_\\d+/post_processing$").str();
        auto extrinsics_regex_str                = (rsutils::string::from() << "^/device_" << get_device_index() << "/sensor_\\d+/[^/]+/tf/ref_\\d+$").str();

        auto regex_str = (rsutils::string::from() << "("
            << device_info_regex_str << "|"
            << sensor_info_regex_str << "|"
            << sensor_option_regex_str << "|"
            << sensor_option_description_regex_str << "|"
            << stream_info_regex_str << "|"
            << stream_info_intrinsics_regex_str << "|"
            << post_processing_blocks_regex_str << "|"
            << extrinsics_regex_str
            << ")").str();
        auto regex = std::regex(regex_str);
        auto filter_topics = filter_topics_by_regex(regex);

        _storage->set_filter({filter_topics});

        snapshot_collection device_extensions;
        auto sensors_info = std::map<uint32_t, std::shared_ptr<info_container>>();
        auto sensors_options = std::map<uint32_t, std::shared_ptr<options_container>>();
        auto sensors_processing_blocks = std::map<uint32_t, std::shared_ptr<recommended_proccesing_blocks_snapshot>>();
        std::map<uint32_t, stream_profiles> sensor_to_streams;
        while (has_next_cached())
        {
            auto msg = peek_next_cached();
            if (!msg)
            {
                throw std::runtime_error("read_device_description: invalid message");
            }
            if (std::regex_match(msg->topic_name, std::regex(device_info_regex_str)))
            {
                auto device_info = read_info_snapshot(msg->topic_name); // Will read all device info messages
                device_extensions[RS2_EXTENSION_INFO] = device_info;
            }
            else if (std::regex_match(msg->topic_name, std::regex(sensor_info_regex_str)))
            {
                uint32_t sensor_index = ros2_topic::get_sensor_index(msg->topic_name);
                sensors_info[sensor_index] = read_info_snapshot(msg->topic_name);
            }
            else if (std::regex_match(msg->topic_name, std::regex(sensor_option_regex_str)))
            {
                uint32_t sensor_index = ros2_topic::get_sensor_index(msg->topic_name);
                sensors_options[sensor_index] = read_sensor_options({ get_device_index(), sensor_index });
            }
            else if (std::regex_match(msg->topic_name, std::regex(post_processing_blocks_regex_str)))
            {
                uint32_t sensor_index = ros2_topic::get_sensor_index(msg->topic_name);
                auto sensor_options = sensors_options[sensor_index]; // Assuming options were already read
                sensors_processing_blocks[sensor_index] = update_proccesing_blocks(sensor_index, sensor_options);
            }
            else if (std::regex_match(msg->topic_name, std::regex(extrinsics_regex_str)))
            {
                stream_identifier stream_id = ros2_topic::get_stream_identifier(msg->topic_name);
                uint32_t reference_id;
                rs2_extrinsics stream_extrinsic;
                if (try_read_stream_extrinsic(stream_id, reference_id, stream_extrinsic))
                {
                    m_extrinsics_map[stream_id] = std::make_pair(reference_id, stream_extrinsic);
                }
            }
            else if (std::regex_match(msg->topic_name, std::regex(stream_info_regex_str)))
            {
                stream_identifier stream_id = ros2_topic::get_stream_identifier(msg->topic_name);
                auto stream_profile = read_next_stream_profile();
                if (!stream_profile)
                    throw std::runtime_error(rsutils::string::from() << "Failed to read stream profile for topic: " << msg->topic_name);

                sensor_to_streams[stream_id.sensor_index].push_back(stream_profile);
            }
        }

        // Build sensor descriptions from info and streams
        for (auto sensor_index : sensor_indices)
        {
            snapshot_collection sensor_extensions;
            sensor_extensions[RS2_EXTENSION_INFO] = sensors_info[sensor_index];
            auto proccesing_blocks = sensors_processing_blocks.find(sensor_index) != sensors_processing_blocks.end() ?
                sensors_processing_blocks[sensor_index] : std::make_shared<recommended_proccesing_blocks_snapshot>(processing_blocks{});
            sensor_extensions[RS2_EXTENSION_RECOMMENDED_FILTERS] = proccesing_blocks;

            auto& sensor_options = sensors_options[sensor_index];
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

            // Get sensor name and add appropriate sensor extension
            std::string sensor_name = "";
            auto sensor_info = sensors_info[sensor_index];
            if (sensor_info && sensor_info->supports_info(RS2_CAMERA_INFO_NAME))
            {
                sensor_name = sensor_info->get_info(RS2_CAMERA_INFO_NAME);
            }
            add_sensor_extension(sensor_extensions, sensor_name);

            auto& sensor_streams = sensor_to_streams[sensor_index];
            sensor_descriptions.emplace_back(sensor_index, sensor_extensions, sensor_streams);
        }

        m_initial_device_description = device_snapshot(device_extensions, sensor_descriptions, m_extrinsics_map);
        _initialized = true;

        prepare_for_streaming();

        return m_initial_device_description;
    }

    std::vector<std::string> ros2_reader::get_stream_topics() const
    {
        // /device_N/sensor_N/StreamType_Idx/<ros_type>/(data|metadata)
        auto re = std::regex((rsutils::string::from() << "^/device_" << get_device_index() << "/sensor_\\d+/[^/]+/[^/]+/(data|metadata)$").str());
        return filter_topics_by_regex(re);
    }

    std::vector<std::string> ros2_reader::get_option_topics() const
    {
        // /device_{device_index}/sensor_{sensor_index}/option/{option_name}/value
        auto re = std::regex((rsutils::string::from() << "^/device_" << get_device_index() << "/sensor_\\d+/option/[^/]+/value$").str());
        return filter_topics_by_regex(re);
    }

    std::vector<std::string> ros2_reader::get_notification_topics() const
    {
        // /device_{device_index}/sensor_{sensor_index}/notification/{notification_type}
        auto re = std::regex((rsutils::string::from() << "^/device_" << get_device_index() << "/sensor_\\d+/notification/[^/]+$").str());
        return filter_topics_by_regex(re);
    }

    std::shared_ptr<info_container> ros2_reader::read_info_snapshot(const std::string& topic)
    {
        auto infos = std::make_shared<info_container>();
        auto msg = peek_next_cached();
        if (msg && msg->topic_name == topic)
        {
            msg = read_next_cached();
            auto kv = parse_msg_payload(msg);
            register_camera_infos(infos, kv);
        }
        return infos;
    }

    std::set<uint32_t> ros2_reader::read_sensor_indices(uint32_t device_index) const
    {
        std::regex regex((rsutils::string::from() << "^/device_" << device_index
            << "/sensor_(\\d+)/info$").str());
        auto stream_info_topics = filter_topics_by_regex(regex);

        std::set<uint32_t> sensor_indices;
        for (const auto& topic : stream_info_topics) {
            sensor_indices.insert(ros2_topic::get_sensor_index(topic));
        }
        return sensor_indices;
    }

    std::shared_ptr<video_stream_profile> ros2_reader::create_video_stream_profile(const stream_identifier& stream_id, rs2_format format,
        uint32_t fps, const std::map<std::string, std::string>& intrinsics_kv)
    {
        auto profile = std::make_shared<video_stream_profile>();
        rs2_intrinsics intrinsics{};
        intrinsics.height = static_cast<int>(std::stoul(get_value(intrinsics_kv, "height")));
        intrinsics.width = static_cast<int>(std::stoul(get_value(intrinsics_kv, "width")));
        intrinsics.fx = std::stof(get_value(intrinsics_kv, "fx"));
        intrinsics.ppx = std::stof(get_value(intrinsics_kv, "ppx"));
        intrinsics.fy = std::stof(get_value(intrinsics_kv, "fy"));
        intrinsics.ppy = std::stof(get_value(intrinsics_kv, "ppy"));
        intrinsics.model = RS2_DISTORTION_NONE;
        auto dist_model_str = get_value(intrinsics_kv, "model");
        rs2_distortion dist_model;
        convert(dist_model_str, dist_model);
        intrinsics.model = dist_model;

        auto coeffs_str = get_value(intrinsics_kv, "coeffs");
        auto coeffs_tokens = split_string(coeffs_str, ',');
        for (size_t i = 0; i < std::min(coeffs_tokens.size(), size_t(5)); ++i)
        {
            intrinsics.coeffs[i] = std::stof(coeffs_tokens[i]);
        }

        profile->set_stream_index(stream_id.stream_index);
        profile->set_stream_type(stream_id.stream_type);
        profile->set_format(format);
        profile->set_framerate(fps);

        uint32_t width = static_cast<uint32_t>(intrinsics.width);
        uint32_t height = static_cast<uint32_t>(intrinsics.height);

        profile->set_dims(width, height);
        profile->set_intrinsics([intrinsics]() { return intrinsics; });

        return profile;
    }

    std::string ros2_reader::read_option_description(const uint32_t sensor_index, const rs2_option& id)
    {
        static const auto is_topic_description_topic = [](const std::string& topic, uint32_t sensor_index, rs2_option id)
            {
                return topic == ros2_topic::option_description_topic({ get_device_index(), sensor_index }, id);
            };

        const auto find_description = [this](uint32_t sensor_index, rs2_option id) -> const std::string*
            {
                auto sensor_it = m_read_options_descriptions.find(sensor_index);
                if (sensor_it == m_read_options_descriptions.end())
                    return nullptr;

                auto option_it = sensor_it->second.find(id);
                if (option_it == sensor_it->second.end())
                    return nullptr;

                return &option_it->second;
            };

        auto msg = peek_next_cached();
        if (!msg)
        {
            LOG_ERROR("read_option_description: invalid message");
            return "";
        }

        if (is_topic_description_topic(msg->topic_name, sensor_index, id))
        {
            // If the next message is the description topic, read it and return the description
            msg = read_next_cached();
            auto description = deserialize_message<cdr_string>(msg).value;
            m_read_options_descriptions[sensor_index][id] = description;
            return description;
        }

        if (auto description = find_description(sensor_index, id))
        {
            return *description;
        }

        // Not expected to reach here - no description available and next message is not the description topic.
        LOG_WARNING("Option description for sensor " << sensor_index << " option " << id << " not found. Returning empty description.");
        return "";
    }

}