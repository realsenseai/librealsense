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
        "${CMAKE_CURRENT_LIST_DIR}/ros_factory.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros_factory.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros_common.h"
        "${CMAKE_CURRENT_LIST_DIR}/bag_to_db3_converter.h"
        "${CMAKE_CURRENT_LIST_DIR}/bag_to_db3_converter.cpp"
)

if(BUILD_ROSBAG2)
    target_compile_definitions(${LRS_TARGET} PRIVATE BUILD_ROSBAG2)
    # Include path for ROS2 message types (only needed when building rosbag2 support)
    target_include_directories(${LRS_TARGET} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types")
    set(ROS2_SOURCE_FILES
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_file_format.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_image_codec.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_image_codec.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_writer.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_writer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_reader_base.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_reader_base.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_reader.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_reader.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_native_reader.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2_native_reader.cpp"
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
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/sensor_msgs/msg/RegionOfInterest.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/sensor_msgs/msg/RegionOfInterest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/sensor_msgs/msg/CameraInfo.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/sensor_msgs/msg/CameraInfo.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/sensor_msgs/msg/CompressedImage.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/sensor_msgs/msg/CompressedImage.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/geometry_msgs/msg/Transform.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/geometry_msgs/msg/Transform.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/geometry_msgs/msg/TransformStamped.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/geometry_msgs/msg/TransformStamped.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/tf2_msgs/msg/TFMessage.h"
        "${CMAKE_CURRENT_LIST_DIR}/ros2/ros2-msg-types/tf2_msgs/msg/TFMessage.cpp"
    )

    # libdeflate: portable PNG (zlib) compression for CompressedImage topics —
    # byte-identical output on every architecture
    include(${PROJECT_SOURCE_DIR}/CMake/external_libdeflate.cmake)
    set(LIBDEFLATE_DIR "${LIBDEFLATE_SOURCE_DIR}")
    set(LIBDEFLATE_SOURCES
        "${LIBDEFLATE_DIR}/lib/adler32.c"
        "${LIBDEFLATE_DIR}/lib/crc32.c"
        "${LIBDEFLATE_DIR}/lib/deflate_compress.c"
        "${LIBDEFLATE_DIR}/lib/utils.c"
        "${LIBDEFLATE_DIR}/lib/zlib_compress.c"
        "${LIBDEFLATE_DIR}/lib/arm/cpu_features.c"
        "${LIBDEFLATE_DIR}/lib/x86/cpu_features.c"
        # "${LIBDEFLATE_DIR}/lib/x86/decompress_impl.h" # decode uses stb_image instead
    )
    target_include_directories(${LRS_TARGET} PRIVATE "${LIBDEFLATE_DIR}")
    target_sources(${LRS_TARGET} PRIVATE ${LIBDEFLATE_SOURCES})
    if(BUILD_SHARED_LIBS AND UNIX AND NOT APPLE)
        # C sources: -fvisibility-inlines-hidden from hide_sources_symbols is C++-only
        set_source_files_properties(${LIBDEFLATE_SOURCES} PROPERTIES COMPILE_FLAGS "-fvisibility=hidden")
    endif()

    target_sources(${LRS_TARGET} PRIVATE ${ROS2_SOURCE_FILES})
    hide_sources_symbols( ${ROS2_SOURCE_FILES} )
endif()
