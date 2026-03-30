// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <core/serialization.h>
#include <memory>
#include <string>

namespace librealsense
{
    class context;

    bool is_db3_file(const std::string& filename);

    // Dispatches to ros_reader or ros2_reader based on file extension (.db3 → ROS2, everything else → ROS1)
    std::shared_ptr<device_serializer::reader> create_reader_for_file(
        const std::string& filename, const std::shared_ptr<context>& ctx);

    // With BUILD_ROSBAG2: always ros2_writer (requires .db3 extension)
    // Without BUILD_ROSBAG2: ros_writer (rejects .db3)
    std::shared_ptr<device_serializer::writer> create_writer_for_file(
        const std::string& file, bool compress);
}
