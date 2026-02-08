cmake_minimum_required(VERSION 3.10)

project(RealsensePythonWrappers)

set(PYRS_CPP
    pyrealsense2.cpp
    c_files.cpp
    pyrs_advanced_mode.cpp
    pyrs_context.cpp
    pyrs_device.cpp
    pyrs_eth_config.cpp
    pyrs_export.cpp
    pyrs_frame.cpp
    pyrs_internal.cpp
    pyrs_options.cpp
    pyrs_pipeline.cpp
    pyrs_processing.cpp
    pyrs_record_playback.cpp
    pyrs_sensor.cpp
    pyrs_types.cpp
    pyrsutil.cpp
    ../../common/metadata-helper.cpp
)

pybind11_add_module(pyrealsense2 SHARED pyrealsense2.h ${PYRS_CPP})

set(CMAKECONFIG_PY_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/pyrealsense2")

target_link_libraries(pyrealsense2 PRIVATE ${DEPENDENCIES})
set_target_properties(pyrealsense2 PROPERTIES VERSION
    ${REALSENSE_VERSION_STRING} SOVERSION "${REALSENSE_VERSION_MAJOR}.${REALSENSE_VERSION_MINOR}")
set_target_properties( pyrealsense2
    PROPERTIES
        FOLDER Library/Python
    )

set(RAW_RS
    pybackend.cpp
    pybackend_extras.h
    pybackend_extras.cpp
    ../../src/types.h
    ../../src/types.cpp
    ../../src/log.cpp
    ../../src/backend.h
    ../../src/backend.cpp
    ../../src/frame.cpp
    ../../src/environment.cpp
    ../../src/to-string.cpp
    ../../src/core/options-registry.cpp
)

if(UNIX)
    list(APPEND RAW_RS
        ../../src/libusb/interface-libusb.cpp
        ../../src/libusb/device-libusb.cpp
        ../../src/libusb/messenger-libusb.cpp
        ../../src/libusb/enumerator-libusb.cpp
        ../../src/libusb/request-libusb.cpp
        ../../src/libusb/context-libusb.cpp
    )
endif()

if(WIN32)
    list(APPEND RAW_RS
        ../../src/win/win-helpers.cpp
        ../../src/winusb/interface-winusb.cpp
        ../../src/winusb/device-winusb.cpp
        ../../src/winusb/messenger-winusb.cpp
        ../../src/winusb/enumerator-winusb.cpp
        ../../src/winusb/request-winusb.cpp
    )
endif()

if(${BACKEND} STREQUAL RS2_USE_V4L2_BACKEND)
    list(APPEND RAW_RS
        ../../src/linux/backend-v4l2.cpp
        ../../src/linux/backend-hid.cpp
    )
endif()

if(${BACKEND} STREQUAL RS2_USE_WMF_BACKEND)
    list(APPEND RAW_RS
        ../../src/win/win-helpers.cpp
        ../../src/mf/mf-uvc.cpp
        ../../src/mf/mf-hid.cpp
        ../../src/mf/mf-backend.cpp
    )
endif()

if(${FORCE_RSUSB_BACKEND})
    list(APPEND RAW_RS
        ../../src/rsusb-backend/rsusb-backend.h
        ../../src/rsusb-backend/rsusb-backend.cpp
        ../../src/uvc/uvc-device.cpp
        ../../src/uvc/uvc-parser.cpp
        ../../src/uvc/uvc-streamer.cpp
        ../../src/hid/hid-device.cpp
        ../../src/hid/hid-device.h
    )
    if(APPLE)
        list(APPEND RAW_RS
            ../../third-party/hidapi/hidapi.cpp
            ../../third-party/hidapi/hidapi.h
        )
    endif()
endif()

if(${BUILD_EASYLOGGINGPP})
    list(APPEND RAW_RS
        ../../third-party/easyloggingpp/src/easylogging++.h
        ../../third-party/easyloggingpp/src/easylogging++.cc
    )
endif()

if(${BACKEND} STREQUAL RS2_USE_WINUSB_UVC_BACKEND)
    list(APPEND RAW_RS
        ../../src/win7/rsusb-backend-windows.h
        ../../src/win7/rsusb-backend-windows.cpp
    )
endif()

if(${BACKEND} STREQUAL RS2_USE_LIBUVC_BACKEND)
    list(APPEND RAW_RS
        ../../src/libuvc/rsusb-backend-linux.h
        ../../src/libuvc/rsusb-backend-linux.cpp
    )
endif()

if( BUILD_LEGACY_PYBACKEND )
    pybind11_add_module(pybackend2 SHARED ${RAW_RS})

    if(USE_EXTERNAL_USB)
        add_dependencies(pybackend2 libusb)
    endif()
    target_link_libraries(pybackend2 PRIVATE rsutils usb ${CMAKE_THREAD_LIBS_INIT})
    set_target_properties(pybackend2 PROPERTIES
                            VERSION     ${REALSENSE_VERSION_STRING}
                            SOVERSION   ${REALSENSE_VERSION_MAJOR})
    set_target_properties( pybackend2
        PROPERTIES
            FOLDER Library/Python
        )
    target_include_directories(pybackend2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

    if(${FORCE_RSUSB_BACKEND})
        if(APPLE)
            target_include_directories(pybackend2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../third-party/hidapi/)
        endif()
    endif()

    if(${BACKEND} STREQUAL RS2_USE_V4L2_BACKEND)
        if(UDEV_FOUND)
            target_sources( pybackend2
                PRIVATE
                    ../../src/linux/udev-device-watcher.cpp
                    ../../src/linux/udev-device-watcher.h
                )
            target_link_libraries( pybackend2 PRIVATE udev )
            target_compile_definitions( pybackend2 PRIVATE -DUSING_UDEV )
        endif()
    endif()

    install(TARGETS pybackend2 pyrealsense2
        EXPORT pyrealsense2Targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${PYTHON_INSTALL_DIR}
        ARCHIVE DESTINATION ${PYTHON_INSTALL_DIR}
    )

    target_include_directories(pybackend2 PRIVATE ../../src)

else() # not BUILD_LEGACY_PYBACKEND

    install(TARGETS pyrealsense2
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${PYTHON_INSTALL_DIR}
        ARCHIVE DESTINATION ${PYTHON_INSTALL_DIR}
    )

endif()  # BUILD_LEGACY_PYBACKEND

write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/pyrealsense2ConfigVersion.cmake"
    VERSION ${REALSENSE_VERSION_STRING} COMPATIBILITY AnyNewerVersion)

configure_package_config_file(../../CMake/pyrealsense2Config.cmake.in pyrealsense2Config.cmake
    INSTALL_DESTINATION ${CMAKECONFIG_PY_INSTALL_DIR}
    INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/bin
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR
)

install(EXPORT pyrealsense2Targets
    FILE pyrealsense2Targets.cmake
    NAMESPACE pyrealsense2::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/pyrealsense2")

install(FILES "${CMAKE_BINARY_DIR}/wrappers/python/pyrealsense2Config.cmake"
    DESTINATION ${CMAKECONFIG_PY_INSTALL_DIR}
)

install(FILES "${CMAKE_BINARY_DIR}/wrappers/python/pyrealsense2ConfigVersion.cmake"
    DESTINATION ${CMAKECONFIG_PY_INSTALL_DIR}
)

install(FILES pyrealsense2/__init__.py
    DESTINATION ${PYTHON_INSTALL_DIR}
)

target_include_directories(pyrealsense2 PRIVATE ../../src)

if (BUILD_PYTHON_DOCS)
    add_subdirectory(docs)
endif()
