// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once
#include <string>
#include <chrono>
#include "librealsense2/rs.h"
#include "sensor_msgs/image_encodings.h"
#include "ros2-msg-types/sensor_msgs/msg/Imu.h"
#include "ros2-msg-types/sensor_msgs/msg/Image.h"
#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>
#include "metadata-parser.h"
#include "option.h"
#include "core/serialization.h"
#include <regex>
#include "stream.h"

#include <rsutils/string/from.h>


enum ros_file_versions
{
    ROS_FILE_VERSION_2 = 2u,
    ROS_FILE_VERSION_3 = 3u,
    ROS_FILE_WITH_RECOMMENDED_PROCESSING_BLOCKS = 4u
};


namespace librealsense
{
    struct stream_descriptor
    {
        stream_descriptor() : type( RS2_STREAM_ANY ), index( 0 ) {}
        stream_descriptor( rs2_stream type, int index = 0 ) : type( type ), index( index ) {}

        rs2_stream type;
        int index;
    };

    inline void convert(rs2_format source, std::string& target)
    {
        switch (source)
        {
        case RS2_FORMAT_Z16: target = sensor_msgs::image_encodings::MONO16;     break;
        case RS2_FORMAT_RGB8: target = sensor_msgs::image_encodings::RGB8;      break;
        case RS2_FORMAT_BGR8: target = sensor_msgs::image_encodings::BGR8;      break;
        case RS2_FORMAT_RGBA8: target = sensor_msgs::image_encodings::RGBA8;    break;
        case RS2_FORMAT_BGRA8: target = sensor_msgs::image_encodings::BGRA8;    break;
        case RS2_FORMAT_Y8: target = sensor_msgs::image_encodings::TYPE_8UC1;   break;
        case RS2_FORMAT_Y16: target = sensor_msgs::image_encodings::TYPE_16UC1; break;
        case RS2_FORMAT_RAW8: target = sensor_msgs::image_encodings::MONO8;     break;
        case RS2_FORMAT_UYVY: target = sensor_msgs::image_encodings::YUV422;    break;
        default: target = rs2_format_to_string(source);
        }
    }

    template <typename T>
    inline bool convert(const std::string& source, T& target)
    {
        if (!try_parse(source, target))
        {
            LOG_INFO("Failed to convert source: " << source << " to matching " << typeid(T).name());
            return false;
        }
        return true;
    }

    // Specialized methods for selected types
    template <>
    inline bool convert(const std::string& source, rs2_format& target)
    {
        bool ret = true;
        std::string source_alias("");
        bool mapped_format = false;
        if (source == sensor_msgs::image_encodings::MONO16) {
            target = RS2_FORMAT_Z16;
            mapped_format = true;
        }
        if (source == sensor_msgs::image_encodings::TYPE_8UC1) {
            target = RS2_FORMAT_Y8;
            mapped_format = true;
        }
        if (source == sensor_msgs::image_encodings::TYPE_16UC1) {
            target = RS2_FORMAT_Y16;
            mapped_format = true;
        }
        if (source == sensor_msgs::image_encodings::MONO8) {
            target = RS2_FORMAT_RAW8;
            mapped_format = true;
        }
        if (source == sensor_msgs::image_encodings::YUV422) {
            target = RS2_FORMAT_UYVY;
            mapped_format = true;
        }
        if (source == sensor_msgs::image_encodings::RGB8)       target = RS2_FORMAT_RGB8;
        if (source == sensor_msgs::image_encodings::BGR8)       target = RS2_FORMAT_BGR8;
        if (source == sensor_msgs::image_encodings::RGBA8)      target = RS2_FORMAT_RGBA8;
        if (source == sensor_msgs::image_encodings::BGRA8)      target = RS2_FORMAT_BGRA8;
        
        // formats that need to be mapped to sdk native formats (e.g. MONO16)
        if (mapped_format)
            source_alias = std::string(rs2_format_to_string(target));
        else {
            // formats that are same as the sdk native formats (e.g.rgb8), 
            // these need to be changed to upper case
            // because values in sensor_msgs::image_encodings are lower case
            source_alias = source;
            std::transform(source_alias.begin(), source_alias.end(), source_alias.begin(), ::toupper);
        }
        
        if (!(ret = try_parse(source_alias, target)))
        {
            LOG_INFO("Failed to convert source: " << source << " to matching rs2_format");
        }
        return ret;
    }

