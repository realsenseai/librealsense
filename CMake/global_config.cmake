# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Save the command line compile commands in the build output
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# View the makefile commands during build
#set(CMAKE_VERBOSE_MAKEFILE on)

include(GNUInstallDirs)
# include librealsense helper macros
include(CMake/lrs_macros.cmake)
include(CMake/version_config.cmake)

if(ENABLE_CCACHE)
  find_program(CCACHE_FOUND ccache)
  if(CCACHE_FOUND)
      set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
      set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
  endif(CCACHE_FOUND)
endif()

macro(global_set_flags)
    set(LRS_LIB_NAME ${LRS_TARGET})

    if (BUILD_WITH_CUDA AND BUILD_WITH_HIP)
        message(FATAL_ERROR "BUILD_WITH_CUDA and BUILD_WITH_HIP are mutually exclusive. Please enable only one.")
    endif()

    add_definitions(-DELPP_THREAD_SAFE)

    if (BUILD_GLSL_EXTENSIONS)
        set(LRS_GL_TARGET realsense2-gl)
        set(LRS_GL_LIB_NAME ${LRS_GL_TARGET})
    endif()

    if (BUILD_EASYLOGGINGPP)
        add_definitions(-DBUILD_EASYLOGGINGPP)
    endif()

    if (ENABLE_EASYLOGGINGPP_ASYNC)
        add_definitions(-DEASYLOGGINGPP_ASYNC)
    endif()

    if(TRACE_API)
        add_definitions(-DTRACE_API)
    endif()

    if(HWM_OVER_XU)
        add_definitions(-DHWM_OVER_XU)
    endif()

    if(COM_MULTITHREADED)
        add_definitions(-DCOM_MULTITHREADED)
    endif()

    if (ENFORCE_METADATA)
      add_definitions(-DENFORCE_METADATA)
    endif()

    if (BUILD_WITH_CUDA)
        add_definitions(-DRS2_USE_CUDA)
    endif()

    if (BUILD_WITH_CUDA_ZEROCOPY)
        if (NOT BUILD_WITH_CUDA)
            message(FATAL_ERROR "BUILD_WITH_CUDA_ZEROCOPY requires BUILD_WITH_CUDA=ON")
        endif()
        add_definitions(-DRS2_USE_CUDA_ZEROCOPY)
    endif()

    if (BUILD_WITH_HIP)
        add_definitions(-DRS2_USE_CUDA)
        add_definitions(-DRS2_USE_HIP)
    endif()

    if (BUILD_WITH_HIP_ZEROCOPY)
        if (NOT BUILD_WITH_HIP)
            message(FATAL_ERROR "BUILD_WITH_HIP_ZEROCOPY requires BUILD_WITH_HIP=ON")
        endif()
        # Shares RS2_USE_CUDA_ZEROCOPY with the CUDA path: the guarded code in
        # rscuda_utils.cuh / cuda-pointcloud.cu is already vendor-neutral (it falls back
        # to the persistent-buffer path whenever try_device_ptr() returns nullptr), so a
        # single macro is enough for both backends.
        add_definitions(-DRS2_USE_CUDA_ZEROCOPY)
    endif()

    if (BUILD_WITH_NEON)
        add_definitions(-DBUILD_WITH_NEON)
    endif()

    if (BUILD_SHARED_LIBS)
        add_definitions(-DBUILD_SHARED_LIBS)
    endif()

    if (BUILD_WITH_CUDA)
        include(CMake/cuda_config.cmake)
    endif()

    if (BUILD_WITH_HIP)
        include(CMake/hip_config.cmake)
    endif()

    if(BUILD_PYTHON_BINDINGS)
        include(libusb_config)
        include(CMake/external_pybind11.cmake)
    endif()

    if(CHECK_FOR_UPDATES)
        if (ANDROID_NDK_TOOLCHAIN_INCLUDED)
            message(STATUS "Android build do not support CHECK_FOR_UPDATES flag, turning it off..")
            set(CHECK_FOR_UPDATES false)
        elseif (NOT BUILD_GRAPHICAL_EXAMPLES)
            message(STATUS "CHECK_FOR_UPDATES depends on BUILD_GRAPHICAL_EXAMPLES flag, turning it off..")
            set(CHECK_FOR_UPDATES false)
        else()
            include(CMake/external_libcurl.cmake)
            add_definitions(-DCHECK_FOR_UPDATES)
        endif()
    endif()
        
    add_definitions(-D${BACKEND} -DUNICODE)
endmacro()

macro(global_target_config)
    target_link_libraries(${LRS_TARGET} PRIVATE realsense-file ${CMAKE_THREAD_LIBS_INIT})

    if (BUILD_WITH_HIP)
        target_include_directories(${LRS_TARGET} PRIVATE ${HIP_INCLUDE_DIRS})
        # Resolve the full path to the HIP runtime instead of linking the bare "amdhip64"
        # name: a bare name needs "-L${ROCM_PATH}/lib" on the final link line, and
        # ROCm's lib dir is not always on the default linker search path (only picked up
        # automatically if ldconfig / HIP_PATH already knows about it). More importantly,
        # when BUILD_SHARED_LIBS=OFF, ${LRS_TARGET} is a static archive: CMake forwards its
        # PRIVATE link libraries to whatever finally links that archive (e.g. the "static!"
        # unit tests), but only this target's own target_link_directories -- which is
        # PRIVATE and does NOT forward. A resolved full path needs no "-L" at all, so it
        # works correctly however far downstream the archive is linked.
        find_library(RS_AMDHIP64_LIBRARY amdhip64 PATHS "${ROCM_PATH}/lib" NO_DEFAULT_PATH)
        if(NOT RS_AMDHIP64_LIBRARY)
            message(FATAL_ERROR "Could not find the HIP runtime library (amdhip64) under ${ROCM_PATH}/lib")
        endif()
        target_link_libraries(${LRS_TARGET} PRIVATE ${RS_AMDHIP64_LIBRARY})
    endif()

    set_target_properties (${LRS_TARGET} PROPERTIES FOLDER Library)

    target_include_directories(${LRS_TARGET}
        PRIVATE
            src
            ${LIBUSB_LOCAL_INCLUDE_PATH}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
            PRIVATE ${USB_INCLUDE_DIRS}
    )


endmacro()

