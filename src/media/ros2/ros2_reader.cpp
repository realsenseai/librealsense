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

    ros2_reader::ros2_reader(const std::string& file_path, const std::shared_ptr<context>& ctx)
        : _file_path(file_path + ".db3")
        , _context(ctx)
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

    device_snapshot ros2_reader::query_device_description(const nanoseconds& time)
    {
        if (_initialized) return _initial_snapshot;

        reset(); // Ensure we scan from start
        device_snapshot dev_snap;
        std::map< uint32_t, sensor_snapshot > sensors;
        snapshot_collection device_extensions;
        uint32_t device_index = get_device_index();

        // 1. Read device Info
        auto info = read_info_snapshot(ros_topic::device_info_topic(device_index));
        device_extensions[RS2_EXTENSION_INFO] = info;

        // between reads, reset storage to read from start
        reset();

        // 2. Read sensor indices from topics cached - does not call storage read!
        auto sensor_indices = read_sensor_indices(device_index);

        // TODO: Move this to a helper function
        std::vector<sensor_snapshot> sensor_descriptions;
        // Read each sensor info topic
        for (auto sensor_index : sensor_indices)
        {
            snapshot_collection sensor_extensions;
            auto sensor_info = std::make_shared<info_container>();
            auto sensor_info_topic = ros_topic::sensor_info_topic({ device_index, sensor_index });
            _storage->reset_filter();
            _storage->set_filter(rosbag2_storage::StorageFilter{ {sensor_info_topic} });

            while (_storage->has_next())
            {
                auto msg = _storage->read_next();
                std::string topic = msg->topic_name;
                std::string payload_str;

                if (msg->serialized_data && msg->serialized_data->buffer_length > 0)
                {
                    payload_str = std::string(reinterpret_cast<const char*>(msg->serialized_data->buffer), msg->serialized_data->buffer_length);
                }
                auto kv = parse_key_value_string(payload_str);
                try
                {
                    rs2_camera_info info;
                    auto it = kv.begin();
                    std::string key = it->first;
                    std::string value = it->second;
                    convert(key, info);
                    sensor_info->register_info(info, value);
                }
                catch (const std::exception& e)
                {
                    std::cerr << e.what() << std::endl;
                }
            }


            sensor_extensions[RS2_EXTENSION_INFO] = sensor_info;
            // TODO: Read sensor stream infos and sensor options as well, and see add_sensor_extension on previous code
            auto streams_snapshots = read_stream_info(device_index, sensor_index);
            sensor_descriptions.emplace_back(sensor_index, sensor_extensions, streams_snapshots);

            reset();
        }

        _storage->reset_filter();


        //while (_storage->has_next())
        //{
        //    auto msg = _storage->read_next();
        //    std::string topic = msg->topic_name;
        //    std::string payload_str;

        //    if (msg->serialized_data && msg->serialized_data->buffer_length > 0)
        //    {
        //        payload_str = std::string(reinterpret_cast<const char*>(msg->serialized_data->buffer), msg->serialized_data->buffer_length);
        //    }

        //    if (topic.find("librealsense/file_version") != std::string::npos) continue;

        //    // 1. Device Info (Simple heuristic based on topic name)
        //    if (topic.find("info") != std::string::npos && topic.find("sensor") == std::string::npos && topic.find("stream") == std::string::npos)
        //    {
        //        auto kv = parse_key_value_string(payload_str);
        //        // In a real implementation: populate info_interface from KV
        //    }

        //    // 2. Stream Info
        //    stream_identifier stream_id;
        //    if (is_stream_topic(topic, stream_id) && topic.find("info") != std::string::npos && topic.find("video") == std::string::npos)
        //    {
        //        // Logic to add stream profile to sensor snapshot
        //    }
        //}

        //for (auto& s : sensors) dev_snap.get_sensors_snapshots().push_back(s.second);

        //_initial_snapshot = dev_snap;
        //_initialized = true;

        //reset(); // Reset for playback

        _initial_snapshot = device_snapshot(device_extensions, sensor_descriptions, {});
        return _initial_snapshot;
    }

    std::shared_ptr<info_container> ros2_reader::read_info_snapshot(const std::string& topic) const
    {
        // temp return nothing
        //return std::make_shared<info_container>();
        auto infos = std::make_shared<info_container>();
        std::map<rs2_camera_info, std::string> values;
        // Read all messages on the topic and populate infos
        _storage->reset_filter();
        _storage->set_filter(rosbag2_storage::StorageFilter{ {topic} });

        while (_storage->has_next())
        {
            auto msg = _storage->read_next();
            std::string topic = msg->topic_name;
            std::string payload_str;

            if (msg->serialized_data && msg->serialized_data->buffer_length > 0)
            {
                payload_str = std::string(reinterpret_cast<const char*>(msg->serialized_data->buffer), msg->serialized_data->buffer_length);
            }
            auto kv = parse_key_value_string(payload_str);
            try
            {
                rs2_camera_info info;
                auto it = kv.begin();
                std::string key = it->first;
                std::string value = it->second;
                convert(key, info);
                infos->register_info(info, value);
            }
            catch (const std::exception& e)
            {
                std::cerr << e.what() << std::endl;
            }
        }
        _storage->reset_filter();
        return infos;
    }

    stream_profiles ros2_reader::read_stream_info(uint32_t device_index, uint32_t sensor_index)
    {
        stream_profiles streams;
        auto dummy_stream = std::make_shared<video_stream_profile>();
        dummy_stream->set_stream_index(1);
        dummy_stream->set_stream_type(RS2_STREAM_DEPTH);
        dummy_stream->set_format(RS2_FORMAT_Z16);
        dummy_stream->set_framerate(30);
        dummy_stream->set_dims(640, 480);

        //streams.push_back(dummy_stream); // Placeholder to avoid empty stream list
        //return streams;


        //The below regex matches both stream info messages and also video \ imu stream info (both have the same prefix)
        std::string regex_const_str = R"RRR(/([a-zA-Z0-9_ ])+_(\d)+/info)RRR";
        std::string topic_query = "/device_" + std::to_string(device_index) + "/sensor_" + std::to_string(sensor_index) + regex_const_str;
        _storage->reset_filter();
        _storage->set_filter(rosbag2_storage::StorageFilter{ {topic_query} });
        while (_storage->has_next())
        {
            auto msg = _storage->read_next();
            if (!msg) continue;
            // Placeholder for actual message deserialization
            // if (infos_view.isType<realsense_msgs::StreamInfo>() == false)
            // {
            //     continue;
            // }
            stream_identifier stream_id = {}; // ros_topic::get_stream_identifier(msg->topic_name);
            // Placeholder: create a dummy video stream profile
            auto profile = std::make_shared<video_stream_profile>();
            streams.push_back(profile);
        }
        _storage->reset_filter();
        return streams;

    }

    std::set<uint32_t> ros2_reader::read_sensor_indices(uint32_t device_index)
    {
        std::set<uint32_t> sensor_indices;

        for (const auto& topic : _topics_cache)
        {
            // expecting topic name to be like device_0/sensor_X/info
            std::string expected_prefix = "/device_" + std::to_string(device_index) + "/sensor_";
            std::string expected_suffix = "/info";
            if (topic.name.find(expected_prefix) == 0 && topic.name.find(expected_suffix) != std::string::npos)
            {
                // Extract sensor index from topic name
                uint32_t sensor_index = ros_topic::get_sensor_index(topic.name);
                sensor_indices.insert(sensor_index);
            }
        }

        /*for (auto sensor_index : sensor_indices)
        {
            snapshot_collection sensor_extensions;
            auto sensor_info = std::make_shared<info_container>();
            auto sensor_info_topic = ros_topic::sensor_info_topic({ device_index, sensor_index });
            _storage->reset_filter();
            _storage->set_filter(rosbag2_storage::StorageFilter{ {sensor_info_topic} });

            while (_storage->has_next())
            {
                auto msg = _storage->read_next();
                std::string topic = msg->topic_name;
                std::string payload_str;

                if (msg->serialized_data && msg->serialized_data->buffer_length > 0)
                {
                    payload_str = std::string(reinterpret_cast<const char*>(msg->serialized_data->buffer), msg->serialized_data->buffer_length);
                }
                auto kv = parse_key_value_string(payload_str);
                try
                {
                    rs2_camera_info info;
                    auto it = kv.begin();
                    std::string key = it->first;
                    std::string value = it->second;
                    convert(key, info);
                    sensor_info->register_info(info, value);
                    sensor_extensions[RS2_EXTENSION_INFO] = sensor_info;
                }
                catch (const std::exception& e)
                {
                    std::cerr << e.what() << std::endl;
                }
            }

            reset();
        }*/

        _storage->reset_filter();


        return sensor_indices;
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
            if (!msg) return std::make_shared<serialized_end_of_file>();

            std::string topic = msg->topic_name;
            nanoseconds ts(msg->time_stamp);

            // 1. Check if this is a frame data topic (e.g., /device_0/sensor_0/Depth_0/image/data)
            stream_identifier sid;
            if (topic.find("/image/data") != std::string::npos || topic.find("/imu/data") != std::string::npos)
            {
                if (!is_stream_topic(topic, sid))
                {
                    LOG_WARNING("Failed to parse stream topic: " << topic);
                    continue;
                }

                // Filter: if we have enabled streams and this isn't one, skip it
                if (!_enabled_streams.empty() && _enabled_streams.find(sid) == _enabled_streams.end())
                {
                    continue;
                }

                // TODO: Parse the actual frame data from msg->serialized_data
                // For now, return a placeholder invalid frame
                auto data = std::make_shared<serialized_invalid_frame>(ts, sid);

                // Update cache for fetch_last_frames
                _last_frame_cache[sid] = data;

                return data;
            }

            // 2. Options (e.g., /device_0/sensor_0/option/...)
            sensor_identifier sensor_id;
            rs2_option opt;
            if (is_option_topic(topic, sensor_id, opt))
            {
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