# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Always compiled; ENABLED_STATS gates behavior (hook bodies and the rs2_rum_* C-API), not the
# build. When off, the hooks no-op and the collector/config are simply never invoked.
target_sources(${LRS_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/rum-config.h"
        "${CMAKE_CURRENT_LIST_DIR}/rum-config.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/rum-collector.h"
        "${CMAKE_CURRENT_LIST_DIR}/rum-collector.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/rum-hooks.h"
        "${CMAKE_CURRENT_LIST_DIR}/rum-hooks.cpp"
)
