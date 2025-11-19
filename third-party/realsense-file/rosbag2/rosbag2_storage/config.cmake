cmake_minimum_required(VERSION 3.10)

#set(HEADER_FILES_ROSBAG2_STORAGE
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/bag_metadata.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/logging.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/metadata_io.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/ros_helper.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/serialized_bag_message.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/storage_factory.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/storage_factory_interface.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/storage_filter.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/storage_traits.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/topic_metadata.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/visibility_control.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/storage_options.hpp
# #   ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/yaml.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/storage_interfaces/base_info_interface.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/storage_interfaces/base_io_interface.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/storage_interfaces/base_read_interface.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/storage_interfaces/base_write_interface.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/storage_interfaces/read_only_interface.hpp
#    ${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage/storage_interfaces/read_write_interface.hpp
#)
#
#set(SOURCE_FILES_ROSBAG2_STORAGE
#    ${CMAKE_CURRENT_LIST_DIR}/src/rosbag2_storage/base_io_interface.cpp
#    ${CMAKE_CURRENT_LIST_DIR}/src/rosbag2_storage/metadata_io.cpp
#    ${CMAKE_CURRENT_LIST_DIR}/src/rosbag2_storage/ros_helper.cpp
#    ${CMAKE_CURRENT_LIST_DIR}/src/rosbag2_storage/storage_factory.cpp
#)

file(GLOB_RECURSE SOURCE_FILES_ROSBAG2_STORAGE
    "${CMAKE_CURRENT_LIST_DIR}/src/**/*.cpp"
)
file(GLOB_RECURSE HEADER_FILES_ROSBAG2_STORAGE
    "${CMAKE_CURRENT_LIST_DIR}/include/**/*.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/**/*.h"
)

set(SOURCE_FILES_ROSBAG2_STORAGE ${SOURCE_FILES_ROSBAG2_STORAGE} PARENT_SCOPE)
set(HEADER_FILES_ROSBAG2_STORAGE ${HEADER_FILES_ROSBAG2_STORAGE} PARENT_SCOPE)
set(ROSBAG2_STORAGE_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/include" PARENT_SCOPE)

#add compiler flags
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};PLUGINLIB__DISABLE_BOOST_FUNCTIONS")
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};ROSBAG2_STORAGE_BUILDING_DLL")