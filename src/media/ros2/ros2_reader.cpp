// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "ros2_reader.h"
#include <rcutils/time.h>
#include <rsutils/string/from.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <limits>
#include "rosbag2_storage_default_plugins/sqlite/sqlite_storage.hpp"
#include <regex>
#include "core/video-frame.h"
#include "core/motion-frame.h"
#include "core/pose-frame.h"
#include "stream.h"
#include "source.h"
#include "image.h"
#include <src/context.h>
#include <src/depth-sensor.h>
#include <src/color-sensor.h>
#include <src/safety-sensor.h>
#include <src/depth-mapping-sensor.h>
#include <src/sensor.h>

namespace librealsense
{
    using namespace device_serializer;

    // Basic string splitter helper
    static std::vector<std::string> split_string(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    static std::string get_value(const std::map<std::string, std::string>& kv, const std::string& key)
    {
        auto it = kv.find(key);
        if (it == kv.end())
            throw std::runtime_error(rsutils::string::from() << "Key not found: " << key);
        return it->second;
    }

    std::vector<std::string> filter_by_regex(const std::vector<rosbag2_storage::TopicMetadata>& input,
        const std::regex& re)
    {
        std::vector<std::string> out;
        for (auto const& s : input)
            if (std::regex_match(s.name, re))
                out.push_back(s.name);
        return out;
    }



    std::map< std::string, std::string > ros2_reader::parse_key_value_string(const std::string& payload) const
    {
        std::map< std::string, std::string > kv_map;
        auto pairs = split_string(payload, ';');
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

    std::map< std::string, std::string > ros2_reader::parse_msg_payload(const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg) const
    {
        std::string payload_str;
        if (msg && msg->serialized_data && msg->serialized_data->buffer_length > 0)
        {
            payload_str = std::string(reinterpret_cast<const char*>(msg->serialized_data->buffer), msg->serialized_data->buffer_length);
        }
        return parse_key_value_string(payload_str);
    }

    void ros2_reader::register_camera_infos(std::shared_ptr<info_container>& infos, const std::map<std::string, std::string>& kv) const
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

    ros2_reader::ros2_reader(const std::string& file_path, const std::shared_ptr<context>& ctx) :
        m_metadata_parser_map(md_constant_parser::create_metadata_parser_map()),
        m_total_duration(0),
        m_file_path(file_path + ".db3"),
        m_context(ctx),
        m_version(0)
    {
        try
        {
            reset(); //Note: calling a virtual function inside c'tor, safe while base function is pure virtual
            m_total_duration = get_file_duration();
        }
        catch (const std::exception& e)
        {
            //Rethrowing with better clearer message
            throw io_exception( rsutils::string::from() << "Failed to create ros reader: " << e.what() );
        }
    }

    device_snapshot ros2_reader::query_device_description(const nanoseconds& time)
    {
        return read_device_description(time);
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

            std::string topic = msg->topic_name;
            nanoseconds ts(msg->time_stamp);

            // 1. Check if this is a frame data topic (e.g., /device_0/sensor_0/Depth_0/image/data)
            stream_identifier sid;
            if (is_stream_topic(topic, sid))
            {
                // Filter: if we have enabled streams and this isn't one, skip it
                if (!_enabled_streams.empty() && _enabled_streams.find(sid) == _enabled_streams.end())
                {
                    // Following message is expected to be metadata, consume it
                    read_next_cached();
                    continue;
                }
                LOG_DEBUG("Next message is a frame");
                return read_frame_data(msg, sid);
            }

            // 2. Options (e.g., /device_0/sensor_0/option/...)
            sensor_identifier sensor_id;
            rs2_option opt;
            if (is_option_topic(topic, sensor_id, opt))
            {
                LOG_DEBUG("Next message is an option");
                auto timestamp = nanoseconds(msg->time_stamp);
                auto sensor_id = ros_topic::get_sensor_identifier(msg->topic_name);
                auto option = create_option(msg);
                return std::make_shared<serialized_option>(timestamp, sensor_id, option.first, option.second);
            }

            // 3. Notifications
            if (topic.find("/notification/") != std::string::npos)
            {
                LOG_DEBUG("Next message is a notification");
                auto timestamp = nanoseconds(msg->time_stamp);
                auto sensor_id = ros_topic::get_sensor_identifier(msg->topic_name);
                auto notification = create_notification(msg);
                return std::make_shared<serialized_notification>(timestamp, sensor_id, notification);
            }

            std::string err_msg = rsutils::string::from() << "Unknown message type "
                                                          << "(Topic: " << msg->topic_name << ")"
                                                          << "(Timestamp: " << msg->time_stamp << ")";
            LOG_ERROR(err_msg);
            throw invalid_value_exception(err_msg);
        }
        return std::make_shared<serialized_end_of_file>(); // reached if only disabled streams are left
    }

    void ros2_reader::seek_to_time(const nanoseconds& seek_time)
    {
        // read all messages up to the requested time, updating the last frame is done inside the read function

        if (seek_time > m_total_duration)
        {
            throw invalid_value_exception( rsutils::string::from()
                                           << "Requested time is out of playback length. (Requested = "
                                           << seek_time.count() << ", Duration = " << m_total_duration.count() << ")" );
        }

        reset();

        while (nanoseconds(peek_next_cached()->time_stamp) < seek_time)
        {
            read_next_cached();
        }
    }

    std::vector<std::shared_ptr<serialized_data>> ros2_reader::fetch_last_frames(const nanoseconds& seek_time)
    {
        std::vector<std::shared_ptr<serialized_data>> frames;
        for (auto&& kv : _last_frame_cache)
        {
            // Filter by enabled streams
            if (_enabled_streams.empty() || _enabled_streams.count(kv.first))
            {
                if (kv.second) frames.push_back(kv.second);
            }
        }
        return frames;
    }
    nanoseconds ros2_reader::query_duration() const
    {
        return m_total_duration;
    }

    void ros2_reader::reset()
    {
        _storage = std::make_shared< rosbag2_storage_plugins::SqliteStorage >();
        _storage->open(m_file_path, rosbag2_storage::storage_interfaces::IOFlag::READ_ONLY);
        m_version = read_file_version();
        _last_frame_cache.clear();
        _initialized = false;
        m_read_options_descriptions.clear();
        m_frame_source = std::make_shared<frame_source>(32);
        m_frame_source->init(m_metadata_parser_map);
        m_initial_device_description = read_device_description(nanoseconds(0), true);
    }

    void ros2_reader::enable_stream(const std::vector<device_serializer::stream_identifier>& stream_ids)
    {
        for (const auto& id : stream_ids) _enabled_streams.insert(id);
    }

    void ros2_reader::disable_stream(const std::vector<device_serializer::stream_identifier>& stream_ids)
    {
        for (const auto& id : stream_ids) _enabled_streams.erase(id);
    }

    const std::string& ros2_reader::get_file_name() const 
    {
        return m_file_path;
    }

    nanoseconds ros2_reader::get_file_duration()
    {
        auto meta = _storage->get_metadata();
        return nanoseconds(meta.duration.count());
    }

    std::shared_ptr<stream_profile_interface> ros2_reader::read_next_stream_profile()
    {
        auto msg = read_next_cached();
        if (!msg)
            return nullptr;

        auto kv = parse_msg_payload(msg);
        auto encoding = get_value(kv, "encoding");
        auto fps = static_cast<uint32_t>(std::stoul(get_value(kv, "fps")));
        //auto is_recommended = (kv.find("is_recommended")->second == "true");

        rs2_format format;
        convert(encoding, format);

        stream_identifier stream_id = ros_topic::get_stream_identifier(msg->topic_name);

        msg = read_next_cached();
        if (!msg)
            return nullptr;
        
        auto intrinsics_kv = parse_msg_payload(msg);

        if (msg->topic_name.find("imu_intrinsic") != std::string::npos)
        {
            return create_motion_profile(stream_id, format, fps, intrinsics_kv);
        }
        else if (msg->topic_name.find("camera_info") != std::string::npos)
        {
            return create_video_stream_profile(stream_id, format, fps, intrinsics_kv);
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

    std::map<uint32_t, stream_profiles> ros2_reader::read_all_stream_profiles(uint32_t device_index)
    {
        auto stream_info_topics_regex = std::regex((rsutils::string::from() << "^/device_" << device_index 
            << "/sensor_\\d+/[^/]+/info$").str()); // get only stream info topics - expecting length to be number of streams
        std::vector<std::string> stream_info_topics = filter_by_regex(_topics_cache, stream_info_topics_regex);

        std::map<uint32_t, stream_profiles> sensor_to_streams;

        for (auto& stream_topic : stream_info_topics)
        {
            stream_identifier stream_id = ros_topic::get_stream_identifier(stream_topic);
            auto stream_profile = read_next_stream_profile();
            if (!stream_profile)
                throw std::runtime_error(rsutils::string::from() << "Failed to read stream profile for topic: " << stream_topic);
            
            sensor_to_streams[stream_id.sensor_index].push_back(stream_profile);
        }

        return sensor_to_streams;
    }

    std::pair<rs2_option, std::shared_ptr<librealsense::option>> ros2_reader::create_option(const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg)
    {
        auto value_topic = msg->topic_name;
        std::string option_name = ros_topic::get_option_name(value_topic);
        device_serializer::sensor_identifier sensor_id = ros_topic::get_sensor_identifier(value_topic);
        rs2_option id;
        std::replace(option_name.begin(), option_name.end(), '_', ' ');
        convert(option_name, id);
        auto message_payload = std::string(reinterpret_cast<const char*>(msg->serialized_data->buffer), msg->serialized_data->buffer_length);
        float value = std::stof(message_payload);
        std::string description = read_option_description(sensor_id.sensor_index, id);
        return std::make_pair(id, std::make_shared<const_value_option>(description, value));
    }

    notification ros2_reader::create_notification(const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg) const
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
            auto value_topic = ros_topic::option_value_topic(sensor_id, id);
            std::string option_name = ros_topic::get_option_name(value_topic);
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

    std::shared_ptr< serialized_data > ros2_reader::read_frame_data(const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg, const stream_identifier& stream_id)
    {
        nanoseconds ts(msg->time_stamp);

        // Parse the actual frame data from msg->serialized_data
        if (!msg->serialized_data || !msg->serialized_data->buffer || msg->serialized_data->buffer_length == 0)
        {
            throw std::runtime_error("Frame data message has no payload");
        }

        // Read metadata from the next message (metadata immediately follows frame data)
        frame_additional_data additional_data{};
        read_frame_metadata(additional_data);
        
        rs2_extension frame_ext = frame_source::stream_to_frame_types(stream_id.stream_type);
        frame_holder frame = m_frame_source->alloc_frame(
            { stream_id.stream_type, stream_id.stream_index, frame_ext },
            msg->serialized_data->buffer_length,
            std::move(additional_data),
            true
        );

        if (frame == nullptr)
        {
            LOG_WARNING("Failed to allocate new frame");
            return nullptr;
        }

        // Copy the raw binary data into the frame's data buffer
        auto frame_data = const_cast<uint8_t*>(frame->get_frame_data());
        std::memcpy(frame_data, msg->serialized_data->buffer, msg->serialized_data->buffer_length);

        if (frame_ext == RS2_EXTENSION_VIDEO_FRAME || frame_ext == RS2_EXTENSION_DEPTH_FRAME)
        {
            setup_video_frame(frame.frame, stream_id);
        }
        else if (frame_ext == RS2_EXTENSION_MOTION_FRAME)
        {
            setup_motion_frame(frame.frame, stream_id);
        }

        auto data = std::make_shared<serialized_frame>(ts, stream_id, std::move(frame));

        // Update cache for fetch_last_frames
        _last_frame_cache[stream_id] = data;

        return data;
    }
        
    frame_holder ros2_reader::allocate_frame(const stream_identifier& sid, const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg, const frame_additional_data& additional_data)
    {
        rs2_extension frame_ext = frame_source::stream_to_frame_types(sid.stream_type);

        // Allocate frame through frame source
        frame_additional_data ad = additional_data;

        return m_frame_source->alloc_frame(
            { sid.stream_type, sid.stream_index, frame_ext },
            msg->serialized_data->buffer_length,
            std::move(ad),
            true
        );
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

        // Read all RS2_FRAME_METADATA values and populate metadata_blob
        uint8_t* blob_ptr = additional_data.metadata_blob.data();
        size_t blob_offset = 0;

        for (int i = 0; i < RS2_FRAME_METADATA_COUNT; i++)
        {
            rs2_frame_metadata_value md_type = static_cast<rs2_frame_metadata_value>(i);
            std::string md_name = librealsense::get_string(md_type);
            
            try
            {
                rs2_metadata_type md_value;
                convert(get_value(kv, md_name), md_value);
                
                // Write the type
                std::memcpy(blob_ptr + blob_offset, &md_type, sizeof(rs2_frame_metadata_value));
                blob_offset += sizeof(rs2_frame_metadata_value);
                
                // Write the value
                std::memcpy(blob_ptr + blob_offset, &md_value, sizeof(rs2_metadata_type));
                blob_offset += sizeof(rs2_metadata_type);
            }
            catch (...) {}
        }

        additional_data.metadata_size = static_cast<uint32_t>(blob_offset);
    }

    void ros2_reader::setup_video_frame(frame_interface* frame_ptr, const stream_identifier& sid) const
    {
        auto video_frame_ptr = static_cast<video_frame*>(frame_ptr);
        
        // Try to get the stream profile from the device snapshot to get width/height
        if (_initialized && !m_initial_device_description.get_sensors_snapshots().empty())
        {
            // Find the matching stream profile
            for (auto& sensor_snap : m_initial_device_description.get_sensors_snapshots())
            {
                for (auto& stream_profile : sensor_snap.get_stream_profiles())
                {
                    if (stream_profile->get_stream_type() == sid.stream_type &&
                        stream_profile->get_stream_index() == sid.stream_index)
                    {
                        // Found matching profile - use it
                        frame_ptr->set_stream(stream_profile);
                        
                        // For video frames, set dimensions
                        if (auto vsp = std::dynamic_pointer_cast<video_stream_profile>(stream_profile))
                        {
                            int width = vsp->get_width();
                            int height = vsp->get_height();
                            int bpp = get_image_bpp(vsp->get_format());
                            int stride = width * bpp / 8;
                            video_frame_ptr->assign(width, height, stride, bpp);
                        }
                        return;
                    }
                }
            }
        }
        
        // Fallback: create a temporary profile if we don't have the snapshot yet
        auto temp_profile = std::make_shared<video_stream_profile>();
        temp_profile->set_stream_type(sid.stream_type);
        temp_profile->set_stream_index(sid.stream_index);
        temp_profile->set_format(RS2_FORMAT_ANY);
        frame_ptr->set_stream(temp_profile);
        
        // Note: Without width/height, the frame won't display properly
        // The playback device should set the correct stream profile later
        LOG_WARNING("Using temporary stream profile without dimensions for " << rs2_stream_to_string(sid.stream_type));
    }

    void ros2_reader::setup_motion_frame(frame_interface* frame_ptr, const stream_identifier& sid) const
    {
        // Try to get the profile from snapshot
        if (_initialized && !m_initial_device_description.get_sensors_snapshots().empty())
        {
            for (auto& sensor_snap : m_initial_device_description.get_sensors_snapshots())
            {
                for (auto& stream_profile : sensor_snap.get_stream_profiles())
                {
                    if (stream_profile->get_stream_type() == sid.stream_type &&
                        stream_profile->get_stream_index() == sid.stream_index)
                    {
                        frame_ptr->set_stream(stream_profile);
                        return;
                    }
                }
            }
        }
        
        // Fallback: create temporary motion profile
        auto temp_profile = std::make_shared<motion_stream_profile>();
        temp_profile->set_stream_type(sid.stream_type);
        temp_profile->set_stream_index(sid.stream_index);
        temp_profile->set_format(RS2_FORMAT_MOTION_XYZ32F);
        frame_ptr->set_stream(temp_profile);
    }


    uint32_t ros2_reader::read_file_version()
    {
        auto msg = read_next_cached();
        std::string payload_str;
        if (msg && msg->serialized_data && msg->serialized_data->buffer_length > 0)
        {
            payload_str = std::string(reinterpret_cast<const char*>(msg->serialized_data->buffer), msg->serialized_data->buffer_length);
        }
        return std::stoi(payload_str);
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
                                                     "/sensor_" << stream_id.sensor_index << "/[^/]+/tf/\\d+$").str();
        auto extrinsic_topics = filter_by_regex(_topics_cache, std::regex(regex_str));
        if (std::find(extrinsic_topics.begin(), extrinsic_topics.end(), msg->topic_name) == extrinsic_topics.end())
        {
            return false;
        }
        msg = read_next_cached();
        group_id = ros_topic::get_extrinsic_group_index(msg->topic_name);
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

    // Helpers ---------------------------------------------------------------------

    bool ros2_reader::is_stream_topic(const std::string& topic, stream_identifier& id) const
    {
        // Parse topics like: /device_0/sensor_0/Depth_0/image/data or /device_0/sensor_0/Accel_0/imu/data
        // Extract device_index, sensor_index, stream_type, and stream_index using ros_topic helper
        
        // Check if it's a frame data topic
        if (topic.find("/image/data") == std::string::npos && 
            topic.find("/imu/data") == std::string::npos &&
            topic.find("/pose/transform/data") == std::string::npos)
        {
            return false;
        }

        try
        {
            // Use ros_topic helper to parse the identifier
            id = ros_topic::get_stream_identifier(topic);
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_WARNING("Failed to parse stream identifier from topic '" << topic << "': " << e.what());
            return false;
        }
    }

    device_snapshot ros2_reader::read_device_description(const nanoseconds& time, bool reset)
    {
        if (_initialized) return m_initial_device_description;

        //reset(); // Ensure we scan from start
        _topics_cache = _storage->get_all_topics_and_types();
        snapshot_collection device_extensions;
        constexpr auto device_index = get_device_index();

        // Read all stream profiles info
        auto sensor_to_streams = read_all_stream_profiles(device_index);

        // Read device info
        auto device_info = read_info_snapshot(ros_topic::device_info_topic(get_device_index()));
        device_extensions[RS2_EXTENSION_INFO] = device_info;

        // Read sensor indices from topics cached - does not call storage read!

        std::vector<sensor_snapshot> sensor_descriptions;
        auto sensor_indices = read_sensor_indices(get_device_index());

        // Read all stream profiles and sensor info
        auto sensors_info = std::map<uint32_t, std::shared_ptr<info_container>>();
        auto sensors_options = std::map<uint32_t, std::shared_ptr<options_container>>();

        for (auto& sensor_index : sensor_indices)
        {
            auto sensor_info_topic = ros_topic::sensor_info_topic({ device_index, sensor_index });
            auto sensor_info = read_info_snapshot(sensor_info_topic);
            sensors_info[sensor_index] = sensor_info;
            auto sensor_options = read_sensor_options({ device_index, sensor_index });
            sensors_options[sensor_index] = sensor_options;
        }

        // Read extrinsics for all streams - written after all sensors info and options
        m_extrinsics_map.clear();

        for (auto& sensor_pair : sensor_to_streams)
        {
            uint32_t sensor_index = sensor_pair.first;
            for (auto& stream_profile : sensor_pair.second)
            {
                auto stream_id = stream_identifier{ get_device_index(), sensor_index, stream_profile->get_stream_type(), static_cast<uint32_t>(stream_profile->get_stream_index())};
                uint32_t reference_id;
                rs2_extrinsics stream_extrinsic;
                if (try_read_stream_extrinsic(stream_id, reference_id, stream_extrinsic))
                {
                    m_extrinsics_map[stream_id] = std::make_pair(reference_id, stream_extrinsic);
                }
            }
        }

        // Build sensor descriptions from info and streams
        for (auto sensor_index : sensor_indices)
        {
            snapshot_collection sensor_extensions;
            sensor_extensions[RS2_EXTENSION_INFO] = sensors_info[sensor_index];

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
        return m_initial_device_description;
    }

    std::shared_ptr<info_container> ros2_reader::read_info_snapshot(const std::string& topic)
    {
        // Read all messages on the topic and populate infos
        auto infos = std::make_shared<info_container>();

        auto msg = peek_next_cached();
        while (msg && msg->topic_name == topic)
        {
            msg = read_next_cached();
            auto kv = parse_msg_payload(msg);
            register_camera_infos(infos, kv);
            msg = peek_next_cached();
        }

        return infos;
    }

    std::set<uint32_t> ros2_reader::read_sensor_indices(uint32_t device_index) const
    {
        std::regex regex((rsutils::string::from() << "^/device_" << device_index
            << "/sensor_(\\d+)/info$").str());
        auto stream_info_topics = filter_by_regex(_topics_cache, regex);

        std::set<uint32_t> sensor_indices;
        for (const auto& topic : stream_info_topics) {
            sensor_indices.insert(ros_topic::get_sensor_index(topic));
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
        if (m_read_options_descriptions[sensor_index].find(id) == m_read_options_descriptions[sensor_index].end())
        {
            auto msg = read_next_cached();
            auto description = std::string(reinterpret_cast<const char*>(msg->serialized_data->buffer), msg->serialized_data->buffer_length);
            m_read_options_descriptions[sensor_index][id] = description;
        }
        return m_read_options_descriptions[sensor_index][id];
    }

    bool ros2_reader::is_option_topic(const std::string& topic, sensor_identifier& sid, rs2_option& opt) const
    {
        // Parse topics like: /device_0/sensor_0/option/Depth_Units/value
        if (topic.find("/option/") == std::string::npos || topic.find("/value") == std::string::npos)
        {
            return false;
        }

        try
        {
            // Extract sensor identifier
            sid = ros_topic::get_sensor_identifier(topic);
            
            // Extract option name
            std::string option_name = ros_topic::get_option_name(topic);
            
            // Convert option name to rs2_option enum
            if (!convert(option_name, opt))
            {
                LOG_WARNING("Failed to convert option name: " << option_name);
                return false;
            }
            
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_WARNING("Failed to parse option topic '" << topic << "': " << e.what());
            return false;
        }
    }


    bool ros2_reader::has_next_cached() const
    {
        // If we have a valid cached message, we have next
        if (_cache_valid)
            return true;

        // Otherwise, check storage
        return _storage->has_next();
    }

    std::shared_ptr<rosbag2_storage::SerializedBagMessage> ros2_reader::read_next_cached()
    {
        // If cache is valid, return cached message and mark as consumed
        if (_cache_valid)
        {
            _cache_valid = false;
            return _cached_message;
        }

        // Otherwise, read from storage and return immediately (no caching)
        if (!_storage->has_next())
            return nullptr;

        auto next_msg = _storage->read_next();

        // Update last frame cache if this is a stream topic
        update_last_frame_cache(next_msg);

        return next_msg;
    }

    std::shared_ptr<rosbag2_storage::SerializedBagMessage> ros2_reader::peek_next_cached()
    {
        // If cache is valid, return cached message without consuming
        if (_cache_valid)
            return _cached_message;

        // Otherwise, read from storage and cache it
        if (!_storage->has_next())
            return nullptr;

        _cached_message = _storage->read_next();
        _cache_valid = true;
        update_last_frame_cache(_cached_message);
        return _cached_message;
    }

    void ros2_reader::update_last_frame_cache(std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg)
    {
        if (!msg)
            return;

        std::string topic = msg->topic_name;
        auto msg_ts = nanoseconds(msg->time_stamp);
        stream_identifier stream_id;

        if (is_stream_topic(topic, stream_id))
        {
            // We have to wrap this into serialized_data to cache it
            // Note: Full frame parsing would be needed here for a complete implementation
            // For now, we use serialized_invalid_frame as a placeholder
            auto data = std::make_shared<serialized_invalid_frame>(msg_ts, stream_id);
            _last_frame_cache[stream_id] = data;
        }
    }

}