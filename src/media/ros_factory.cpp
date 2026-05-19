// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "ros_factory.h"
#include "ros/ros_reader.h"
#include <core/info-interface.h>
#include "ros/ros_writer.h"
#ifdef BUILD_ROSBAG2
#include "ros2/ros2_reader.h"
#include "ros2/ros2_writer.h"
#include "rcutils/logging.h"
#endif

namespace librealsense
{
#ifdef BUILD_ROSBAG2
    // Redirect rosbag2/rcutils logging through librealsense's logging system.
    // Without this, rcutils prints messages like "Opened database '...' for READ_WRITE"
    // directly to the console on every record/playback. Only errors are forwarded;
    // lower-severity messages (info, debug, warn) are suppressed.
    static void rcutils_to_librealsense_log(
        const rcutils_log_location_t *, int severity, const char *,
        rcutils_time_point_value_t, const char * format, va_list * args)
    {
        if (severity < RCUTILS_LOG_SEVERITY_ERROR)
            return;
        char buf[512];
        vsnprintf(buf, sizeof(buf), format, *args);
        LOG_ERROR(buf);
    }

#endif

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
            rcutils_logging_set_output_handler(rcutils_to_librealsense_log);
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
        rcutils_logging_set_output_handler(rcutils_to_librealsense_log);
        return std::make_shared<ros2_writer>(file, compress);
#else
        if (is_db3_file(file))
            throw invalid_value_exception("Cannot record to .db3 without BUILD_ROSBAG2");
        return std::make_shared<ros_writer>(file, compress);
#endif
    }
}
