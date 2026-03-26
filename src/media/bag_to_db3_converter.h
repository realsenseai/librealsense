// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <string>
#include <memory>
#include <functional>

namespace librealsense
{
    class context;

    // Converts a ROS1 .bag recording to a ROS2 .db3 recording.
    // If progress_callback is set, it is called with a value in [0,1] as frames are written.
    void convert_bag_to_db3(const std::string& input_bag, const std::string& output_db3, std::shared_ptr<context> ctx,
                            std::function<void(float)> progress_callback = nullptr);
}