    template <>
    inline bool convert(const std::string& source, double& target)
    {
        target = std::stod(source);
        return std::isfinite(target);
    }

    template <>
    inline bool convert(const std::string& source, long long& target)
    {
        target = std::stoll(source);
        return true;
    }

    constexpr const char* FRAME_NUMBER_MD_STR = "Frame number";
    constexpr const char* TIMESTAMP_DOMAIN_MD_STR = "timestamp_domain";
    constexpr const char* SYSTEM_TIME_MD_STR = "system_time";
    constexpr const char* MAPPER_CONFIDENCE_MD_STR = "Mapper Confidence";
    constexpr const char* FRAME_TIMESTAMP_MD_STR = "frame_timestamp";
    constexpr const char* TRACKER_CONFIDENCE_MD_STR = "Tracker Confidence";
    constexpr const char* TIMESTAMP_MD_STR = "timestamp";

    class ros2_topic
    {
    public:
        static constexpr const char* elements_separator() { return "/"; }
        static constexpr const char* ros_image_type_str() { return "image"; }
        static constexpr const char* ros_imu_type_str() { return "imu"; }
        static constexpr const char* ros_pose_type_str() { return "pose"; }
        static constexpr const char* ros_safety_type_str() { return "safety"; }
        static constexpr const char* ros_occupancy_type_str() { return "occupancy"; }
        static constexpr const char* ros_labeled_points_type_str() { return "labeled_points"; }

        static uint32_t get_device_index(const std::string& topic)
        {
            return get_id("device_", get<1>(topic));
        }

        static uint32_t get_sensor_index(const std::string& topic)
        {
            return get_id("sensor_", get<2>(topic));
        }

        static rs2_stream get_stream_type(const std::string& topic)
        {
            auto stream_with_id = get<3>(topic);
            auto pos = stream_with_id.find_last_of('_');
            auto stream_name = stream_with_id.substr(0, pos);
            std::replace(stream_name.begin(), stream_name.end(), '_', ' ');
            rs2_stream type;
            convert(stream_name, type);
            return type;
        }

        static uint32_t get_stream_index(const std::string& topic)
        {
            auto stream_with_id = get<3>(topic);
            auto pos = stream_with_id.find_last_of('_');
            return static_cast<uint32_t>(std::stoul(stream_with_id.substr(pos + 1)));
        }

        static device_serializer::sensor_identifier get_sensor_identifier(const std::string& topic)
        {
            return device_serializer::sensor_identifier{ get_device_index(topic),  get_sensor_index(topic) };
        }

        static device_serializer::stream_identifier get_stream_identifier(const std::string& topic)
        {
            return device_serializer::stream_identifier{ get_device_index(topic),  get_sensor_index(topic),  get_stream_type(topic),  get_stream_index(topic) };
        }

        static uint32_t get_extrinsic_group_index(const std::string& topic)
        {
            // ROS2 extrinsic topics use .../tf/ref_N format
            const std::string prefix = "ref_";
            auto pos = topic.rfind(prefix);
            if (pos == std::string::npos)
                throw std::runtime_error("Invalid extrinsic topic: " + topic);
            return std::stoul(topic.substr(pos + prefix.size()));
        }

