# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019-2024 RealSense, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.10)

# Save the command line compile commands in the build output
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
# View the makefile commands during build
#set(CMAKE_VERBOSE_MAKEFILE on)

set_security_flags_for_executable() # examples are executables so we want position independent executables and not libraries

set( DEPENDENCIES ${LRS_TARGET} )
if(BUILD_GRAPHICAL_EXAMPLES)
    include(${PROJECT_SOURCE_DIR}/CMake/opengl_config.cmake)
else()
    if(ANDROID_NDK_TOOLCHAIN_INCLUDED)
        find_library(log-lib log)
        list( APPEND DEPENDENCIES log )
    endif()
endif()
# Check the platform and conditionally link OpenGL and libdl (for linux)
if(WIN32)
    list( APPEND DEPENDENCIES OpenGL::GL)
else()
    list( APPEND DEPENDENCIES OpenGL::GL dl)
endif()

# Add all imgui cpp source files and store the list in the IMGUI_SOURCES variable
file(GLOB IMGUI_SOURCES "../third-party/imgui/*.cpp") 

# We need tclap for command-line interface
list(APPEND DEPENDENCIES tclap)

add_subdirectory(hello-realsense)
add_subdirectory(software-device)
add_subdirectory(capture)
add_subdirectory(callback)
add_subdirectory(save-to-disk)
add_subdirectory(multicam)
add_subdirectory(pointcloud)
add_subdirectory(labeledpointcloud)
add_subdirectory(align)
add_subdirectory(align-gl)
add_subdirectory(align-advanced)
add_subdirectory(sensor-control)
add_subdirectory(measure)
add_subdirectory(C/depth)
add_subdirectory(C/color)
add_subdirectory(C/distance)
add_subdirectory(C/infrared)
add_subdirectory(post-processing)
add_subdirectory(record-playback)
add_subdirectory(motion)
add_subdirectory(gl)
add_subdirectory(hdr)
add_subdirectory(on-chip-calib)
add_subdirectory(eth-config)
add_subdirectory(embedded-filters)

unset_security_flags_for_executable()
