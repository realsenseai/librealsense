# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#
# Fetch libdeflate for PNG (zlib) compression of ROS2 CompressedImage frame topics
# Uses configure-time git clone (compatible with CMake 3.10+); sources are compiled
# directly into the realsense2 target — see src/media/CMakeLists.txt

set(LIBDEFLATE_SOURCE_DIR ${CMAKE_BINARY_DIR}/third-party/libdeflate)

function(get_libdeflate)
    message(STATUS "Fetching libdeflate...")

    if(NOT EXISTS ${LIBDEFLATE_SOURCE_DIR}/libdeflate.h)
        find_package(Git REQUIRED)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch v1.25
                https://github.com/ebiggers/libdeflate.git
                ${LIBDEFLATE_SOURCE_DIR}
            RESULT_VARIABLE GIT_RESULT
            ERROR_VARIABLE GIT_ERROR
        )
        if(NOT GIT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to clone libdeflate repository: ${GIT_ERROR}")
        endif()
    endif()

    message(STATUS "Fetching libdeflate... done")
endfunction()

get_libdeflate()
