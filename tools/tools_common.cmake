# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#
# Shared build configuration for graphical tool targets (realsense-viewer, rs-depth-quality, realsense-viewer-tests)
# Expects the caller to set RS_VIEWER_LIBS and (on Windows) LD_FLAGS_STR before calling this macro.

macro(tools_target_config _target)
    # Link OpenGL (all platforms) and libdl (linux)
    target_link_libraries(${_target} OpenGL::GL)
    using_easyloggingpp(${_target} SHARED)
    target_include_directories(${_target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    if(WIN32)
        target_sources(${_target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/res/resource.h
            ${CMAKE_CURRENT_SOURCE_DIR}/res/realsense-app.rc)
        set_target_properties(${_target} PROPERTIES LINK_FLAGS "/ignore:4199 ${LD_FLAGS_STR}")
    else()
        target_link_libraries(${_target} dl)
    endif()
    if(CHECK_FOR_UPDATES)
        message( STATUS "Check for updates capability added to ${_target}" )
        add_dependencies(${_target} libcurl)
        set(RS_VIEWER_LIBS ${RS_VIEWER_LIBS} curl)
    endif()
    target_link_libraries(${_target} ${DEPENDENCIES} ${RS_VIEWER_LIBS} tclap)
    set_target_properties(${_target} PROPERTIES CXX_STANDARD 11 FOLDER Tools)
    # Apple + DDS: install tree is bin/ + lib/; never bake build-tree paths.
    if(APPLE AND BUILD_WITH_DDS)
        set_target_properties(${_target} PROPERTIES
            BUILD_RPATH "@loader_path;${CMAKE_BINARY_DIR}/Release;${CMAKE_BINARY_DIR}"
            INSTALL_RPATH "@loader_path;@loader_path/../lib"
            INSTALL_RPATH_USE_LINK_PATH FALSE
            BUILD_WITH_INSTALL_RPATH FALSE
            MACOSX_RPATH ON)
    endif()
endmacro()
