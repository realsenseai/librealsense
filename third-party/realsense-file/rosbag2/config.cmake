if(NOT BUILD_ROSBAG2)
    message(STATUS "rosbag2: BUILD_ROSBAG2=OFF - skipping build of rosbag2 third-party components")
    return()
endif()

# -- zstd --
file(GLOB_RECURSE ZSTD_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/zstd/zstd.c"
)

file(GLOB_RECURSE ZSTD_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/zstd/*.h"
)

include(${CMAKE_CURRENT_LIST_DIR}/tinyxml2/config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/rcutils/config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/pluginlib/config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/rcpputils/config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/rosbag2_storage/config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/rosbag2_storage_default_plugins/config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/rosbag2_cpp/config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/rosbag2_compression/config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/console_bridge/config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/class_loader/config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/ament_index_cpp/config.cmake)
# Temporary workaround as we reach here twice for some reason...
if(NOT TARGET SQLite3)
    include(${CMAKE_CURRENT_LIST_DIR}/sqlite3/config.cmake)
endif()

set(HEADER_FILES_ROSBAG2
    ${HEADER_FILES_CONSOLE_BRIDGE_ROS2}
    ${HEADER_FILES_AMENT_INDEX}
    ${HEADER_FILES_CLASS_LOADER}
    ${HEADER_FILES_RCPPUTILS}
    ${HEADER_FILES_RCUTILS}
    ${HEADER_FILES_PLUGINLIB}
    ${HEADER_FILES_ROSBAG2_STORAGE}
    ${HEADER_FILES_ROSBAG2_STORAGE_DEFAULT_PLUGINS}
    ${HEADER_FILES_TINYXML2}
    ${HEADER_FILES_SQLITE3}
    ${ZSTD_HEADERS}
)


set(SOURCE_FILES_ROSBAG2
    ${SOURCE_FILES_CONSOLE_BRIDGE_ROS2}
    ${SOURCE_FILES_AMENT_INDEX}
    ${SOURCE_FILES_CLASS_LOADER}
    ${SOURCE_FILES_RCPPUTILS}
    ${SOURCE_FILES_RCUTILS}
    ${SOURCE_FILES_ROSBAG2_STORAGE}
    ${SOURCE_FILES_ROSBAG2_STORAGE_DEFAULT_PLUGINS}
    ${SOURCE_FILES_TINYXML2}
    ${SOURCE_FILES_SQLITE3}
    ${ZSTD_SOURCES}
)

set(ROSBAG2_HEADER_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/console_bridge/include/
    ${CMAKE_CURRENT_LIST_DIR}/ament_index_cpp/include/
    ${CMAKE_CURRENT_LIST_DIR}/class_loader/include/
    ${CMAKE_CURRENT_LIST_DIR}/rcpputils/include/
    ${CMAKE_CURRENT_LIST_DIR}/rcutils/include/
    ${CMAKE_CURRENT_LIST_DIR}/pluginlib/include/
    ${CMAKE_CURRENT_LIST_DIR}/rosbag2_storage/include/
    ${CMAKE_CURRENT_LIST_DIR}/rosbag2_storage_default_plugins/include/
    ${CMAKE_CURRENT_LIST_DIR}/tinyxml2/
    ${HEADER_DIR_SQLITE3}
)
