# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#
# Build integration for vendored ROS2 rosbag2 libraries.
# This file lists sources, headers, include dirs, and compile flags
# for all vendored ROS2 components used by librealsense.
#
# The vendored library folders contain unmodified upstream code.
# sqlite3 and yaml-cpp are fetched externally via CMake/external_sqlite3.cmake
# and CMake/external_yaml_cpp.cmake respectively.

if(NOT BUILD_ROSBAG2)
    message(STATUS "rosbag2: BUILD_ROSBAG2=OFF - skipping build of rosbag2 third-party components")
    return()
endif()

set(ROSBAG2_COMPILE_FLAGS)

# -- zstd --
file(GLOB_RECURSE ZSTD_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/zstd/zstd.c"
)
file(GLOB_RECURSE ZSTD_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/zstd/*.h"
)

# -- ament_index_cpp --
file(GLOB HEADER_FILES_AMENT_INDEX
    "${CMAKE_CURRENT_LIST_DIR}/ament_index_cpp/include/ament_index_cpp/*.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/ament_index_cpp/include/ament_index_cpp/*.h"
)
file(GLOB SOURCE_FILES_AMENT_INDEX
    "${CMAKE_CURRENT_LIST_DIR}/ament_index_cpp/src/*.cpp"
)
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};AMENT_INDEX_CPP_BUILDING_DLL")

# -- class_loader --
file(GLOB HEADER_FILES_CLASS_LOADER
    "${CMAKE_CURRENT_LIST_DIR}/class_loader/include/class_loader/*.hpp"
)
file(GLOB SOURCE_FILES_CLASS_LOADER
    "${CMAKE_CURRENT_LIST_DIR}/class_loader/src/*.cpp"
)
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};CLASS_LOADER_BUILDING_DLL")

# -- console_bridge --
set(HEADER_FILES_CONSOLE_BRIDGE
    ${CMAKE_CURRENT_LIST_DIR}/console_bridge/include/console_bridge/console.h
)
# Source guard: rosbag1 may already compile console.cpp into the same library
if(NOT SOURCE_FILES_CONSOLE_BRIDGE)
    set(SOURCE_FILES_CONSOLE_BRIDGE
        ${CMAKE_CURRENT_LIST_DIR}/console_bridge/src/console.cpp
    )
endif()

# -- pluginlib --
file(GLOB_RECURSE HEADER_FILES_PLUGINLIB
    "${CMAKE_CURRENT_LIST_DIR}/pluginlib/include/pluginlib/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/pluginlib/include/pluginlib/*.hpp"
)

# -- rcpputils --
file(GLOB_RECURSE HEADER_FILES_RCPPUTILS
    "${CMAKE_CURRENT_LIST_DIR}/rcpputils/include/rcppmath/*.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/rcpputils/include/rcpputils/*.hpp"
)
file(GLOB SOURCE_FILES_RCPPUTILS
    "${CMAKE_CURRENT_LIST_DIR}/rcpputils/src/*.cpp"
)
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};RCPPUTILS_BUILDING_LIBRARY")

# -- rcutils --
if(UNIX AND NOT APPLE)
    include(${CMAKE_CURRENT_LIST_DIR}/rcutils/cmake/check_c_compiler_uses_glibc.cmake)
    check_c_compiler_uses_glibc(USES_GLIBC)
    if(USES_GLIBC)
        set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};_GNU_SOURCE")
    endif()
endif()
if(WIN32)
    set(time_impl_c time_win32.c)
else()
    set(time_impl_c time_unix.c)
endif()
file(GLOB_RECURSE HEADER_FILES_RCUTILS
    "${CMAKE_CURRENT_LIST_DIR}/rcutils/include/rcutils/*.h"
)
file(GLOB SOURCE_FILES_RCUTILS
    "${CMAKE_CURRENT_LIST_DIR}/rcutils/src/*.c"
)
list(FILTER SOURCE_FILES_RCUTILS EXCLUDE REGEX ".*time_win32\\.c$")
list(FILTER SOURCE_FILES_RCUTILS EXCLUDE REGEX ".*time_unix\\.c$")
list(APPEND SOURCE_FILES_RCUTILS "${CMAKE_CURRENT_LIST_DIR}/rcutils/src/${time_impl_c}")
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};RCUTILS_BUILDING_DLL")

# -- rosbag2_storage --
file(GLOB_RECURSE SOURCE_FILES_ROSBAG2_STORAGE
    "${CMAKE_CURRENT_LIST_DIR}/rosbag2_storage/src/**/*.cpp"
)
file(GLOB_RECURSE HEADER_FILES_ROSBAG2_STORAGE
    "${CMAKE_CURRENT_LIST_DIR}/rosbag2_storage/include/**/*.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/rosbag2_storage/include/**/*.h"
)
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};PLUGINLIB__DISABLE_BOOST_FUNCTIONS")
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};ROSBAG2_STORAGE_BUILDING_DLL")

