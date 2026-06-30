# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.10)

project( rs-object-detection )

add_executable( ${PROJECT_NAME} rs-object-detection.cpp )
set_property( TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11 )
target_link_libraries( ${PROJECT_NAME} ${DEPENDENCIES} )
set_target_properties( ${PROJECT_NAME} PROPERTIES FOLDER "Examples" )

using_easyloggingpp( ${PROJECT_NAME} SHARED )

install( TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )
