cmake_minimum_required(VERSION 3.10)

file(GLOB_RECURSE HEADER_FILES_RCPPUTILS
    "${CMAKE_CURRENT_LIST_DIR}/include/rcppmath/*.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/*.hpp"
)

file(GLOB SOURCE_FILES_RCPPUTILS
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)

message(STATUS "rosbag2: Added rcpputils sources and headers")
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};RCPPUTILS_BUILDING_LIBRARY")