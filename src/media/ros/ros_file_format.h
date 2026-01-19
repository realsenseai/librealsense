// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once
#include <string>
#include <chrono>
#include "librealsense2/rs.h"
#include "sensor_msgs/image_encodings.h"
#include "sensor_msgs/Imu.h"
#include "sensor_msgs/Image.h"
#include "diagnostic_msgs/KeyValue.h"
#include "std_msgs/UInt32.h"
#include "std_msgs/Float32.h"
#include "std_msgs/Float32MultiArray.h"
#include "std_msgs/String.h"
#include "realsense_msgs/StreamInfo.h"
#include "realsense_msgs/ImuIntrinsic.h"
#include "realsense_msgs/Notification.h"
#include "realsense_legacy_msgs/legacy_headers.h"
#include "sensor_msgs/CameraInfo.h"
#include "geometry_msgs/Transform.h"
#include "geometry_msgs/Twist.h"
#include "geometry_msgs/Accel.h"
#include "metadata-parser.h"
#include "option.h"
#include "rosbag/structures.h"
#include "core/serialization.h"
#include <regex>
#include "stream.h"
#include "types.h"
#include <vector>

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
    /*
    quat2rot(), rot2quat()
    ------------------

    rotation matrix is column major
    m[3][3] <==> r[9]

    [00, 01, 02] <==> [ r[0], r[3], r[6] ]
    m = [10, 11, 12] <==> [ r[1], r[4], r[7] ]
    [20, 21, 22] <==> [ r[2], r[5], r[8] ]
    */

    inline void quat2rot(const geometry_msgs::Transform::_rotation_type& q, float(&r)[9])
    {
        r[0] = 1 - 2 * q.y*q.y - 2 * q.z*q.z;  //  m00 = 1 - 2 * qy2 - 2 * qz2
        r[3] = 2 * q.x*q.y - 2 * q.z*q.w;      //  m01 = 2 * qx*qy - 2 * qz*qw
        r[6] = 2 * q.x*q.z + 2 * q.y*q.w;      //  m02 = 2 * qx*qz + 2 * qy*qw
        r[1] = 2 * q.x*q.y + 2 * q.z*q.w;      //  m10 = 2 * qx*qy + 2 * qz*qw
        r[4] = 1 - 2 * q.x*q.x - 2 * q.z*q.z;  //  m11 = 1 - 2 * qx2 - 2 * qz2
        r[7] = 2 * q.y*q.z - 2 * q.x*q.w;      //  m12 = 2 * qy*qz - 2 * qx*qw
        r[2] = 2 * q.x*q.z - 2 * q.y*q.w;      //  m20 = 2 * qx*qz - 2 * qy*qw
        r[5] = 2 * q.y*q.z + 2 * q.x*q.w;      //  m21 = 2 * qy*qz + 2 * qx*qw
        r[8] = 1 - 2 * q.x*q.x - 2 * q.y*q.y;  //  m22 = 1 - 2 * qx2 - 2 * qy2
    }

    inline void rot2quat(const float(&r)[9], geometry_msgs::Transform::_rotation_type& q)
    {
        auto m = (float(&)[3][3])r; // column major
        float tr[4];
        tr[0] = ( m[0][0] + m[1][1] + m[2][2]);
        tr[1] = ( m[0][0] - m[1][1] - m[2][2]);
        tr[2] = (-m[0][0] + m[1][1] - m[2][2]);
        tr[3] = (-m[0][0] - m[1][1] + m[2][2]);
        if (tr[0] >= tr[1] && tr[0]>= tr[2] && tr[0] >= tr[3]) {
            float s = 2 * std::sqrt(tr[0] + 1);
            q.w = s/4;
            q.x = (m[2][1] - m[1][2]) / s;
            q.y = (m[0][2] - m[2][0]) / s;
            q.z = (m[1][0] - m[0][1]) / s;
        } else if (tr[1] >= tr[2] && tr[1] >= tr[3]) {
            float s = 2 * std::sqrt(tr[1] + 1);
            q.w = (m[2][1] - m[1][2]) / s;
            q.x = s/4;
            q.y = (m[1][0] + m[0][1]) / s;
            q.z = (m[2][0] + m[0][2]) / s;
        } else if (tr[2] >= tr[3]) {
            float s = 2 * std::sqrt(tr[2] + 1);
            q.w = (m[0][2] - m[2][0]) / s;
            q.x = (m[1][0] + m[0][1]) / s;
            q.y = s/4;
            q.z = (m[1][2] + m[2][1]) / s;
        } else {
            float s = 2 * std::sqrt(tr[3] + 1);
            q.w = (m[1][0] - m[0][1]) / s;
            q.x = (m[0][2] + m[2][0]) / s;
            q.y = (m[1][2] + m[2][1]) / s;
            q.z = s/4;
        }
        q.w = -q.w; // column major
    }

    inline bool convert(const geometry_msgs::Transform& source, rs2_extrinsics& target)
    {
        target.translation[0] = source.translation.x;
        target.translation[1] = source.translation.y;
        target.translation[2] = source.translation.z;
        quat2rot(source.rotation, target.rotation);
        return true;
    }

    inline bool convert(const rs2_extrinsics& source, geometry_msgs::Transform& target)
    {
        target.translation.x = source.translation[0];
        target.translation.y = source.translation[1];
        target.translation.z = source.translation[2];
        rot2quat(source.rotation, target.rotation);
        return true;
    }

    constexpr const char* FRAME_NUMBER_MD_STR = "Frame number";
    constexpr const char* TIMESTAMP_DOMAIN_MD_STR = "timestamp_domain";
    constexpr const char* SYSTEM_TIME_MD_STR = "system_time";
    constexpr const char* MAPPER_CONFIDENCE_MD_STR = "Mapper Confidence";
    constexpr const char* FRAME_TIMESTAMP_MD_STR = "frame_timestamp";
    constexpr const char* TRACKER_CONFIDENCE_MD_STR = "Tracker Confidence";
    constexpr const char* TIMESTAMP_MD_STR = "timestamp";

    class ros_topic
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
            rs2_stream type;
            convert(stream_with_id.substr(0, stream_with_id.find_first_of("_")), type);
            return type;
        }

        static uint32_t get_stream_index(const std::string& topic)
        {
            auto stream_with_id = get<3>(topic);
            return get_id(stream_with_id.substr(0, stream_with_id.find_first_of("_") + 1), get<3>(topic));
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
            return std::stoul(get<5>(topic));
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
            return create_from({ stream_full_prefix(stream_id), "info", "camera_info" });
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

        static std::string pose_transform_topic(const device_serializer::stream_identifier& stream_id)
        {
            assert(stream_id.stream_type == RS2_STREAM_POSE);
            return create_from({ stream_full_prefix(stream_id), stream_to_ros_type(stream_id.stream_type), "transform", "data" });
        }

        static std::string pose_accel_topic(const device_serializer::stream_identifier& stream_id)
        {
            assert(stream_id.stream_type == RS2_STREAM_POSE);
            return create_from({ stream_full_prefix(stream_id), stream_to_ros_type(stream_id.stream_type), "accel", "data" });
        }
        static std::string pose_twist_topic(const device_serializer::stream_identifier& stream_id)
        {
            assert(stream_id.stream_type == RS2_STREAM_POSE);
            return create_from({ stream_full_prefix(stream_id), stream_to_ros_type(stream_id.stream_type),"twist",  "data" });
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
            return create_from({ stream_full_prefix(stream_id), "tf", std::to_string(ref_id) });
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

            throw std::out_of_range( rsutils::string::from() << "Requeted index \"" << index
                                                             << "\" is out of bound of topic: \"" << value << "\"" );
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
            return std::string(rs2_stream_to_string(type)) + "_" + std::to_string(stream_id);
        }
    };

    class FalseQuery {
    public:
        bool operator()(rosbag::ConnectionInfo const* info) const { return false; }
    };

    class MultipleRegexTopicQuery
    {
    public:
        MultipleRegexTopicQuery(const std::vector<std::string>& regexps)
        {
            for (auto&& regexp : regexps)
            {
                LOG_DEBUG("RegexTopicQuery with expression: " << regexp);
                _exps.emplace_back(regexp);
            }
        }

        bool operator()(rosbag::ConnectionInfo const* info) const
        {
            return std::any_of(std::begin(_exps), std::end(_exps), [info](const std::regex& exp) { return std::regex_search(info->topic, exp); });
        }

    private:
        std::vector<std::regex> _exps;
    };

    class RegexTopicQuery : public MultipleRegexTopicQuery
    {
    public:
        RegexTopicQuery(std::string const& regexp) : MultipleRegexTopicQuery({ regexp })
        {
        }

        static std::string data_msg_types()
        {   //Either "image" or "imu" or "pose/transform" or "safety/occupancy/labeled_points"
            return rsutils::string::from() << ros_topic::ros_image_type_str() << "|" << ros_topic::ros_imu_type_str()
                                             << "|" << ros_topic::ros_safety_type_str()
                                             << "|" << ros_topic::ros_occupancy_type_str()
                                             << "|" << ros_topic::ros_labeled_points_type_str()
                                             << "|" << ros_topic::ros_pose_type_str() << "/transform";
        }

        static std::string stream_prefix(const device_serializer::stream_identifier& stream_id)
        {
            return rsutils::string::from()
                << "/device_" << stream_id.device_index << "/sensor_" << stream_id.sensor_index << "/"
                << get_string( stream_id.stream_type ) << "_" << stream_id.stream_index;                               
        }

    private:
        std::regex _exp;
    };

    class SensorInfoQuery : public RegexTopicQuery
    {
    public:
        SensorInfoQuery( uint32_t device_index )
            : RegexTopicQuery( rsutils::string::from()
                               << "/device_" << device_index << R"RRR(/sensor_(\d)+/info)RRR" )
        {
        }
    };

    class FrameQuery : public RegexTopicQuery
    {
    public:
        //TODO: Improve readability and robustness of expressions
        FrameQuery()
            : RegexTopicQuery( rsutils::string::from() << R"RRR(/device_\d+/sensor_\d+/.*_\d+)RRR"
                                                         << "/(" << data_msg_types() << ")/data" )
        {
        }
    };

    class StreamQuery : public RegexTopicQuery
    {
    public:
        StreamQuery( const device_serializer::stream_identifier & stream_id )
            : RegexTopicQuery( rsutils::string::from()
                               << stream_prefix( stream_id ) << "/(" << data_msg_types() << ")/data" )
        {
        }
    };

    class ExtrinsicsQuery : public RegexTopicQuery
    {
    public:
        ExtrinsicsQuery( const device_serializer::stream_identifier & stream_id )
            : RegexTopicQuery( stream_prefix( stream_id ) + "/tf" )
        {
        }
    };

    class OptionsQuery : public RegexTopicQuery
    {
    public:
        OptionsQuery()
            : RegexTopicQuery( R"RRR(/device_\d+/sensor_\d+/option/.*/value)RRR" )
        {
        }
    };

    class NotificationsQuery : public RegexTopicQuery
    {
    public:
        NotificationsQuery()
            : RegexTopicQuery( R"RRR(/device_\d+/sensor_\d+/notification/.*)RRR" )
        {
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

    inline device_serializer::nanoseconds to_nanoseconds(const rs2rosinternal::Time& t)
    {
        if (t == rs2rosinternal::TIME_MIN)
            return get_static_file_info_timestamp();

        return device_serializer::nanoseconds(t.toNSec());
    }

    inline rs2rosinternal::Time to_rostime(const device_serializer::nanoseconds& t)
    {
        if (t == get_static_file_info_timestamp())
            return rs2rosinternal::TIME_MIN;
        
        auto secs = std::chrono::duration_cast<std::chrono::duration<double>>(t);
        return rs2rosinternal::Time(secs.count());
    }

    namespace legacy_file_format
    {
        constexpr const char* USB_DESCRIPTOR = "{ 0x94b5fb99, 0x79f2, 0x4d66,{ 0x85, 0x06, 0xb1, 0x5e, 0x8b, 0x8c, 0x9d, 0xa1 } }";
        constexpr const char* DEVICE_INTERFACE_VERSION = "{ 0xafcd9c11, 0x52e3, 0x4258,{ 0x8d, 0x23, 0xbe, 0x86, 0xfa, 0x97, 0xa0, 0xab } }";
        constexpr const char* FW_VERSION = "{ 0x7b79605b, 0x5e36, 0x4abe,{ 0xb1, 0x01, 0x94, 0x86, 0xb8, 0x9a, 0xfe, 0xbe } }";
        constexpr const char* CENTRAL_VERSION = "{ 0x5652ffdb, 0xacac, 0x47ea,{ 0xbf, 0x65, 0x73, 0x3e, 0xf3, 0xd9, 0xe2, 0x70 } }";
        constexpr const char* CENTRAL_PROTOCOL_VERSION = "{ 0x50376dea, 0x89f4, 0x4d70,{ 0xb1, 0xb0, 0x05, 0xf6, 0x07, 0xb6, 0xae, 0x8a } }";
        constexpr const char* EEPROM_VERSION = "{ 0x4617d177, 0xb546, 0x4747,{ 0x9d, 0xbf, 0x4f, 0xf8, 0x99, 0x0c, 0x45, 0x6b } }";
        constexpr const char* ROM_VERSION = "{ 0x16a64010, 0xfee4, 0x4c67,{ 0xae, 0xc5, 0xa0, 0x4d, 0xff, 0x06, 0xeb, 0x0b } }";
        constexpr const char* TM_DEVICE_TYPE = "{ 0x1212e1d5, 0xfa3e, 0x4273,{ 0x9e, 0xbf, 0xe4, 0x43, 0x87, 0xbe, 0xe5, 0xe8 } }";
        constexpr const char* HW_VERSION = "{ 0x4439fcca, 0x8673, 0x4851,{ 0x9b, 0xb6, 0x1a, 0xab, 0xbd, 0x74, 0xbd, 0xdc } }";
        constexpr const char* STATUS = "{ 0x5d6c6637, 0x28c7, 0x4a90,{ 0xab, 0x35, 0x90, 0xb2, 0x1f, 0x1a, 0xe6, 0xb8 } }";
        constexpr const char* STATUS_CODE = "{ 0xe22a94a6, 0xed64, 0x46ea,{ 0x81, 0x52, 0x6a, 0xb3, 0x0b, 0x0e, 0x2a, 0x18 } }";
        constexpr const char* EXTENDED_STATUS = "{ 0xff6e50db, 0x3c5f, 0x43e7,{ 0xb4, 0x82, 0xb8, 0xc3, 0xa6, 0x8e, 0x78, 0xcd } }";
        constexpr const char* SERIAL_NUMBER = "{ 0x7d3e44e7, 0x8970, 0x4a32,{ 0x8e, 0xee, 0xe8, 0xd1, 0xd1, 0x32, 0xa3, 0x22 } }";
        constexpr const char* TIMESTAMP_SORT_TYPE = "{ 0xb409b217, 0xe5cd, 0x4a04,{ 0x9e, 0x85, 0x1a, 0x7d, 0x59, 0xd7, 0xe5, 0x61 } }";
        
        constexpr const char* DEPTH = "DEPTH";
        constexpr const char* COLOR = "COLOR";
        constexpr const char* INFRARED = "INFRARED";
        constexpr const char* FISHEYE = "FISHEYE";
        constexpr const char* ACCEL = "ACCLEROMETER"; //Yes, there is a typo, that's how it is saved.
        constexpr const char* GYRO = "GYROMETER";
        constexpr const char* POSE = "rs_6DoF";

        constexpr uint32_t actual_exposure = 0; //    float RS2_FRAME_METADATA_ACTUAL_EXPOSURE
        constexpr uint32_t actual_fps = 1; //    float
        constexpr uint32_t frame_counter = 2; //    float RS2_FRAME_METADATA_FRAME_COUNTER
        constexpr uint32_t frame_timestamp = 3; //    float RS2_FRAME_METADATA_FRAME_TIMESTAMP
        constexpr uint32_t sensor_timestamp = 4; //    float RS2_FRAME_METADATA_SENSOR_TIMESTAMP
        constexpr uint32_t gain_level = 5; //    float RS2_FRAME_METADATA_GAIN_LEVEL
        constexpr uint32_t auto_exposure = 6; //    float RS2_FRAME_METADATA_AUTO_EXPOSURE
        constexpr uint32_t white_balance = 7; //    float RS2_FRAME_METADATA_WHITE_BALANCE
        constexpr uint32_t time_of_arrival = 8; //    float RS2_FRAME_METADATA_TIME_OF_ARRIVAL
        constexpr uint32_t SYSTEM_TIMESTAMP = 65536; //    int64
        constexpr uint32_t TEMPRATURE = 65537; //    float
        constexpr uint32_t EXPOSURE_TIME = 65538; //    uint32_t
        constexpr uint32_t FRAME_LENGTH = 65539; //    uint32_t
        constexpr uint32_t ARRIVAL_TIMESTAMP = 65540; //    int64
        constexpr uint32_t CONFIDENCE = 65541; //    uint32

        inline bool convert_metadata_type(uint64_t type, rs2_frame_metadata_value& res)
        {
            switch (type)
            {
            case actual_exposure: res = RS2_FRAME_METADATA_ACTUAL_EXPOSURE; break;
                //Not supported case actual_fps: ;
            case frame_counter: res = RS2_FRAME_METADATA_FRAME_COUNTER; break;
            case frame_timestamp: res = RS2_FRAME_METADATA_FRAME_TIMESTAMP; break;
            case sensor_timestamp: res = RS2_FRAME_METADATA_SENSOR_TIMESTAMP; break;
            case gain_level: res = RS2_FRAME_METADATA_GAIN_LEVEL; break;
            case auto_exposure: res = RS2_FRAME_METADATA_AUTO_EXPOSURE; break;
            case white_balance: res = RS2_FRAME_METADATA_WHITE_BALANCE; break;
            case time_of_arrival: res = RS2_FRAME_METADATA_TIME_OF_ARRIVAL; break;
                //Not supported here case SYSTEM_TIMESTAMP:
                //Not supported case TEMPRATURE: res =  RS2_FRAME_METADATA_;
            case EXPOSURE_TIME: res = RS2_FRAME_METADATA_SENSOR_TIMESTAMP; break;
                //Not supported case FRAME_LENGTH: res =  RS2_FRAME_METADATA_;
            case ARRIVAL_TIMESTAMP: res = RS2_FRAME_METADATA_TIME_OF_ARRIVAL; break;
                //Not supported case CONFIDENCE: res =  RS2_FRAME_METADATA_;
            default:
                return false;
            }
            return true;
        }
        inline rs2_timestamp_domain convert(uint64_t source)
        {
            switch (source)
            {
            case 0 /*camera*/: //[[fallthrough]]
            case 1 /*microcontroller*/ : 
                return RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK;
            case 2: return RS2_TIMESTAMP_DOMAIN_SYSTEM_TIME;
            }
            throw std::runtime_error( rsutils::string::from() << "Unknown timestamp domain: " << source );
        }

        inline bool info_from_string(const std::string& str, rs2_camera_info& out)
        {
            const size_t number_of_hexadecimal_values_in_a_guid = 11;
            const std::string left_group = R"RE(\s*(0x[0-9a-fA-F]{1,8})\s*,\s*(0x[0-9a-fA-F]{1,4})\s*,\s*(0x[0-9a-fA-F]{1,4})\s*,\s*)RE";
            const std::string right_group = R"RE(\s*(0x[0-9a-fA-F]{1,2})\s*,\s*(0x[0-9a-fA-F]{1,2})\s*,\s*(0x[0-9a-fA-F]{1,2})\s*,\s*(0x[0-9a-fA-F]{1,2})\s*,\s*(0x[0-9a-fA-F]{1,2})\s*,\s*(0x[0-9a-fA-F]{1,2})\s*,\s*(0x[0-9a-fA-F]{1,2})\s*,\s*(0x[0-9a-fA-F]{1,2})\s*)RE";
            const std::string guid_regex_pattern = R"RE(\{)RE" + left_group + R"RE(\{)RE" + right_group + R"RE(\}\s*\})RE";
            //The GUID pattern looks like this:     "{ 0x________, 0x____, 0x____, { 0x__, 0x__, 0x__, ... , 0x__ } }"

            std::regex reg(guid_regex_pattern, std::regex_constants::icase);
            const std::map<rs2_camera_info, const char*> legacy_infos{
                { RS2_CAMERA_INFO_SERIAL_NUMBER                  , SERIAL_NUMBER },
                { RS2_CAMERA_INFO_FIRMWARE_VERSION               , FW_VERSION },
                { RS2_CAMERA_INFO_PHYSICAL_PORT                  , USB_DESCRIPTOR },
            };
            for (auto&& s : legacy_infos)
            {
                if (std::regex_match(s.second, reg))
                {
                    out = s.first;
                    return true;
                }
            }
            return false;
        }

        constexpr uint32_t file_version()
        {
            return 1u;
        }

        constexpr uint32_t get_maximum_supported_legacy_file_version()
        {
            return file_version();
        }
     
        inline std::string stream_type_to_string(const stream_descriptor& source)
        {
            //Other than 6DOF, streams are in the form of "<stream_type><stream_index>" where "stream_index" is empty for index 0/1 and the actual number for 2 and above
            //6DOF is in the form "rs_6DoF<stream_index>" where "stream_index" is a zero based index
            std::string name;
            switch (source.type)
            {
            case RS2_STREAM_DEPTH: name = DEPTH; break;
            case RS2_STREAM_COLOR: name = COLOR; break;
            case RS2_STREAM_INFRARED: name = INFRARED; break;
            case RS2_STREAM_FISHEYE: name = FISHEYE; break;
            case RS2_STREAM_GYRO: name = GYRO; break;
            case RS2_STREAM_ACCEL: name = ACCEL; break;
            case RS2_STREAM_POSE: name = POSE; break;
            default:
                throw io_exception( rsutils::string::from() << "Unknown stream type : " << source.type );
            }
            if (source.type == RS2_STREAM_POSE)
            {
                return name  + std::to_string(source.index);
            }
            else
            {
                if (source.index == 1)
                {
                    throw io_exception( rsutils::string::from()
                                        << "Unknown index for type : " << source.type << ", index = " << source.index );
                }
                return name + (source.index == 0 ? "" : std::to_string(source.index));
            }
        }

        inline stream_descriptor parse_stream_type(const std::string& source)
        {
            stream_descriptor retval{};
            auto starts_with = [source](const std::string& s) {return source.find(s) == 0; };
            std::string type_str;
            if (starts_with(DEPTH))
            {
                retval.type = RS2_STREAM_DEPTH;
                type_str = DEPTH;
            }
            else if (starts_with(COLOR))
            {
                retval.type = RS2_STREAM_COLOR;
                type_str = COLOR;
            }
            else if (starts_with(INFRARED))
            {
                retval.type = RS2_STREAM_INFRARED;
                type_str = INFRARED;
            }
            else if (starts_with(FISHEYE))
            {
                retval.type = RS2_STREAM_FISHEYE;
                type_str = FISHEYE;
            }
            else if (starts_with(ACCEL))
            {
                retval.type = RS2_STREAM_ACCEL;
                type_str = ACCEL;
            }
            else if (starts_with(GYRO))
            {
                retval.type = RS2_STREAM_GYRO;
                type_str = GYRO;
            }
            else if (starts_with(POSE))
            {
                retval.type = RS2_STREAM_POSE;
                auto index_str = source.substr(std::string(POSE).length());
                retval.index = std::stoi(index_str);
                return retval;
            }
            else
                throw io_exception( rsutils::string::from() << "Unknown stream type : " << source );

            auto index_str = source.substr(type_str.length());
            if (index_str.empty())
            {
                    
                retval.index = 0;
            }
            else
            {
                int index = std::stoi(index_str);
                assert(index != 1);
                retval.index = index;
            }
            return retval;
        }

        class FrameQuery : public MultipleRegexTopicQuery
        {
        public:
            //Possible patterns:
            //    /camera/<CAMERA_STREAM_TYPE><id>/image_raw/0
            //    /camera/rs_6DoF<id>/0
            //   /imu/ACCELEROMETER/imu_raw/0
            //   /imu/GYROMETER/imu_raw/0
            FrameQuery()
                : MultipleRegexTopicQuery( { R"RRR(/(camera|imu)/.*/(image|imu)_raw/\d+)RRR",
                                             R"RRR(/camera/rs_6DoF\d+/\d+)RRR" } )
            {
            }
        };

        inline bool is_camera(rs2_stream s)
        {
            return
                s == RS2_STREAM_DEPTH ||
                s == RS2_STREAM_COLOR ||
                s == RS2_STREAM_INFRARED ||
                s == RS2_STREAM_FISHEYE ||
                s == RS2_STREAM_POSE;
        }

        class StreamQuery : public RegexTopicQuery
        {
        public:
            StreamQuery( const device_serializer::stream_identifier & stream_id )
                : RegexTopicQuery( rsutils::string::from()
                                   << ( is_camera( stream_id.stream_type ) ? "/camera/" : "/imu/" )
                                   << stream_type_to_string( { stream_id.stream_type, (int)stream_id.stream_index } )
                                   << ( ( stream_id.stream_type == RS2_STREAM_POSE ) ? "/"
                                        : ( is_camera( stream_id.stream_type ) )     ? "/image_raw/"
                                                                                     : "/imu_raw/" )
                                   << stream_id.sensor_index )
            {
            }
        };

        class FrameInfoExt : public RegexTopicQuery
        {
        public:
            FrameInfoExt( const device_serializer::stream_identifier & stream_id )
                : RegexTopicQuery( rsutils::string::from()
                                   << ( is_camera( stream_id.stream_type ) ? "/camera/" : "/imu/" )
                                   << stream_type_to_string( { stream_id.stream_type, (int)stream_id.stream_index } )
                                   << "/rs_frame_info_ext/" << stream_id.sensor_index )
            {
            }
        };
        inline device_serializer::stream_identifier get_stream_identifier(const std::string& topic)
        {
            auto stream = parse_stream_type(ros_topic::get<2>(topic));
            uint32_t sensor_index;
            if(stream.type == RS2_STREAM_POSE)
                sensor_index = static_cast<uint32_t>(std::stoll(ros_topic::get<3>(topic)));
            else
                sensor_index = static_cast<uint32_t>(std::stoll(ros_topic::get<4>(topic)));

            return device_serializer::stream_identifier{ 0u,   static_cast<uint32_t>(sensor_index),  stream.type, static_cast<uint32_t>(stream.index) };
        }

        inline std::string file_version_topic()
        {
            return "/FILE_VERSION";
        }
    }
}
