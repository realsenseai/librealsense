// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
// Shared definitions between ros_file_format.h and ros2_file_format.h.
// Fold into ros2_file_format.h once ROS1 .bag support is removed.

#pragma once
#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <typeinfo>
#include "librealsense2/rs.h"
#include "sensor_msgs/image_encodings.h"
#include "core/serialization.h"
#include <rsutils/easylogging/easyloggingpp.h>


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
        else if (source == sensor_msgs::image_encodings::TYPE_8UC1) {
            target = RS2_FORMAT_Y8;
            mapped_format = true;
        }
        else if (source == sensor_msgs::image_encodings::TYPE_16UC1) {
            target = RS2_FORMAT_Y16;
            mapped_format = true;
        }
        else if (source == sensor_msgs::image_encodings::MONO8) {
            target = RS2_FORMAT_RAW8;
            mapped_format = true;
        }
        else if (source == sensor_msgs::image_encodings::YUV422) {
            target = RS2_FORMAT_UYVY;
            mapped_format = true;
        }
        else if (source == sensor_msgs::image_encodings::RGB8)       target = RS2_FORMAT_RGB8;
        else if (source == sensor_msgs::image_encodings::BGR8)       target = RS2_FORMAT_BGR8;
        else if (source == sensor_msgs::image_encodings::RGBA8)      target = RS2_FORMAT_RGBA8;
        else if (source == sensor_msgs::image_encodings::BGRA8)      target = RS2_FORMAT_BGRA8;

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
            LOG_ERROR("Failed to convert source: " << source << " to matching rs2_format");
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

    /**
    * Incremental number of the RealSense file format version
    * Since we maintain backward compatibility, changes to topics/messages are reflected by the version
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
}
