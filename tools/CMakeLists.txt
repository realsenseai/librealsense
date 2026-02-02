# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019-2024 RealSense, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.10)

# Save the command line compile commands in the build output
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
# View the makefile commands during build
#set(CMAKE_VERBOSE_MAKEFILE on)

set_security_flags_for_executable() # tools are executables so we want position indepandent executables and not libraries

list( APPEND DEPENDENCIES ${LRS_TARGET} tclap )
# Add all imgui cpp source files and store the list in the IMGUI_SOURCES variable
file(GLOB IMGUI_SOURCES "../third-party/imgui/*.cpp") 

if(BUILD_TOOLS)
    add_subdirectory(convert)
    add_subdirectory(enumerate-devices)
    add_subdirectory(fw-logger)
    add_subdirectory(terminal)
    add_subdirectory(recorder)
    add_subdirectory(fw-update)
    add_subdirectory(embed)
    if(BUILD_WITH_DDS)
        add_subdirectory(dds)
    endif()
endif()

if(BUILD_EXAMPLES)
    if(BUILD_GRAPHICAL_EXAMPLES)
        include(${PROJECT_SOURCE_DIR}/CMake/opengl_config.cmake)
        if (NOT BUILD_GLSL_EXTENSIONS)
            MESSAGE(STATUS "BUILD_GRAPHICAL_EXAMPLES explicitely depends on BUILD_GLSL_EXTENSIONS, set it ON")
            SET(BUILD_GLSL_EXTENSIONS ON)
        endif()
        set(DEPENDENCIES ${DEPENDENCIES} realsense2-gl)
        add_subdirectory(data-collect)
        if(NOT APPLE)
            add_subdirectory(realsense-viewer)
        endif()
        add_subdirectory(depth-quality)
        add_subdirectory(rosbag-inspector)
        add_subdirectory(benchmark)
    else()
        if(ANDROID_NDK_TOOLCHAIN_INCLUDED)
            find_library(log-lib log)
            add_dependencies(RealsenseTools log)
            set(DEPENDENCIES ${DEPENDENCIES} log)
        #else()
        #    set(DEPENDENCIES realsense2)
        endif()
    endif()
endif()

unset_security_flags_for_executable()
