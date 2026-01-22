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

namespace librealsense {
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


    ros2_reader::ros2_reader(const std::string& file_path, const std::shared_ptr<context>& ctx)
        : _file_path(file_path + ".db3")
        , m_context(ctx)
    {
        _storage = std::make_shared< rosbag2_storage_plugins::SqliteStorage >();
        _storage->open(_file_path, rosbag2_storage::storage_interfaces::IOFlag::READ_ONLY);

        if (!_storage)
            throw std::runtime_error(rsutils::string::from() << "Failed to open rosbag2 storage for uri '" << _file_path << "'");

        _topics_cache = _storage->get_all_topics_and_types();
    }

    void ros2_reader::reset()
    {
        // Sqlite storage reset usually involves re-opening to clear cursor state
        _storage = std::make_shared< rosbag2_storage_plugins::SqliteStorage >();
        _storage->open(_file_path, rosbag2_storage::storage_interfaces::IOFlag::READ_ONLY);
        _last_frame_cache.clear();
    }

    void ros2_reader::seek_to_time(const nanoseconds& time)
    {
        // TODO: Rework this function
        // rosbag2 storage interfaces typically read sequentially.
        // To seek accurately in a raw storage plugin without the higher-level Reader API:
        // 1. Reset file.
        // 2. Read forward until timestamp >= time.
        // 3. While reading forward, we MUST update _last_frame_cache so fetch_last_frames works.

        reset();

        while (_storage->has_next())
        {
            auto next_msg = _storage->read_next();
            if (!next_msg) break;

            nanoseconds msg_ts(next_msg->time_stamp);

            // Update cache if it's a frame
            std::string topic = next_msg->topic_name;
            stream_identifier stream_id;
            if (is_stream_topic(topic, stream_id))
            {
                // We have to wrap this into serialized_data to cache it
                // Note: Full frame parsing would be needed here for a complete implementation
                // For now, we use serialized_invalid_frame as a placeholder
                auto data = std::make_shared<serialized_invalid_frame>(msg_ts, stream_id);
                _last_frame_cache[stream_id] = data;
            }

            if (msg_ts >= time)
            {
                break;
            }
        }
    }

    nanoseconds ros2_reader::query_duration() const
    {
        auto meta = _storage->get_metadata();
        return nanoseconds(meta.duration.count());
    }

    void ros2_reader::enable_stream(const std::vector<stream_identifier>& stream_ids)
    {
        for (const auto& id : stream_ids) _enabled_streams.insert(id);
    }