        static std::string get_option_name(const std::string& topic)
        {
            return get<4>(topic);
        }
        static std::string file_version_topic()
        {
            return create_from({ "file_version" });
        }
        static std::string device_info_topic(uint32_t device_id)
        {
            return create_from({ device_prefix(device_id),  "info" });
        }
        static std::string sensor_info_topic(const device_serializer::sensor_identifier& sensor_id)
        {
            return create_from({ device_prefix(sensor_id.device_index), sensor_prefix(sensor_id.sensor_index),  "info" });
        }
        static std::string stream_info_topic(const device_serializer::stream_identifier& stream_id)
        {
            return create_from({ stream_full_prefix(stream_id), "info" });
        }
        static std::string video_stream_info_topic(const device_serializer::stream_identifier& stream_id)
        {
            return create_from({ stream_full_prefix(stream_id), "camera_info" });
        }
        static std::string imu_intrinsic_topic(const device_serializer::stream_identifier& stream_id)
        {
            return create_from({ stream_full_prefix(stream_id), "imu_intrinsic" });
        }

        /*version 2 and down*/
        static std::string property_topic(const device_serializer::sensor_identifier& sensor_id)
        {
            return create_from({ device_prefix(sensor_id.device_index), sensor_prefix(sensor_id.sensor_index), "property" });
        }

        /*version 3 and up*/
        static std::string option_value_topic(const device_serializer::sensor_identifier& sensor_id, rs2_option option_type)
        {
            std::string topic_name = rs2_option_to_string(option_type);
            std::replace(topic_name.begin(), topic_name.end(), ' ', '_');
            return create_from({ device_prefix(sensor_id.device_index), sensor_prefix(sensor_id.sensor_index), "option", topic_name, "value" });
        }

        static std::string post_processing_blocks_topic(const device_serializer::sensor_identifier& sensor_id)
        {
            return create_from({ device_prefix(sensor_id.device_index), sensor_prefix(sensor_id.sensor_index), "post_processing" });
        }

        /*version 3 and up*/
        static std::string option_description_topic(const device_serializer::sensor_identifier& sensor_id, rs2_option option_type)
        {
            std::string topic_name = rs2_option_to_string(option_type);
            std::replace(topic_name.begin(), topic_name.end(), ' ', '_');
            return create_from({ device_prefix(sensor_id.device_index), sensor_prefix(sensor_id.sensor_index), "option", topic_name, "description" });
        }

        static std::string frame_data_topic(const device_serializer::stream_identifier& stream_id)
        {
            return create_from({ stream_full_prefix(stream_id), stream_to_ros_type(stream_id.stream_type), "data" });
        }

        static std::string frame_metadata_topic(const device_serializer::stream_identifier& stream_id)
        {
            return create_from({ stream_full_prefix(stream_id), stream_to_ros_type(stream_id.stream_type), "metadata" });
        }

        static std::string stream_extrinsic_topic(const device_serializer::stream_identifier& stream_id, uint32_t ref_id)
        {
            return create_from({ stream_full_prefix(stream_id), "tf", "ref_" + std::to_string(ref_id) });
        }

        static std::string  additional_info_topic()
        {
            return create_from({ "additional_info" });
        }

        static std::string stream_full_prefix(const device_serializer::stream_identifier& stream_id)
        {
            return create_from({ device_prefix(stream_id.device_index), sensor_prefix(stream_id.sensor_index), stream_prefix(stream_id.stream_type, stream_id.stream_index) }).substr(1); //substr(1) to remove the first "/"
        }

        static std::string notification_topic(const device_serializer::sensor_identifier& sensor_id, rs2_notification_category nc)
        {
            return create_from({ device_prefix(sensor_id.device_index), sensor_prefix(sensor_id.sensor_index), "notification", rs2_notification_category_to_string(nc)});
        }

        template<uint32_t index>
        static std::string get(const std::string& value)
        {
            size_t current_pos = 0;
            std::string value_copy = value;
            uint32_t elements_iterator = 0;
            const auto seperator_length = std::string(elements_separator()).length();
            while ((current_pos = value_copy.find(elements_separator())) != std::string::npos)
            {
                auto token = value_copy.substr(0, current_pos);
                if (elements_iterator == index)
                {
                    return token;
                }
                value_copy.erase(0, current_pos + seperator_length);
                ++elements_iterator;
            }

            if (elements_iterator == index)
                return value_copy;

            throw std::out_of_range( rsutils::string::from() << "Requested index \"" << index
                                                             << "\" is out of bound of topic: \"" << value << "\"" );
        }

