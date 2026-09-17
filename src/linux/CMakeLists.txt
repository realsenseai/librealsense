# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

include(${CMAKE_CURRENT_LIST_DIR}/find_udev.cmake)

# Exported to the parent scope so wrappers/python can build the legacy pybackend2 target from the
# same list instead of repeating a subset of it.
set(LRS_V4L2_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/backend-v4l2.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/backend-hid.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-ioctl.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-kernel-buffers.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-video-md-syncer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-frame-drop-monitor.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-enumerator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-enumerator-usb.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-enumerator-mipi.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-uvc-device.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-uvc-meta-device.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-mipi-device.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-product-quirks.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-named-mutex.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/camera-identifier-v4l.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-mipi-logic.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-usb-logic.cpp"
)
set(LRS_V4L2_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/backend-v4l2.h"
    "${CMAKE_CURRENT_LIST_DIR}/backend-hid.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-ioctl.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-kernel-buffers.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-video-md-syncer.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-frame-drop-monitor.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-enumerator.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-enumerator-usb.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-enumerator-mipi.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-uvc-device.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-uvc-meta-device.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-mipi-device.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-product-quirks.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-node-info.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-named-mutex.h"
    "${CMAKE_CURRENT_LIST_DIR}/camera-identifier-v4l.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-mipi-logic.h"
    "${CMAKE_CURRENT_LIST_DIR}/v4l-usb-logic.h"
)
set(LRS_V4L2_LINK_LIBS usb)
set(LRS_V4L2_INCLUDE_DIRS "")

if(UDEV_FOUND)
    list(APPEND LRS_V4L2_SOURCES "${CMAKE_CURRENT_LIST_DIR}/udev-device-watcher.cpp")
    list(APPEND LRS_V4L2_HEADERS "${CMAKE_CURRENT_LIST_DIR}/udev-device-watcher.h")
    list(APPEND LRS_V4L2_LINK_LIBS udev)
    list(APPEND LRS_V4L2_INCLUDE_DIRS ${UDEV_INCLUDE_DIRS})
else()
    message(STATUS "UDEV not found; using polling device-watcher!")
endif()

target_sources(${LRS_TARGET}
    PRIVATE
        ${LRS_V4L2_SOURCES}
        ${LRS_V4L2_HEADERS}
)

include(libusb_config)
target_link_libraries(${LRS_TARGET} PRIVATE ${LRS_V4L2_LINK_LIBS})
target_include_directories(${LRS_TARGET} PRIVATE ${LRS_V4L2_INCLUDE_DIRS})
if(USE_EXTERNAL_USB)
    add_dependencies(${LRS_TARGET} libusb)
endif()

if(UDEV_FOUND)
    add_definitions(-DUSING_UDEV)
endif()
