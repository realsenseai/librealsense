cmake_minimum_required(VERSION 3.10)

file(GLOB_RECURSE HEADER_FILES_ROSBAG2_STORAGE_DEFAULT_PLUGINS
    "${CMAKE_CURRENT_LIST_DIR}/include/rosbag2_storage_default_plugins/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_ROSBAG2_STORAGE_DEFAULT_PLUGINS
    "${CMAKE_CURRENT_LIST_DIR}/src/rosbag2_storage_default_plugins/*.cpp"
)

message(STATUS "ROSBAG2_COMPILE_FLAGS before adding rosbag2_storage_default_plugins: ${ROSBAG2_COMPILE_FLAGS}")
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};ROSBAG2_STORAGE_DEFAULT_PLUGINS_BUILDING_DLL") # for some reason, the previous line doesn't propagate to parent scope
message(STATUS "ROSBAG2_COMPILE_FLAGS after adding rosbag2_storage_default_plugins: ${ROSBAG2_COMPILE_FLAGS}")