# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 RealSense, Inc. All Rights Reserved.
if(BUILD_WITH_HIP)
    target_sources(${LRS_TARGET}
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/cuda-conversion.hip"
            "${CMAKE_CURRENT_LIST_DIR}/cuda-conversion.cuh"
            "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.hip"
            "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.cuh"
            "${CMAKE_CURRENT_LIST_DIR}/rscuda_utils.cuh"
    )
    set_source_files_properties(
        "${CMAKE_CURRENT_LIST_DIR}/cuda-conversion.hip"
        "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.hip"
        PROPERTIES LANGUAGE HIP
    )
else()
    target_sources(${LRS_TARGET}
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/cuda-conversion.cu"
            "${CMAKE_CURRENT_LIST_DIR}/cuda-conversion.cuh"
            "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.cu"
            "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.cuh"
            "${CMAKE_CURRENT_LIST_DIR}/rscuda_utils.cuh"
    )
endif()
