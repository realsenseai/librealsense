# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022-2024 RealSense, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.10)
project( pyrealdds )

set( PYREALDDS_FILES
    pyrealdds.cpp
    )

pybind11_add_module( ${PROJECT_NAME} MODULE ${PYREALDDS_FILES} )

target_link_libraries( ${PROJECT_NAME} PRIVATE realdds )
set_target_properties( ${PROJECT_NAME}
    PROPERTIES
        VERSION ${REALSENSE_VERSION_STRING}
        SOVERSION "${REALSENSE_VERSION_MAJOR}.${REALSENSE_VERSION_MINOR}"
    )
set_target_properties( ${PROJECT_NAME}
    PROPERTIES
    FOLDER Library/Python
    )

using_easyloggingpp( ${PROJECT_NAME} )

install(
    TARGETS ${PROJECT_NAME}
    EXPORT pyrealsense2Targets
    LIBRARY DESTINATION ${PYTHON_INSTALL_DIR}
    )
