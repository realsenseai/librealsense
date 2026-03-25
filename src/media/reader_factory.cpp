// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "reader_factory.h"
#include "ros/ros_reader.h"
#include "ros2/ros2_reader.h"

namespace librealsense
{
    bool is_bag_file(const std::string& filename)
    {
        if (filename.size() < 4)
            return false;
        auto ext = filename.substr(filename.size() - 4);
        return ext == ".bag" || ext == ".BAG";
    }

    bool is_db3_file(const std::string& filename)
    {
        if (filename.size() < 4)
            return false;
        return filename.substr(filename.size() - 4) == ".db3";
    }

    std::shared_ptr<device_serializer::reader> create_reader_for_file(
        const std::string& filename, const std::shared_ptr<context>& ctx)
    {
        if (is_bag_file(filename))
            return std::make_shared<ros_reader>(filename, ctx);

        return std::make_shared<ros2_reader>(filename, ctx);
    }
}
