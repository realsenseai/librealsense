# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 RealSense, Inc. All Rights Reserved.
target_sources(${LRS_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/hid-types.h"
        "${CMAKE_CURRENT_LIST_DIR}/hid-device.h"
        "${CMAKE_CURRENT_LIST_DIR}/hid-device.cpp"
)
