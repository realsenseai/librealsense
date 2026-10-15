# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 RealSense, Inc. All Rights Reserved.
target_sources(${LRS_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/win-helpers.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/h264-decoder-mf.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/win-helpers.h"
)