    void ros2_reader::disable_stream(const std::vector<stream_identifier>& stream_ids)
    {
        for (const auto& id : stream_ids) _enabled_streams.erase(id);
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
                std::cerr << e.what() << std::endl;
            }
        }
    }

    device_snapshot ros2_reader::query_device_description(const nanoseconds& time)
    {
        if (_initialized) return _initial_snapshot;

        reset(); // Ensure we scan from start
        snapshot_collection device_extensions;
        auto device_index = get_device_index();

        // 1. Read device Info
        auto device_info = read_info_snapshot(ros_topic::device_info_topic(device_index));
        device_extensions[RS2_EXTENSION_INFO] = device_info;

        // TODO: Need to fix write order, so that device info is written first, before streams, and so we can get rid of the reset here
        reset();

        // 2. Read sensor indices from topics cached - does not call storage read!
        auto sensor_indices = read_sensor_indices(device_index);

        // 3. Read all stream profiles and sensor info
        auto sensor_to_streams = read_all_stream_profiles(device_index);
        auto sensors_info = read_all_sensor_info();

        // 4. Build sensor descriptions
        std::vector<sensor_snapshot> sensor_descriptions;
        for (auto sensor_index : sensor_indices)
        {
            snapshot_collection sensor_extensions;
            sensor_extensions[RS2_EXTENSION_INFO] = sensors_info[sensor_index];
            auto& sensor_streams = sensor_to_streams[sensor_index];
            sensor_descriptions.emplace_back(sensor_index, sensor_extensions, sensor_streams);
        }

        _initial_snapshot = device_snapshot(device_extensions, sensor_descriptions, {});
        _initialized = true;
        reset();
        return _initial_snapshot;
    }

    std::shared_ptr<info_container> ros2_reader::read_info_snapshot(const std::string& topic) const
    {
        auto infos = std::make_shared<info_container>();
        // Read all messages on the topic and populate infos
        _storage->reset_filter();
        _storage->set_filter(rosbag2_storage::StorageFilter{ {topic} });

        while (_storage->has_next())
        {
            auto msg = _storage->read_next();
            auto kv = parse_msg_payload(msg);
            register_camera_infos(infos, kv);
        }
        _storage->reset_filter();
        return infos;
    }

    std::shared_ptr<stream_profile_interface> ros2_reader::read_next_stream_profile()
    {
        auto msg = _storage->read_next();
        if (!msg)
            return nullptr;

        auto kv = parse_msg_payload(msg);
        auto encoding = get_value(kv, "encoding");
        auto fps = static_cast<uint32_t>(std::stoul(get_value(kv, "fps")));
        //auto is_recommended = (kv.find("is_recommended")->second == "true");

        rs2_format format;
        convert(encoding, format);

        stream_identifier stream_id = ros_topic::get_stream_identifier(msg->topic_name);

        msg = _storage->read_next();
        if (!msg)
            return nullptr;
        
        auto intrinsics_kv = parse_msg_payload(msg);

        if (msg->topic_name.find("imu_intrinsic") != std::string::npos)
        {
            return create_motion_profile(stream_id, format, fps, intrinsics_kv);
        }
        else if (msg->topic_name.find("camera_info") != std::string::npos)
        {
            return create_video_profile(stream_id, format, fps, intrinsics_kv);
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

    rs2_intrinsics ros2_reader::parse_video_intrinsics(const std::map<std::string, std::string>& kv) const
    {
        rs2_intrinsics intrinsics{};

        intrinsics.width = static_cast<int>(std::stoul(get_value(kv, "width")));
        intrinsics.height = static_cast<int>(std::stoul(get_value(kv, "height")));
        intrinsics.fx = std::stof(get_value(kv, "fx"));
        intrinsics.ppx = std::stof(get_value(kv, "ppx"));
        intrinsics.fy = std::stof(get_value(kv, "fy"));
        intrinsics.ppy = std::stof(get_value(kv, "ppy"));

        auto dist_model_str = get_value(kv, "model");
        rs2_distortion dist_model;
        convert(dist_model_str, dist_model);
        intrinsics.model = dist_model;

        auto coeffs_str = get_value(kv, "coeffs");
        auto coeffs_tokens = split_string(coeffs_str, ',');
        for (size_t i = 0; i < std::min(coeffs_tokens.size(), size_t(5)); ++i)
        {
            intrinsics.coeffs[i] = std::stof(coeffs_tokens[i]);
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

    std::shared_ptr<video_stream_profile> ros2_reader::create_video_profile(const stream_identifier& stream_id, rs2_format format,
        uint32_t fps, const std::map<std::string, std::string>& intrinsics_kv) const
    {
        auto video_profile = std::make_shared<video_stream_profile>();
        video_profile->set_stream_index(stream_id.stream_index);
        video_profile->set_stream_type(stream_id.stream_type);
        video_profile->set_format(format);
        video_profile->set_framerate(fps);

        auto intrinsics = parse_video_intrinsics(intrinsics_kv);
        uint32_t width = static_cast<uint32_t>(intrinsics.width);
        uint32_t height = static_cast<uint32_t>(intrinsics.height);

        video_profile->set_dims(width, height);
        video_profile->set_intrinsics([intrinsics]() { return intrinsics; });

        return video_profile;
    }

    std::set<uint32_t> ros2_reader::read_sensor_indices(uint32_t device_index)
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

    std::map<uint32_t, stream_profiles> ros2_reader::read_all_stream_profiles(uint32_t device_index)
    {
        auto sensor_info_topics_regex = std::regex((rsutils::string::from() << "^/device_" << device_index 
            << "/sensor_\\d+.*/(info|imu_intrinsic|camera_info)$").str()); // get both sensor and stream info topics
        std::vector<std::string> sensor_info_topics = filter_by_regex(_topics_cache, sensor_info_topics_regex);

        _storage->reset_filter();
        _storage->set_filter(rosbag2_storage::StorageFilter{ sensor_info_topics });

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

    std::map<uint32_t, std::shared_ptr<info_container>> ros2_reader::read_all_sensor_info()
    {
        std::map<uint32_t, std::shared_ptr<info_container>> sensors_info;

        while (_storage->has_next())
        {
            auto msg = _storage->read_next();
            auto kv = parse_msg_payload(msg);
            uint32_t sensor_index = ros_topic::get_sensor_index(msg->topic_name);
            auto& sensor_info = sensors_info[sensor_index];
            if (!sensor_info)
                sensor_info = std::make_shared<info_container>();
            register_camera_infos(sensor_info, kv);
        }

        return sensors_info;
    }

    std::shared_ptr< serialized_data > ros2_reader::read_next_data()
    {
        if (!_storage->has_next())
        {
            return std::make_shared<serialized_end_of_file>();
        }

        while (_storage->has_next())
        {
            auto msg = _storage->read_next();

            std::string topic = msg->topic_name;
            nanoseconds ts(msg->time_stamp);

            // 1. Check if this is a frame data topic (e.g., /device_0/sensor_0/Depth_0/image/data)
            stream_identifier sid;
            if (is_stream_topic(topic, sid))
            {
                // Filter: if we have enabled streams and this isn't one, skip it
                if (!_enabled_streams.empty() && _enabled_streams.find(sid) == _enabled_streams.end())
                {
                    continue;
                }
                return read_frame_data(msg, sid);
            }

            // 2. Options (e.g., /device_0/sensor_0/option/...)
            sensor_identifier sensor_id;
            rs2_option opt;
            if (false && is_option_topic(topic, sensor_id, opt))
            {
                // TODO: disabled for now
                // Parse option value
                if (msg->serialized_data && msg->serialized_data->buffer_length > 0)
                {
                    std::string payload(reinterpret_cast<const char*>(msg->serialized_data->buffer), msg->serialized_data->buffer_length);
                    try
                    {
                        float val = std::stof(payload);
                        auto option = std::make_shared<const_value_option>("Recorded option", val);
                        return std::make_shared<serialized_option>(ts, sensor_id, opt, option);
                    }
                    catch (const std::exception& e)
                    {
                        LOG_WARNING("Failed to parse option value: " << e.what());
                    }
                }
                continue;
            }

            // 3. Notifications
            if (topic.find("/notification/") != std::string::npos)
            {
                // TODO: Implement notification parsing
                continue;
            }
        }
        return std::make_shared<serialized_end_of_file>();
    }

    std::shared_ptr< serialized_data > ros2_reader::read_frame_data(const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg, const stream_identifier& sid)
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

        frame_holder frame = allocate_frame(sid, msg, additional_data);

        if (!frame)
        {
            throw std::runtime_error("Failed to allocate frame");
        }

        // Copy the raw binary data into the frame's data buffer
        auto frame_data = const_cast<uint8_t*>(frame->get_frame_data());
        std::memcpy(frame_data, msg->serialized_data->buffer, msg->serialized_data->buffer_length);

        rs2_extension frame_ext = frame_source::stream_to_frame_types(sid.stream_type);
        if (frame_ext == RS2_EXTENSION_VIDEO_FRAME || frame_ext == RS2_EXTENSION_DEPTH_FRAME)
        {
            setup_video_frame(frame.frame, sid);
        }
        else if (frame_ext == RS2_EXTENSION_MOTION_FRAME)
        {
            setup_motion_frame(frame.frame, sid);
        }

        auto data = std::make_shared<serialized_frame>(ts, sid, std::move(frame));

        // Update cache for fetch_last_frames
        _last_frame_cache[sid] = data;

        return data;
    }
        
    frame_holder ros2_reader::allocate_frame(const stream_identifier& sid, const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg, const frame_additional_data& additional_data)
    {
        rs2_extension frame_ext = frame_source::stream_to_frame_types(sid.stream_type);

        // Initialize frame source if needed
        if (!_frame_source)
        {
            _frame_source = std::make_shared<frame_source>(32);
            _frame_source->init(md_constant_parser::create_metadata_parser_map());
        }

        // Allocate frame through frame source
        frame_additional_data ad = additional_data;

        return _frame_source->alloc_frame(
            { sid.stream_type, sid.stream_index, frame_ext },
            msg->serialized_data->buffer_length,
            std::move(ad),
            true
        );
    }

    void ros2_reader::read_frame_metadata(frame_additional_data& additional_data)
    {
        // Read the next message which should be the metadata for this frame
        if (!_storage->has_next())
            return;

        auto md_msg = _storage->read_next();
        if (!md_msg || md_msg->topic_name.find("/metadata") == std::string::npos)
            return;

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
        if (_initialized && !_initial_snapshot.get_sensors_snapshots().empty())
        {
            // Find the matching stream profile
            for (auto& sensor_snap : _initial_snapshot.get_sensors_snapshots())
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
        if (_initialized && !_initial_snapshot.get_sensors_snapshots().empty())
        {
            for (auto& sensor_snap : _initial_snapshot.get_sensors_snapshots())
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

    const std::string& ros2_reader::get_file_name() const { return _file_path; }

} // namespace librealsense