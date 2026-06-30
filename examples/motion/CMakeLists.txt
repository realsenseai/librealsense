# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019-2024 RealSense, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.10)

project(RealsenseExamplesMotion)

if(BUILD_GRAPHICAL_EXAMPLES)
    add_executable(rs-motion rs-motion.cpp ../example.hpp ../../common/res/d435.h ${IMGUI_SOURCES})
    set_property(TARGET rs-motion PROPERTY CXX_STANDARD 11)
    target_link_libraries(rs-motion ${DEPENDENCIES} rs_lz4)
    include_directories(../ ../../common/res)
    set_target_properties (rs-motion PROPERTIES FOLDER Examples)
    install(TARGETS rs-motion RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
