# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

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
        # ROS2 bag (rosbag2 storage) minimal reader/writer
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_writer.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_writer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_reader.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_reader.cpp"
)

# Include paths for rosbag2 storage (third-party vendor drop)
target_include_directories(${LRS_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third-party/realsense-file/rosbag2/rosbag2_storage/include
        ${CMAKE_SOURCE_DIR}/third-party/realsense-file/rosbag2/rosbag2_storage_default_plugins/include
        ${CMAKE_SOURCE_DIR}/third-party/realsense-file/rosbag2/rcutils/include
        ${CMAKE_SOURCE_DIR}/third-party/realsense-file/rosbag2/rcpputils/include
)
