# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.10)

project(mem-leak-test)

add_executable( ${PROJECT_NAME} mem-leak-test.cpp)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)
target_link_libraries(  ${PROJECT_NAME} ${DEPENDENCIES} )
# Load the ELPP storage of the executable, realsense2 and realsense2-gl together, as the viewer does
using_easyloggingpp( ${PROJECT_NAME} SHARED )
if( TARGET realsense2-gl AND UNIX AND NOT APPLE )
    target_link_libraries( ${PROJECT_NAME} -Wl,--no-as-needed realsense2-gl -Wl,--as-needed )
endif()
set_target_properties(  ${PROJECT_NAME} PROPERTIES FOLDER "Examples" )
install( TARGETS  ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )

