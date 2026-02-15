# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#
# Fetch and build FastCDR library for CDR serialization support
# This is used for ROS2 bag file read/write without requiring full DDS support
# Uses configure-time git clone + add_subdirectory (compatible with CMake 3.10+)

set(FASTCDR_SOURCE_DIR ${CMAKE_BINARY_DIR}/third-party/fastcdr)

function(get_fastcdr_only)
    message(STATUS "Fetching fastcdr...")

    if(NOT EXISTS ${FASTCDR_SOURCE_DIR}/CMakeLists.txt)
        find_package(Git REQUIRED)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch v1.0.25
                https://github.com/eProsima/Fast-CDR.git
                ${FASTCDR_SOURCE_DIR}
            RESULT_VARIABLE GIT_RESULT
        )
        if(NOT GIT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to clone FastCDR repository")
        endif()
    endif()

    # Set special values for fastcdr build
    set(BUILD_SHARED_LIBS OFF)

    add_subdirectory(${FASTCDR_SOURCE_DIR} ${CMAKE_BINARY_DIR}/third-party/fastcdr-build EXCLUDE_FROM_ALL)

    # Place fastcdr with other 3rd-party projects
    set_target_properties(fastcdr PROPERTIES FOLDER "3rd Party")

    # Make sure fastcdr include directories are available
    target_include_directories(fastcdr INTERFACE
        $<BUILD_INTERFACE:${FASTCDR_SOURCE_DIR}/include>
    )

    message(STATUS "Fetching fastcdr... done")
endfunction()

# Only fetch fastcdr if not already available (i.e., BUILD_WITH_DDS is OFF)
if(NOT TARGET fastcdr)
    get_fastcdr_only()
endif()
