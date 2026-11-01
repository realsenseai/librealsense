# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

target_sources(${LRS_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/nn-protocol.h"
        "${CMAKE_CURRENT_LIST_DIR}/nn-protocol.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/nn-loader.h"
        "${CMAKE_CURRENT_LIST_DIR}/nn-loader.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/nn-transport.h"
        "${CMAKE_CURRENT_LIST_DIR}/nn-transport.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/nn-transport-hwm.h"
        "${CMAKE_CURRENT_LIST_DIR}/nn-transport-hwm.cpp"
)