# -- rosbag2_storage_default_plugins --
file(GLOB_RECURSE HEADER_FILES_ROSBAG2_STORAGE_DEFAULT_PLUGINS
    "${CMAKE_CURRENT_LIST_DIR}/rosbag2_storage_default_plugins/include/rosbag2_storage_default_plugins/*.hpp"
)
file(GLOB_RECURSE SOURCE_FILES_ROSBAG2_STORAGE_DEFAULT_PLUGINS
    "${CMAKE_CURRENT_LIST_DIR}/rosbag2_storage_default_plugins/src/rosbag2_storage_default_plugins/*.cpp"
)
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};ROSBAG2_STORAGE_DEFAULT_PLUGINS_BUILDING_DLL")

# -- tinyxml2 --
set(HEADER_FILES_TINYXML2
    ${CMAKE_CURRENT_LIST_DIR}/tinyxml2/tinyxml2.h
)
set(SOURCE_FILES_TINYXML2
    ${CMAKE_CURRENT_LIST_DIR}/tinyxml2/tinyxml2.cpp
)

# -- Aggregate all sources, headers, and include directories --
set(HEADER_FILES_ROSBAG2
    ${HEADER_FILES_AMENT_INDEX}
    ${HEADER_FILES_CLASS_LOADER}
    ${HEADER_FILES_CONSOLE_BRIDGE}
    ${HEADER_FILES_PLUGINLIB}
    ${HEADER_FILES_RCPPUTILS}
    ${HEADER_FILES_RCUTILS}
    ${HEADER_FILES_ROSBAG2_STORAGE}
    ${HEADER_FILES_ROSBAG2_STORAGE_DEFAULT_PLUGINS}
    ${HEADER_FILES_TINYXML2}
    ${ZSTD_HEADERS}
)

set(SOURCE_FILES_ROSBAG2
    ${SOURCE_FILES_AMENT_INDEX}
    ${SOURCE_FILES_CLASS_LOADER}
    ${SOURCE_FILES_CONSOLE_BRIDGE}
    ${SOURCE_FILES_RCPPUTILS}
    ${SOURCE_FILES_RCUTILS}
    ${SOURCE_FILES_ROSBAG2_STORAGE}
    ${SOURCE_FILES_ROSBAG2_STORAGE_DEFAULT_PLUGINS}
    ${SOURCE_FILES_TINYXML2}
    ${ZSTD_SOURCES}
)

set(ROSBAG2_HEADER_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/ament_index_cpp/include/
    ${CMAKE_CURRENT_LIST_DIR}/class_loader/include/
    ${CMAKE_CURRENT_LIST_DIR}/console_bridge/include/
    ${CMAKE_CURRENT_LIST_DIR}/pluginlib/include/
    ${CMAKE_CURRENT_LIST_DIR}/rcpputils/include/
    ${CMAKE_CURRENT_LIST_DIR}/rcutils/include/
    ${CMAKE_CURRENT_LIST_DIR}/rosbag2_storage/include/
    ${CMAKE_CURRENT_LIST_DIR}/rosbag2_storage_default_plugins/include/
    ${CMAKE_CURRENT_LIST_DIR}/tinyxml2/
    ${CMAKE_CURRENT_LIST_DIR}/zstd/
    ${HEADER_DIR_SQLITE3}
    ${HEADER_DIR_YAML_CPP}
)
