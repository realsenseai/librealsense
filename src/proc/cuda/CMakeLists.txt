# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 RealSense, Inc. All Rights Reserved.
if(BUILD_WITH_HIP)
    target_sources(${LRS_TARGET}
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/cuda-align.h"
            "${CMAKE_CURRENT_LIST_DIR}/cuda-align.hip"
            "${CMAKE_CURRENT_LIST_DIR}/cuda-align.cuh"
            "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.h"
            "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.cpp"
    )
    set_source_files_properties(
        "${CMAKE_CURRENT_LIST_DIR}/cuda-align.hip"
        PROPERTIES LANGUAGE HIP
    )
else()
    target_sources(${LRS_TARGET}
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/cuda-align.h"
            "${CMAKE_CURRENT_LIST_DIR}/cuda-align.cu"
            "${CMAKE_CURRENT_LIST_DIR}/cuda-align.cuh"
            "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.h"
            "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.cpp"
    )
endif()
