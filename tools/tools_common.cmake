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
            ${CMAKE_CURRENT_SOURCE_DIR}/res/realsense-viewer.rc)
        set_target_properties(${_target} PROPERTIES LINK_FLAGS "/ignore:4199 ${LD_FLAGS_STR}")
    else()
        target_link_libraries(${_target} dl)
    endif()
    if(CHECK_FOR_UPDATES)
        message( STATUS "Check for updates capability added to ${_target}" )
        add_dependencies(${_target} libcurl)
        set(RS_VIEWER_LIBS ${RS_VIEWER_LIBS} curl)
    endif()
    if(IMPORT_DEPTH_CAM_FW)
        add_definitions(-DINTERNAL_FW)
        target_link_libraries(${_target} fw)
    endif()
    target_link_libraries(${_target} ${DEPENDENCIES} ${RS_VIEWER_LIBS} tclap)
    set_target_properties(${_target} PROPERTIES CXX_STANDARD 11 FOLDER Tools)
endmacro()
