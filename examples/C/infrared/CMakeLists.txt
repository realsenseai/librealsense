# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.10)

project(RealsenseExamples-Infrared)

# Save the command line compile commands in the build output
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

add_executable(rs-infrared rs-infrared.c ../example.h)
include_directories(../../C)
target_link_libraries(rs-infrared ${DEPENDENCIES})
set_target_properties (rs-infrared PROPERTIES
FOLDER "Examples/C"
)

install(TARGETS rs-infrared RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})