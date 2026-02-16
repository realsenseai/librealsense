# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

# Include path for ROS2 message types
target_include_directories(${LRS_TARGET} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types")

target_sources(${LRS_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/playback/playback-device-info.h"
        "${CMAKE_CURRENT_LIST_DIR}/record/record_device.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/record/record_sensor.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/playback/playback_device.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/playback/playback_sensor.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/record/record_device.h"
        "${CMAKE_CURRENT_LIST_DIR}/record/record_sensor.h"
        "${CMAKE_CURRENT_LIST_DIR}/playback/playback_device.h"
        "${CMAKE_CURRENT_LIST_DIR}/playback/playback_sensor.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros/ros_reader.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros/ros_writer.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros/ros_reader.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros/ros_writer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros/ros_file_format.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_file_format.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_writer.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_writer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_reader.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_reader.cpp"
        # ROS2 message types for CDR serialization
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/builtin_interfaces/msg/Time.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/builtin_interfaces/msg/Time.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/std_msgs/msg/Header.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/std_msgs/msg/Header.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/geometry_msgs/msg/Vector3.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/geometry_msgs/msg/Vector3.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/geometry_msgs/msg/Quaternion.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/geometry_msgs/msg/Quaternion.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/sensor_msgs/msg/Image.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/sensor_msgs/msg/Image.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/sensor_msgs/msg/Imu.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/sensor_msgs/msg/Imu.cpp"
)
