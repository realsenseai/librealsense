set(ROSBAG_DIR ${CMAKE_CURRENT_LIST_DIR}/rosbag)
set(LZ4_DIR ${CMAKE_CURRENT_LIST_DIR}/lz4)

set(LZ4_INCLUDE_PATH ${LZ4_DIR}/lib/)
include(${ROSBAG_DIR}/config.cmake)
include(${ROSBAG_DIR}2/config.cmake)

# ---- FetchContent for direct library dependencies ----
include(FetchContent)

# yaml-cpp
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG a83cd31548b19d50f3f983b069dceb4f4d50756d  # need any commit/tag after tag 0.8.0 - we need YAML_CPP_DISABLE_UNINSTALL flag
)
# Disable yaml-cpp's uninstall target
set(YAML_CPP_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(yaml-cpp)

list(APPEND ROSBAG_HEADER_DIRS ${ROSBAG2_HEADER_DIRS})
list(APPEND SOURCE_FILES_ROSBAG ${SOURCE_FILES_ROSBAG2})
list(APPEND HEADER_FILES_ROSBAG ${HEADER_FILES_ROSBAG2})
