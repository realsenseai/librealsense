# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 RealSense, Inc. All Rights Reserved.
target_sources(${LRS_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/cuda-conversion.cu"
        "${CMAKE_CURRENT_LIST_DIR}/cuda-conversion.cuh"
        "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.cu"
        "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.cuh"
        "${CMAKE_CURRENT_LIST_DIR}/cuda-frame-memory.cu"
        "${CMAKE_CURRENT_LIST_DIR}/cuda-frame-memory.h"
        "${CMAKE_CURRENT_LIST_DIR}/rscuda_utils.cuh"
)
