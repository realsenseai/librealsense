// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <core/serialization.h>
#include <memory>
#include <string>

namespace librealsense
{
    class context;

    bool is_bag_file(const std::string& filename);
    bool is_db3_file(const std::string& filename);

    // Dispatches to ros_reader (.bag) or ros2_reader (.db3) based on file extension
    std::shared_ptr<device_serializer::reader> create_reader_for_file(
        const std::string& filename, const std::shared_ptr<context>& ctx);
}
