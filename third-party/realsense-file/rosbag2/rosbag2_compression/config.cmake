file(GLOB_RECURSE ROSBAG2_COMPRESSION_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_compression/*.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_compression/*.h"
)

# Gather all source files
file(GLOB_RECURSE ROSBAG2_COMPRESSION_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/rosbag2_compression/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/rosbag2_compression/*.c"
)