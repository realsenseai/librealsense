# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019-2024 RealSense, Inc. All Rights Reserved.

cmake_minimum_required(VERSION 3.10)

project(realsense2-gl)

set(REALSENSE_GL_CPP
    synthetic-stream-gl.h
    synthetic-stream-gl.cpp
    yuy2rgb-gl.h
    yuy2rgb-gl.cpp
    m420-to-rgb-gl.h
    m420-to-rgb-gl.cpp
    nv12-to-rgb-gl.h
    nv12-to-rgb-gl.cpp
    y4112rgb-gl.h
    y4112rgb-gl.cpp
    pointcloud-gl.h
    pointcloud-gl.cpp
    rs-gl.cpp
    realsense-gl.def
    ../../include/librealsense2-gl/rs_processing_gl.h
    ../../include/librealsense2-gl/rs_processing_gl.hpp
    ../../third-party/glad/glad.c
    ../../common/opengl3.cpp
    ../../common/opengl3.h
    pc-shader.cpp
    pc-shader.h
    camera-shader.cpp
    camera-shader.h
    upload.h
    upload.cpp
    colorizer-gl.h
    colorizer-gl.cpp
    align-gl.h
    align-gl.cpp
)

set(REALSENSE_GL_PUBLIC_HEADERS
    ../../include/librealsense2-gl/rs_processing_gl.h
    ../../include/librealsense2-gl/rs_processing_gl.hpp
)

set(CMAKECONFIG_GL_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${LRS_GL_TARGET}")

set( DEPENDENCIES ${LRS_TARGET} )
include(${CMAKE_CURRENT_SOURCE_DIR}/../../CMake/opengl_config.cmake)

if (${BUILD_SHARED_LIBS} AND ${BUILD_EASYLOGGINGPP})
    list(APPEND REALSENSE_GL_CPP ../../third-party/easyloggingpp/src/easylogging++.cc)
endif()

source_group("3rd Party" FILES
    ../../third-party/glad/glad.c)

source_group("API" FILES
    ../../include/librealsense2-gl/rs_processing_gl.h
    ../../include/librealsense2-gl/rs_processing_gl.hpp
    realsense-gl.def)

add_library(${LRS_GL_TARGET} ${REALSENSE_GL_CPP})

if(BUILD_SHARED_LIBS)
    add_definitions(-DSHARED_LIBS)
endif()

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/..
        ${CMAKE_CURRENT_LIST_DIR}/../../third-party/glad
        ${CMAKE_CURRENT_LIST_DIR}/../../common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER Library)
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${REALSENSE_GL_PUBLIC_HEADERS}")
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${REALSENSE_VERSION_STRING} SOVERSION "${REALSENSE_VERSION_MAJOR}.${REALSENSE_VERSION_MINOR}")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/realsense2-glConfigVersion.cmake"
    VERSION ${REALSENSE_VERSION_STRING} COMPATIBILITY AnyNewerVersion)

configure_package_config_file(../../CMake/realsense2-glConfig.cmake.in realsense2-glConfig.cmake
    INSTALL_DESTINATION ${CMAKECONFIG_GL_INSTALL_DIR}
    INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/bin
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR
)

configure_file(../../config/librealsense-gl.pc.in ../../config/realsense2-gl.pc @ONLY)

target_link_libraries( ${PROJECT_NAME}
    PRIVATE
        ${DEPENDENCIES}
        rs_lz4
    )

# CMake prior to 3.13 cannot install targets located in sub-directories.
#https://gitlab.kitware.com/cmake/cmake/merge_requests/2152
# Hence these targets will be invoked from the project and not the root directory
install(TARGETS ${LRS_GL_TARGET}
    EXPORT realsense2-glTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_PREFIX}/include/librealsense2-gl"
)

install(EXPORT realsense2-glTargets
    FILE realsense2-glTargets.cmake
    NAMESPACE ${LRS_GL_TARGET}::
    DESTINATION ${CMAKECONFIG_GL_INSTALL_DIR}
)

install(FILES "${CMAKE_BINARY_DIR}/src/gl/realsense2-glConfig.cmake"
    DESTINATION ${CMAKECONFIG_GL_INSTALL_DIR}
)

install(FILES "${CMAKE_BINARY_DIR}/src/gl/realsense2-glConfigVersion.cmake"
        DESTINATION ${CMAKECONFIG_GL_INSTALL_DIR}
)

# Set library pkgconfig file for facilitating 3rd party integration
install(FILES "${CMAKE_BINARY_DIR}/config/realsense2-gl.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

install(CODE "execute_process(COMMAND ldconfig)")
