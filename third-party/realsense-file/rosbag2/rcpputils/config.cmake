cmake_minimum_required(VERSION 3.10)

set(HEADER_FILES_RCPPUTILS
    ${CMAKE_CURRENT_LIST_DIR}/include/rcppmath/clamp.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/asserts.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/endian.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/filesystem_helper.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/find_and_replace.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/find_library.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/get_env.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/join.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/pointer_traits.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/process.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/scope_exit.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/shared_library.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/split.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/thread_safety_annotations.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/rcpputils/visibility_control.hpp
)

set(SOURCE_FILES_RCPPUTILS
    ${CMAKE_CURRENT_LIST_DIR}/src/asserts.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/find_library.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/shared_library.cpp
)

message(STATUS "rosbag2: Added rcpputils sources and headers")
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};RCPPUTILS_BUILDING_LIBRARY")