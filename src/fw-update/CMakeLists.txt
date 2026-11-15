# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 RealSense, Inc. All Rights Reserved.
target_sources(${LRS_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/dfu-write.h"
        "${CMAKE_CURRENT_LIST_DIR}/dfu-write.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/fw-update-device-interface.h"
        "${CMAKE_CURRENT_LIST_DIR}/fw-update-device.h"
        "${CMAKE_CURRENT_LIST_DIR}/fw-update-device.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/fw-update-factory.h"
        "${CMAKE_CURRENT_LIST_DIR}/fw-update-factory.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/fw-update-unsigned.h"
        "${CMAKE_CURRENT_LIST_DIR}/fw-update-unsigned.cpp"
)