        // Returns a human-readable stream name for ROS2 message frame_id (e.g., "Depth", "Infrared1")
        static std::string stream_name(rs2_stream type, uint32_t index)
        {
            std::string name = librealsense::get_string(type);
            if (type == RS2_STREAM_INFRARED)
                name += std::to_string(index);
            return name;
        }

    private:
        static std::string stream_to_ros_type(rs2_stream type)
        {
            switch (type)
            {
            case RS2_STREAM_CONFIDENCE:
            case RS2_STREAM_DEPTH:
            case RS2_STREAM_COLOR:
            case RS2_STREAM_INFRARED:
            case RS2_STREAM_FISHEYE:
                return ros_image_type_str();

            case RS2_STREAM_GYRO:
            case RS2_STREAM_ACCEL:
            case RS2_STREAM_MOTION:
                return ros_imu_type_str();

            case RS2_STREAM_POSE:
                return ros_pose_type_str();
            case RS2_STREAM_SAFETY:
                return ros_safety_type_str();
            case RS2_STREAM_OCCUPANCY:
                return ros_occupancy_type_str();
            case RS2_STREAM_LABELED_POINT_CLOUD:
                return ros_labeled_points_type_str();
            }
            throw io_exception( rsutils::string::from() << "Unknown stream type when resolving ros type: " << type );
        }
        static std::string create_from(const std::vector<std::string>& parts)
        {
            std::ostringstream oss;
            oss << elements_separator();
            if (parts.empty() == false)
            {
                std::copy(parts.begin(), parts.end() - 1, std::ostream_iterator<std::string>(oss, elements_separator()));
                oss << parts.back();
            }
            return oss.str();
        }


        static uint32_t get_id(const std::string& prefix, const std::string& str)
        {
            if (str.compare(0, prefix.size(), prefix) != 0)
            {
                throw std::runtime_error("Failed to get id after prefix \"" + prefix + "\"from string \"" + str + "\"");
            }

            std::string id_str = str.substr(prefix.size());
            return static_cast<uint32_t>(std::stoll(id_str));
        }

        static std::string device_prefix(uint32_t device_id)
        {
            return "device_" + std::to_string(device_id);
        }
        static std::string sensor_prefix(uint32_t sensor_id)
        {
            return "sensor_" + std::to_string(sensor_id);
        }
        static std::string stream_prefix(rs2_stream type, uint32_t stream_id)
        {
            std::string name = rs2_stream_to_string(type);
            std::replace(name.begin(), name.end(), ' ', '_');
            return name + "_" + std::to_string(stream_id);
        }
    };

    /**
    * Incremental number of the RealSense file format version
    * Since we maintain backward compatability, changes to topics/messages are reflected by the version
    */
    constexpr uint32_t get_file_version()
    {
        return ROS_FILE_WITH_RECOMMENDED_PROCESSING_BLOCKS;
    }

    constexpr uint32_t get_minimum_supported_file_version()
    {
        return ROS_FILE_VERSION_2;
    }

    constexpr uint32_t get_device_index()
    {
        return 0; //TODO: change once SDK file supports multiple devices
    }

    constexpr device_serializer::nanoseconds get_static_file_info_timestamp()
    {
        return device_serializer::nanoseconds::min();
    }

    // Lightweight CDR wrappers matching the ROS msg serialize/deserialize interface
    struct cdr_string {
        std::string value;
        void serialize(eprosima::fastcdr::Cdr& cdr) const { cdr << value; }
        void deserialize(eprosima::fastcdr::Cdr& cdr) { cdr >> value; }
        static size_t getCdrSerializedSize(const cdr_string& s, size_t = 0) { return 4 + s.value.size() + 1; }
    };

    struct cdr_uint32 {
        uint32_t value = 0;
        void serialize(eprosima::fastcdr::Cdr& cdr) const { cdr << value; }
        void deserialize(eprosima::fastcdr::Cdr& cdr) { cdr >> value; }
        static size_t getCdrSerializedSize(const cdr_uint32&, size_t = 0) { return sizeof(uint32_t); }
    };
}
