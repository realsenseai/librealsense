# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 RealSense, Inc. All Rights Reserved.
string(REPLACE ${PROJECT_SOURCE_DIR}/ "" _rel_path ${CMAKE_CURRENT_LIST_DIR})
include(${_rel_path}/core/CMakeLists.txt)
include(${_rel_path}/ds/CMakeLists.txt)
include(${_rel_path}/media/CMakeLists.txt)
include(${_rel_path}/proc/CMakeLists.txt)
include(${_rel_path}/res/CMakeLists.txt)
include(${_rel_path}/pipeline/CMakeLists.txt)
include(${_rel_path}/usb/CMakeLists.txt)
include(${_rel_path}/fw-logs/CMakeLists.txt)
include(${_rel_path}/fw-update/CMakeLists.txt)

message(STATUS "using ${BACKEND}")

if(UNIX OR (ANDROID_NDK_TOOLCHAIN_INCLUDED AND (${BACKEND} STREQUAL RS2_USE_V4L2_BACKEND)))
    include(${_rel_path}/libusb/CMakeLists.txt)
endif()

if(WIN32)
    include(${_rel_path}/win/CMakeLists.txt)
    include(${_rel_path}/winusb/CMakeLists.txt)
endif()

if(${BACKEND} STREQUAL RS2_USE_ANDROID_BACKEND)
    include(${_rel_path}/usbhost/CMakeLists.txt)
    include(${_rel_path}/android/CMakeLists.txt)
endif()

if(${BACKEND} STREQUAL RS2_USE_V4L2_BACKEND)
    include(${_rel_path}/linux/CMakeLists.txt)
endif()

if(${BACKEND} STREQUAL RS2_USE_WMF_BACKEND)
    include(${_rel_path}/mf/CMakeLists.txt)
endif()

if(${BACKEND} STREQUAL RS2_USE_WINUSB_UVC_BACKEND)
    include(${_rel_path}/win7/CMakeLists.txt)
endif()

if(${BACKEND} STREQUAL RS2_USE_LIBUVC_BACKEND OR ${BACKEND} STREQUAL RS2_USE_ANDROID_BACKEND OR ${BACKEND} STREQUAL RS2_USE_WINUSB_UVC_BACKEND)
    include(${_rel_path}/hid/CMakeLists.txt)
    include(${_rel_path}/uvc/CMakeLists.txt)
    include(${_rel_path}/rsusb-backend/CMakeLists.txt)
endif()

if(${BACKEND} STREQUAL RS2_USE_LIBUVC_BACKEND)
    include(${_rel_path}/libuvc/CMakeLists.txt)
endif()

if(BUILD_WITH_CUDA)
    include(${_rel_path}/cuda/CMakeLists.txt)
endif()

if(LRS_TRY_USE_AVX)
    set_source_files_properties(image-avx.cpp PROPERTIES COMPILE_FLAGS -mavx2)
endif()

