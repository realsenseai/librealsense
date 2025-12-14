file(GLOB_RECURSE ROSBAG2_CPP_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_cpp/*.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_cpp/*.h"
)

# Gather all source files
file(GLOB_RECURSE ROSBAG2_CPP_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/rosbag2_cpp/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/rosbag2_cpp/*.c"
)