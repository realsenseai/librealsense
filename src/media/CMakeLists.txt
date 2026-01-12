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
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_writer.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_writer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_reader.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_reader.cpp"
)

# Suppress deprecated-declarations warning for ros2 files that include third-party rosbag2 headers
# which use deprecated std::iterator (deprecated in C++17, removed in C++20)
if(NOT MSVC)
    set_source_files_properties(
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_writer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_reader.cpp"
        PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations"
    )
endif()
