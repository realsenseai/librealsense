// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "reader_factory.h"
#include "ros/ros_reader.h"
#include "ros2/ros2_reader.h"
#include <iostream>

namespace librealsense
{
    bool is_bag_file(const std::string& filename)
    {
        if (filename.size() < 4)
            return false;
        auto ext = filename.substr(filename.size() - 4);
        return ext == ".bag" || ext == ".BAG";
    }

    std::shared_ptr<device_serializer::reader> create_reader_for_file(
        const std::string& filename, const std::shared_ptr<context>& ctx)
    {
        if (is_bag_file(filename))
        {
            std::cerr << "[WARNING] Opening '" << filename << "': ROS1 .bag format is deprecated and will be "
                      << "removed in a future release. Use rs-convert --output-db3 to convert to .db3 format."
                      << std::endl;
            return std::make_shared<ros_reader>(filename, ctx);
        }
        return std::make_shared<ros2_reader>(filename, ctx);
    }
}
