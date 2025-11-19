set(ROSBAG_DIR ${CMAKE_CURRENT_LIST_DIR}/rosbag)
set(LZ4_DIR ${CMAKE_CURRENT_LIST_DIR}/lz4)

set(LZ4_INCLUDE_PATH ${LZ4_DIR}/lib/)
include(${ROSBAG_DIR}/config.cmake)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/rosbag2
                 ${CMAKE_CURRENT_BINARY_DIR}/rosbag2_build)
# Include rosbag2 configuration
#include(${ROSBAG_DIR}2/config.cmake)


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

# Force SQLite to export all symbols on Windows so the .lib is generated
# There's probably a better way to do it but it works for now - either have the folder on the repo, or pass the flag
# if there's no better way, we can and should restore the old flag
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

FetchContent_Declare(
    sqlite3
    GIT_REPOSITORY https://github.com/sjinks/sqlite3-cmake
    GIT_TAG v3.49.1
)
FetchContent_MakeAvailable(sqlite3)

# Reset it so it doesn't affect other libraries
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF) # TODO restore previous value instead of forcing OFF


list(APPEND ROSBAG_HEADER_DIRS ${ROSBAG2_HEADER_DIRS})
list(APPEND SOURCE_FILES_ROSBAG ${SOURCE_FILES_ROSBAG2})
list(APPEND HEADER_FILES_ROSBAG ${HEADER_FILES_ROSBAG2})
