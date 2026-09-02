# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

if(BUILD_WITH_HIP)
    set(RS_GPU_CONVERSION_SOURCE "${CMAKE_CURRENT_LIST_DIR}/cuda-conversion.hip")
    set(RS_GPU_POINTCLOUD_SOURCE "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.hip")
    # cuda-rggb.cu has __global__/__device__ kernel code (the D401 GMSL dual-RGB debayer +
    # remap kernels), same as conversion/pointcloud above, so it needs the same ".hip"
    # sibling + LANGUAGE HIP treatment -- NOT the LANGUAGE CXX / "-x c++" trick used for the
    # kernel-free cuda-frame-memory.cu below (that trick can't parse __global__/<<<>>>).
    set(RS_GPU_RGGB_SOURCE "${CMAKE_CURRENT_LIST_DIR}/cuda-rggb.hip")
    set_source_files_properties(
        "${RS_GPU_CONVERSION_SOURCE}"
        "${RS_GPU_POINTCLOUD_SOURCE}"
        "${RS_GPU_RGGB_SOURCE}"
        PROPERTIES LANGUAGE HIP
    )
else()
    set(RS_GPU_CONVERSION_SOURCE "${CMAKE_CURRENT_LIST_DIR}/cuda-conversion.cu")
    set(RS_GPU_POINTCLOUD_SOURCE "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.cu")
    set(RS_GPU_RGGB_SOURCE "${CMAKE_CURRENT_LIST_DIR}/cuda-rggb.cu")
endif()

# cuda-frame-memory.cu has no __global__/__device__ kernel code -- it is host-only runtime
# API calls (hipMalloc/hipMemcpy/hipHostAlloc/etc., see the RS2_USE_HIP branch inside it), so
# unlike the conversion/pointcloud kernels above it does not need a HIP-language-tagged
# ".hip" sibling file or a LANGUAGE HIP source property: plain C++ compilation is enough for
# both backends, it just needs the right runtime header/macros, which the file selects
# itself via RS2_USE_HIP. Previously only ever added for the CUDA (non-HIP) branch, which
# left rs_frame_gpu_upload()/rs_frame_gpu_free() (unconditionally referenced from
# src/frame.h/.cpp whenever RS2_USE_CUDA is defined -- true for HIP builds too) undefined
# at link time for any BUILD_WITH_HIP target.
set(RS_GPU_EXTRA_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/cuda-frame-memory.cu"
    "${CMAKE_CURRENT_LIST_DIR}/cuda-frame-memory.h"
)

if(BUILD_WITH_HIP)
    # Under BUILD_WITH_CUDA, enable_language(CUDA) is on, so the ".cu" extension already
    # auto-maps to the CUDA language (nvcc) -- fine, since nvcc happily compiles this
    # kernel-free, host-only-API file too. Under BUILD_WITH_HIP, CUDA is never enabled as a
    # CMake language, so ".cu" has no automatic language mapping; force plain C++ instead
    # of leaving it to fail at generate time.
    # LANGUAGE CXX alone is not enough here: since the CUDA language is never enabled in a
    # HIP-only configure, CMake has no "this extension normally means CUDA, use -x to
    # override" rule to fall back on for ".cu", so the compile driver (g++) sees an
    # unrecognized extension and silently treats the file as a pre-built link object
    # instead of compiling it. Force the C++ front-end explicitly with -x c++.
    set_source_files_properties(
        "${CMAKE_CURRENT_LIST_DIR}/cuda-frame-memory.cu"
        PROPERTIES LANGUAGE CXX COMPILE_OPTIONS "-x;c++"
    )
endif()

target_sources(${LRS_TARGET}
    PRIVATE
        "${RS_GPU_CONVERSION_SOURCE}"
        "${CMAKE_CURRENT_LIST_DIR}/cuda-conversion.cuh"
        "${RS_GPU_POINTCLOUD_SOURCE}"
        "${CMAKE_CURRENT_LIST_DIR}/cuda-pointcloud.cuh"
        "${CMAKE_CURRENT_LIST_DIR}/cuda-compat.h"
        "${RS_GPU_RGGB_SOURCE}"
        "${CMAKE_CURRENT_LIST_DIR}/cuda-rggb.cuh"
        "${CMAKE_CURRENT_LIST_DIR}/rscuda_utils.cuh"
        ${RS_GPU_EXTRA_SOURCES}
)