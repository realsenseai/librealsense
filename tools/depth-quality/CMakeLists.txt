# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019-2024 RealSense, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.10)

project( rs-depth-quality )

if(WIN32)
    add_executable( ${PROJECT_NAME} WIN32 "" )
else()
    add_executable( ${PROJECT_NAME} "" )
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(${CMAKE_BINARY_DIR})

include(../tools_common.cmake)

include(../../common/CMakeLists.txt)

SET(DELAYED
    realsense2d.dll
    realsense2-gld.dll
    realsense2.dll
    realsense2-gl.dll
)

if(BUILD_GRAPHICAL_EXAMPLES)
    set(RS_QUALITY_TOOL_CPP
        ${COMMON_SRC}
        rs-depth-quality.cpp
        depth-quality-model.h
        depth-quality-model.cpp
        depth-metrics.h
        ../../common/realsense-ui-advanced-mode.h
        ${IMGUI_SOURCES}
        ../../third-party/glad/glad.c
        ../../third-party/tinyfiledialogs/tinyfiledialogs.c
        ../../third-party/tinyfiledialogs/tinyfiledialogs.h
        ../../third-party/rsutils/include/rsutils/type/eth-config.h
        ../../common/graph-model.h
        ../../common/graph-model.cpp
        ../../third-party/implot/implot.cpp
        ../../third-party/implot/implot.h
        ../../third-party/implot/implot_internal.h
        ../../third-party/implot/implot_items.cpp
        )

    if(WIN32)
        target_sources( ${PROJECT_NAME}
            PRIVATE
                ${RS_QUALITY_TOOL_CPP}
                ../../common/windows-app-bootstrap.cpp
            )

        source_group("Resources" FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/res/resource.h
                ${CMAKE_CURRENT_SOURCE_DIR}/res/realsense-app.rc)

        source_group("3rd Party" FILES
            ../../third-party/tinyfiledialogs/tinyfiledialogs.c
            ../../third-party/tinyfiledialogs/tinyfiledialogs.h
            ../../third-party/imgui/imgui.cpp
            ../../third-party/imgui/imgui_draw.cpp
            ../../third-party/imgui/imgui_impl_glfw.cpp
            ../../third-party/imgui/imgui_impl_opengl3.cpp
            ../../third-party/imgui/imgui-fonts-karla.hpp
            ../../third-party/imgui/imgui-fonts-fontawesome.hpp)

        source_group("Utils" FILES
            depth-quality-model.h
            depth-quality-model.cpp
            depth-metrics.h
            ${COMMON_SRC})

        include_directories(../../common  ../../third-party
                            ../../third-party/imgui
                            ../../third-party/implot
                            ../../third-party/glad
                            ../../third-party/tinyfiledialogs
                            ${CMAKE_CURRENT_SOURCE_DIR}/res/)

        #list(TRANSFORM DELAYED PREPEND " /DELAYLOAD:")
        PREPEND(DELAYED " /DELAYLOAD:" ${DELAYED})
        #infoValue(DELAYED)
        string(REPLACE ";" " " LD_FLAGS_STR "${DELAYED}")

    else()
        target_sources( ${PROJECT_NAME}
            PRIVATE
                ${RS_QUALITY_TOOL_CPP}
            )

        include_directories(../../common  ../../third-party
                            ../../third-party/imgui
                            ../../third-party/implot
                            ../../third-party/glad
                            ../../third-party/tinyfiledialogs )
    endif()

    source_group("SW-Update" FILES ${SW_UPDATE_FILES})

    set(RS_VIEWER_LIBS ${GTK3_LIBRARIES} Threads::Threads realsense2-gl)
    tools_target_config(${PROJECT_NAME})

    install(
        TARGETS

        ${PROJECT_NAME}

        RUNTIME DESTINATION
        ${CMAKE_INSTALL_BINDIR}
    )
endif()
