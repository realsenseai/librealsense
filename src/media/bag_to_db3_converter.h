// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#pragma once

#include <string>
#include <memory>

namespace librealsense
{
    class context;

    // Converts a ROS1 .bag recording to a ROS2 .db3 recording.
    // The output_db3 path should not include the .db3 extension (it is appended automatically).
    void convert_bag_to_db3(const std::string& input_bag, const std::string& output_db3, std::shared_ptr<context> ctx);
}
