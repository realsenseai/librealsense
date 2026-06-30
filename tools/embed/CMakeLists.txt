# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021-2024 RealSense, Inc. All Rights Reserved.

cmake_minimum_required(VERSION 3.10)

project(RealsenseToolsEmbed)

# Save the command line compile commands in the build output
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

add_executable(rs-embed
   rs-embed.cpp
)

set_property(TARGET rs-embed PROPERTY CXX_STANDARD 11)

target_include_directories(rs-embed PRIVATE ../../third-party)
target_link_libraries( rs-embed PRIVATE tclap rs_lz4 )
set_target_properties (rs-embed PROPERTIES FOLDER Tools)


install(
    TARGETS
    rs-embed
    RUNTIME DESTINATION
    ${CMAKE_INSTALL_PREFIX}/bin
)
