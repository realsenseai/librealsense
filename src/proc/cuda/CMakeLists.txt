# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

if(BUILD_WITH_HIP)
    set(RS_GPU_ALIGN_SOURCE "${CMAKE_CURRENT_LIST_DIR}/cuda-align.hip")
    set_source_files_properties(
        "${RS_GPU_ALIGN_SOURCE}"
        PROPERTIES LANGUAGE HIP
    )
else()
    set(RS_GPU_ALIGN_SOURCE "${CMAKE_CURRENT_LIST_DIR}/cuda-align.cu")
endif()

target_sources(${LRS_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/cuda-align.h"
        "${RS_GPU_ALIGN_SOURCE}"
        "${CMAKE_CURRENT_LIST_DIR}/cuda-align.cuh"
        "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.h"
        "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.cpp"
)