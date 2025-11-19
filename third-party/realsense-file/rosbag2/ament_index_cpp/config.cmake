cmake_minimum_required(VERSION 3.10)

set(HEADER_FILES_AMENT_INDEX
    ${CMAKE_CURRENT_LIST_DIR}/include/ament_index_cpp/get_packages_with_prefixes.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/ament_index_cpp/get_package_prefix.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/ament_index_cpp/get_package_share_directory.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/ament_index_cpp/get_resource.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/ament_index_cpp/get_resources.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/ament_index_cpp/get_search_paths.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/ament_index_cpp/has_resource.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/ament_index_cpp/visibility_control.h
)

set(SOURCE_FILES_AMENT_INDEX
    ${CMAKE_CURRENT_LIST_DIR}/src/get_packages_with_prefixes.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/get_package_prefix.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/get_package_share_directory.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/get_resource.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/get_resources.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/get_search_paths.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/has_resource.cpp
)

set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};AMENT_INDEX_CPP_BUILDING_DLL")