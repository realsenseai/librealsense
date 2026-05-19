# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019-2024 RealSense, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.10)

project(RealsenseToolsRosbagInspector)


if(BUILD_GRAPHICAL_EXAMPLES)

    set(RS_ROSBAG_INSPECTOR_CPP
            rs-rosbag-inspector.cpp
            files_container.h
            print_helpers.h
            rosbag_content.h
            ../../common/os.h
            ../../common/os.cpp
            ../../third-party/glad/glad.c
            ${IMGUI_SOURCES}
            ../../third-party/tinyfiledialogs/tinyfiledialogs.c
            ../../third-party/tinyfiledialogs/tinyfiledialogs.h
        )

    if(WIN32)
        add_executable(rs-rosbag-inspector WIN32 ${RS_ROSBAG_INSPECTOR_CPP})
        include_directories(../../third-party/imgui ../../common ../../third-party/glad
                                             ../../third-party/tinyfiledialogs ../../third-party ${CMAKE_CURRENT_SOURCE_DIR}/res/)
        target_link_libraries(rs-rosbag-inspector realsense-file OpenGL::GL)
    else()
        add_executable(rs-rosbag-inspector ${RS_ROSBAG_INSPECTOR_CPP})
        include_directories(../../third-party/imgui ../../common ../../third-party/glad
                                             ../../third-party/tinyfiledialogs ../../third-party)
        target_link_libraries(rs-rosbag-inspector realsense-file OpenGL::GL dl) # Check the platform and conditionally link OpenGL and libdl (for linux)
    endif()
    set_property(TARGET rs-rosbag-inspector PROPERTY CXX_STANDARD 11)

    target_link_libraries(rs-rosbag-inspector realsense-file ${DEPENDENCIES})

    set_target_properties (rs-rosbag-inspector PROPERTIES
        FOLDER Tools
    )

    using_easyloggingpp( rs-rosbag-inspector SHARED )

    install(
        TARGETS

        rs-rosbag-inspector

        RUNTIME DESTINATION
        ${CMAKE_INSTALL_BINDIR}
    )
endif()
