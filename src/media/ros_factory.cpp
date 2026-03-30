// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "ros_factory.h"
#include "ros/ros_reader.h"
#include <core/info-interface.h>
#include "ros/ros_writer.h"
#ifdef BUILD_ROSBAG2
#include "ros2/ros2_reader.h"
#include "ros2/ros2_writer.h"
#endif

namespace librealsense
{
    bool is_db3_file(const std::string& filename)
    {
        if (filename.size() < 4)
            return false;
        return filename.substr(filename.size() - 4) == ".db3";
    }

    std::shared_ptr<device_serializer::reader> create_reader_for_file(
        const std::string& filename, const std::shared_ptr<context>& ctx)
    {
        if (is_db3_file(filename))
        {
#ifdef BUILD_ROSBAG2
            return std::make_shared<ros2_reader>(filename, ctx);
#else
            throw invalid_value_exception("Cannot open .db3 files without BUILD_ROSBAG2");
#endif
        }
        return std::make_shared<ros_reader>(filename, ctx);
    }

    std::shared_ptr<device_serializer::writer> create_writer_for_file(
        const std::string& file, bool compress)
    {
#ifdef BUILD_ROSBAG2
        return std::make_shared<ros2_writer>(file, compress);
#else
        if (is_db3_file(file))
            throw invalid_value_exception("Cannot record to .db3 without BUILD_ROSBAG2");
        return std::make_shared<ros_writer>(file, compress);
#endif
    }
}
