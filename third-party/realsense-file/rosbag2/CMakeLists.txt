cmake_minimum_required(VERSION 3.10)
project(rosbag2_independent)

if(NOT BUILD_ROSBAG2)
    message(STATUS "rosbag2: BUILD_ROSBAG2=OFF - skipping build of rosbag2 third-party components")
    return()
endif()

# Prevent duplicate processing if this directory is added more than once
if(TARGET rcutils OR TARGET rosbag2_independent)
    message(STATUS "rosbag2: targets already defined - skipping duplicate inclusion")
    return()
endif()

set(CMAKE_CXX_STANDARD 14)

# -- zstd --
file(GLOB_RECURSE ZSTD_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/zstd/zstd.c"
)

file(GLOB_RECURSE ZSTD_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/zstd/*.h"
)


include(tinyxml2/config.cmake)

#add_library(tinyxml2 STATIC tinyxml2/tinyxml2.cpp)
#target_include_directories(tinyxml2 PUBLIC tinyxml2)

set(CLASS_LOADER_IGNORE_AMENT TRUE CACHE BOOL "Do not use ament" FORCE) # for class_loader

# Add subdirectories (adjust pluginlib path to nested folder)
include(rcutils/config.cmake)
include(pluginlib/config.cmake)
include(rcpputils/config.cmake)
include(rosbag2_storage/config.cmake)
include(rosbag2_storage_default_plugins/config.cmake)
include(rosbag2_cpp/config.cmake)
include(rosbag2_compression/config.cmake)
include(console_bridge/config.cmake)
include(class_loader/config.cmake)
include(ament_index_cpp/config.cmake)

include(config.cmake) # sets variables with values for header/source files and dirs

message(STATUS "ROSBAG2_COMPILE_FLAGS (INTERNAL): ${ROSBAG2_COMPILE_FLAGS}")
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS}" PARENT_SCOPE) # PARENT_SCOPE to propagate to parent cmake files