# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 Intel Corporation. All Rights Reserved.

# Modifications Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

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

    if (BUILD_WITH_HIP)
        add_definitions(-DRS2_USE_CUDA)
        add_definitions(-DRS2_USE_HIP)
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
        # hip::device is the imported target produced by find_package(hip)
        # in CMake/hip_config.cmake.  It transitively provides:
        #   - the correct linker flag for libamdhip64 (so we no longer need
        #     a hard-coded "amdhip64" / "amdhip64.lib")
        #   - the HIP include directories (so a separate
        #     target_include_directories on HIP_INCLUDE_DIRS is unnecessary)
        #   - the link search path on Windows (so target_link_directories
        #     on ${ROCM_PATH}/lib is unnecessary)
        target_link_libraries(${LRS_TARGET} PRIVATE hip::device)
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

