cmake_minimum_required(VERSION 3.10)

file(GLOB HEADER_FILES_AMENT_INDEX
    "${CMAKE_CURRENT_LIST_DIR}/include/ament_index_cpp/*.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/ament_index_cpp/*.h"
)

file(GLOB SOURCE_FILES_AMENT_INDEX
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)

set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};AMENT_INDEX_CPP_BUILDING_DLL")