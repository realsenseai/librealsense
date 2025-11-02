# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 RealSense, Inc. All Rights Reserved.
string(REPLACE ${PROJECT_SOURCE_DIR}/ "" _proc_rel_path ${CMAKE_CURRENT_LIST_DIR})

if (BUILD_WITH_CUDA)
    include(${_proc_rel_path}/cuda/CMakeLists.txt)
endif()

include(${_proc_rel_path}/sse/CMakeLists.txt)

include(${_proc_rel_path}/neon/CMakeLists.txt)

target_sources(${LRS_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/processing-blocks-factory.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/align.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/colorizer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/pointcloud.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/occlusion-filter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/synthetic-stream.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/syncer-processing-block.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/decimation-filter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/rectification-filter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/rotation-filter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/spatial-filter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/temporal-filter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/hdr-merge.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/sequence-id-filter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/hole-filling-filter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/disparity-transform.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/y8i-to-y8y8.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/y8i-to-y8y8-mipi.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/y12i-to-y16y16.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/y12i-to-y16y16-mipi.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/y16i-10msb-to-y16y16.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/y16-10msb-to-y16.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/identity-processing-block.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/threshold.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/rates-printer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/units-transform.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/rotation-transform.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/color-formats-converter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/depth-formats-converter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/motion-transform.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/auto-exposure-processor.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/y411-converter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/formats-converter.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/processing-blocks-factory.h"
        "${CMAKE_CURRENT_LIST_DIR}/align.h"
        "${CMAKE_CURRENT_LIST_DIR}/colorizer.h"
        "${CMAKE_CURRENT_LIST_DIR}/pointcloud.h"
        "${CMAKE_CURRENT_LIST_DIR}/occlusion-filter.h"
        "${CMAKE_CURRENT_LIST_DIR}/synthetic-stream.h"
        "${CMAKE_CURRENT_LIST_DIR}/decimation-filter.h"
        "${CMAKE_CURRENT_LIST_DIR}/rectification-filter.h"
        "${CMAKE_CURRENT_LIST_DIR}/rotation-filter.h"
        "${CMAKE_CURRENT_LIST_DIR}/spatial-filter.h"
        "${CMAKE_CURRENT_LIST_DIR}/temporal-filter.h"
        "${CMAKE_CURRENT_LIST_DIR}/hdr-merge.h"
        "${CMAKE_CURRENT_LIST_DIR}/sequence-id-filter.h"
        "${CMAKE_CURRENT_LIST_DIR}/hole-filling-filter.h"
        "${CMAKE_CURRENT_LIST_DIR}/syncer-processing-block.h"
        "${CMAKE_CURRENT_LIST_DIR}/disparity-transform.h"
        "${CMAKE_CURRENT_LIST_DIR}/y8i-to-y8y8.h"
        "${CMAKE_CURRENT_LIST_DIR}/y8i-to-y8y8-mipi.h"
        "${CMAKE_CURRENT_LIST_DIR}/y12i-to-y16y16.h"
        "${CMAKE_CURRENT_LIST_DIR}/y12i-to-y16y16-mipi.h"
        "${CMAKE_CURRENT_LIST_DIR}/y16i-10msb-to-y16y16.h"
        "${CMAKE_CURRENT_LIST_DIR}/y16-10msb-to-y16.h"
        "${CMAKE_CURRENT_LIST_DIR}/identity-processing-block.h"
        "${CMAKE_CURRENT_LIST_DIR}/threshold.h"
        "${CMAKE_CURRENT_LIST_DIR}/rates-printer.h"
        "${CMAKE_CURRENT_LIST_DIR}/units-transform.h"
        "${CMAKE_CURRENT_LIST_DIR}/rotation-transform.h"
        "${CMAKE_CURRENT_LIST_DIR}/color-formats-converter.h"
        "${CMAKE_CURRENT_LIST_DIR}/depth-formats-converter.h"
        "${CMAKE_CURRENT_LIST_DIR}/motion-transform.h"
        "${CMAKE_CURRENT_LIST_DIR}/auto-exposure-processor.h"
        "${CMAKE_CURRENT_LIST_DIR}/y411-converter.h"
        "${CMAKE_CURRENT_LIST_DIR}/formats-converter.h"
        "${CMAKE_CURRENT_LIST_DIR}/decimation-embedded-filter.h"
        "${CMAKE_CURRENT_LIST_DIR}/temporal-embedded-filter.h"
)
