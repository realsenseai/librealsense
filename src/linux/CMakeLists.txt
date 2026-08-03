# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 RealSense, Inc. All Rights Reserved.
target_sources(${LRS_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/backend-v4l2.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/backend-hid.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/backend-v4l2.h"
        "${CMAKE_CURRENT_LIST_DIR}/backend-hid.h"
        "${CMAKE_CURRENT_LIST_DIR}/camera-identifier-v4l.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/camera-identifier-v4l.h"
        "${CMAKE_CURRENT_LIST_DIR}/v4l-mipi-logic.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/v4l-mipi-logic.h"
        "${CMAKE_CURRENT_LIST_DIR}/v4l-usb-logic.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/v4l-usb-logic.h"
)

include(libusb_config)
target_link_libraries(${LRS_TARGET} PRIVATE usb)
if(USE_EXTERNAL_USB)
    add_dependencies(${LRS_TARGET} libusb)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/find_udev.cmake)
if(UDEV_FOUND)
    target_sources(${LRS_TARGET}
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/udev-device-watcher.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/udev-device-watcher.h"
    )
    target_link_libraries(${LRS_TARGET} PRIVATE udev)
    target_include_directories(${LRS_TARGET} PRIVATE ${UDEV_INCLUDE_DIRS})
    add_definitions(-DUSING_UDEV)
else()
    message(STATUS "UDEV not found; using polling device-watcher!")
endif()