if(BUILD_SHARED_LIBS)
    target_sources(${LRS_TARGET} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/realsense.def")
endif()

target_sources(${LRS_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/algo.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/archive.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/backend.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/backend-device-factory.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/backend-device-factory.h"
        "${CMAKE_CURRENT_LIST_DIR}/context.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/device.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/device-info.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/device_hub.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/rscore/device-factory.h"
        "${CMAKE_CURRENT_LIST_DIR}/environment.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/error-handling.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/firmware_logger_device.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/global_timestamp_reader.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/hdr-config.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/hw-monitor.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/image.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/image-avx.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/log.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/option.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/platform-camera.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/rs.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/sensor.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/hid-sensor.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/uvc-sensor.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/rscore-pp-block-factory.h"
        "${CMAKE_CURRENT_LIST_DIR}/rscore-pp-block-factory.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/software-device.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/software-device-info.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/software-sensor.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/stream.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/sync.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/terminal-parser.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/types.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/verify.c"
        "${CMAKE_CURRENT_LIST_DIR}/serialized-utilities.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/frame.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/points.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/labeled-points.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/object-detection-frame.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/to-string.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/eth-config-device.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/algo.h"
        "${CMAKE_CURRENT_LIST_DIR}/api.h"
        "${CMAKE_CURRENT_LIST_DIR}/archive.h"
        "${CMAKE_CURRENT_LIST_DIR}/backend.h"
        "${CMAKE_CURRENT_LIST_DIR}/backend-device.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/backend-device-group.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/platform-device-info.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/device-watcher.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/frame-object.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/hid-data.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/hid-device.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/hid-device-info.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/mipi-device.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/mipi-device-info.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/playback-device-info.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/platform-utils.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/platform-utils.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/platform/stream-profile.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/stream-profile-impl.h"
        "${CMAKE_CURRENT_LIST_DIR}/core/time-service.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/uvc-device.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/uvc-device-info.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/uvc-option.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/platform/uvc-option.h"
        "${CMAKE_CURRENT_LIST_DIR}/context.h"
        "${CMAKE_CURRENT_LIST_DIR}/depth-mapping-sensor.h"
        "${CMAKE_CURRENT_LIST_DIR}/device.h"
        "${CMAKE_CURRENT_LIST_DIR}/device-info.h"
        "${CMAKE_CURRENT_LIST_DIR}/device_hub.h"
        "${CMAKE_CURRENT_LIST_DIR}/environment.h"
        "${CMAKE_CURRENT_LIST_DIR}/firmware-version.h"
        "${CMAKE_CURRENT_LIST_DIR}/float3.h"
        "${CMAKE_CURRENT_LIST_DIR}/log.h"
        "${CMAKE_CURRENT_LIST_DIR}/error-handling.h"
        "${CMAKE_CURRENT_LIST_DIR}/firmware_logger_device.h"
        "${CMAKE_CURRENT_LIST_DIR}/frame-archive.h"
        "${CMAKE_CURRENT_LIST_DIR}/global_timestamp_reader.h"
        "${CMAKE_CURRENT_LIST_DIR}/hdr-config.h"
        "${CMAKE_CURRENT_LIST_DIR}/hw-monitor.h"
        "${CMAKE_CURRENT_LIST_DIR}/image.h"
        "${CMAKE_CURRENT_LIST_DIR}/image-avx.h"
        "${CMAKE_CURRENT_LIST_DIR}/metadata.h"
        "${CMAKE_CURRENT_LIST_DIR}/metadata-parser.h"
        "${CMAKE_CURRENT_LIST_DIR}/option.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform-camera.h"
        "${CMAKE_CURRENT_LIST_DIR}/pose.h"
        "${CMAKE_CURRENT_LIST_DIR}/safety-sensor.h"
        "${CMAKE_CURRENT_LIST_DIR}/sensor.h"
        "${CMAKE_CURRENT_LIST_DIR}/hid-sensor.h"
        "${CMAKE_CURRENT_LIST_DIR}/uvc-sensor.h"
        "${CMAKE_CURRENT_LIST_DIR}/software-device.h"
        "${CMAKE_CURRENT_LIST_DIR}/software-device-info.h"
        "${CMAKE_CURRENT_LIST_DIR}/software-sensor.h"
        "${CMAKE_CURRENT_LIST_DIR}/source.h"
        "${CMAKE_CURRENT_LIST_DIR}/stream.h"
        "${CMAKE_CURRENT_LIST_DIR}/sync.h"
        "${CMAKE_CURRENT_LIST_DIR}/terminal-parser.h"
        "${CMAKE_CURRENT_LIST_DIR}/types.h"
        "${CMAKE_CURRENT_LIST_DIR}/platform/command-transfer.h"
        "${CMAKE_CURRENT_LIST_DIR}/auto-calibrated-device.h"
        "${CMAKE_CURRENT_LIST_DIR}/auto-calibrated-proxy.h"
        "${CMAKE_CURRENT_LIST_DIR}/auto-calibrated-proxy.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/calibration-engine-interface.h"
        "${CMAKE_CURRENT_LIST_DIR}/device-calibration.h"
        "${CMAKE_CURRENT_LIST_DIR}/serializable-interface.h"
        "${CMAKE_CURRENT_LIST_DIR}/max-usable-range-sensor.h"
        "${CMAKE_CURRENT_LIST_DIR}/debug-stream-sensor.h"
        "${CMAKE_CURRENT_LIST_DIR}/serialized-utilities.h"
        "${CMAKE_CURRENT_LIST_DIR}/frame.h"
        "${CMAKE_CURRENT_LIST_DIR}/composite-frame.h"
        "${CMAKE_CURRENT_LIST_DIR}/points.h"
        "${CMAKE_CURRENT_LIST_DIR}/labeled-points.h"
        "${CMAKE_CURRENT_LIST_DIR}/inference-frame.h"
        "${CMAKE_CURRENT_LIST_DIR}/object-detection-frame.h"
        "${CMAKE_CURRENT_LIST_DIR}/depth-sensor.h"
        "${CMAKE_CURRENT_LIST_DIR}/color-sensor.h"
        "${CMAKE_CURRENT_LIST_DIR}/callback-invocation.h"
        "${CMAKE_CURRENT_LIST_DIR}/librealsense-exception.h"
        "${CMAKE_CURRENT_LIST_DIR}/polling-device-watcher.h"
        "${CMAKE_CURRENT_LIST_DIR}/small-heap.h"
        "${CMAKE_CURRENT_LIST_DIR}/basics.h"
        "${CMAKE_CURRENT_LIST_DIR}/feature-interface.h"
        "${CMAKE_CURRENT_LIST_DIR}/synthetic-options-watcher.h"
        "${CMAKE_CURRENT_LIST_DIR}/synthetic-options-watcher.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/eth-config-device.h"
        "${CMAKE_CURRENT_LIST_DIR}/embedded-filter-interface.h"
)

if(BUILD_WITH_DDS)
    file( GLOB_RECURSE RS_DDS_SOURCE_FILES
        LIST_DIRECTORIES false
        RELATIVE ${PROJECT_SOURCE_DIR}
        "${CMAKE_CURRENT_LIST_DIR}/dds/*"
        )
    target_sources( ${LRS_TARGET} PRIVATE ${RS_DDS_SOURCE_FILES} )
endif()
