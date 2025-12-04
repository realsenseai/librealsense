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
    GIT_TAG yaml-cpp-0.7.0  # skip tag 0.8.0 - it adds an uninstall target that makes our duplicate, and YAML_CPP_DISABLE_UNINSTALL flag was only set later to not make that target
)
# Disable yaml-cpp's uninstall target
#set(YAML_CPP_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)

set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
FetchContent_MakeAvailable(yaml-cpp)
unset(CMAKE_POLICY_VERSION_MINIMUM)

list(APPEND ROSBAG_HEADER_DIRS ${ROSBAG2_HEADER_DIRS})
list(APPEND SOURCE_FILES_ROSBAG ${SOURCE_FILES_ROSBAG2})
list(APPEND HEADER_FILES_ROSBAG ${HEADER_FILES_ROSBAG2})
