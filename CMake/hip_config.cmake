info("Building with HIP/ROCm for AMD GPUs..")
cmake_minimum_required(VERSION 3.21)

# Find ROCm/HIP SDK installation (Linux and Windows)
if(NOT DEFINED ROCM_PATH)
    if(DEFINED ENV{ROCM_PATH})
        set(ROCM_PATH "$ENV{ROCM_PATH}")
    elseif(DEFINED ENV{HIP_PATH})
        # Windows HIP SDK sets HIP_PATH automatically
        set(ROCM_PATH "$ENV{HIP_PATH}")
    elseif(WIN32)
        # Search common Windows install locations
        file(GLOB _rocm_candidates "C:/Program Files/AMD/ROCm/*")
        if(_rocm_candidates)
            # Pick the latest version (last in sorted glob)
            list(SORT _rocm_candidates)
            list(GET _rocm_candidates -1 ROCM_PATH)
        endif()
    elseif(EXISTS "/opt/rocm")
        set(ROCM_PATH "/opt/rocm")
    endif()
endif()

if(NOT DEFINED ROCM_PATH OR ROCM_PATH STREQUAL "")
    message(FATAL_ERROR
        "Could not find ROCm/HIP SDK installation.\n"
        "Please do one of the following:\n"
        "  - Set ROCM_PATH: cmake .. -DROCM_PATH=/path/to/rocm\n"
        "  - Set HIP_PATH environment variable\n"
        "  - Linux: install ROCm to /opt/rocm\n"
        "  - Windows: install HIP SDK from https://www.amd.com/en/developer/resources/rocm-hub/hip-sdk.html")
endif()

message(STATUS "ROCm/HIP SDK path: ${ROCM_PATH}")

# Auto-detect HIP compiler if not specified
if(NOT DEFINED CMAKE_HIP_COMPILER)
    if(WIN32)
        find_program(_hip_compiler clang++.exe
            PATHS "${ROCM_PATH}/bin" "${ROCM_PATH}/hip/bin"
            NO_DEFAULT_PATH)
    else()
        find_program(_hip_compiler clang++
            PATHS "${ROCM_PATH}/lib/llvm/bin"
            NO_DEFAULT_PATH)
    endif()
    if(_hip_compiler)
        set(CMAKE_HIP_COMPILER "${_hip_compiler}")
        message(STATUS "Auto-detected HIP compiler: ${_hip_compiler}")
    else()
        message(FATAL_ERROR
            "Could not find HIP compiler (clang++) in ${ROCM_PATH}.\n"
            "Please specify it: cmake .. -DCMAKE_HIP_COMPILER=/path/to/clang++")
    endif()
endif()

# Tell CMake where the ROCm root is (needed for enable_language on Windows)
if(NOT DEFINED CMAKE_HIP_COMPILER_ROCM_ROOT)
    set(CMAKE_HIP_COMPILER_ROCM_ROOT "${ROCM_PATH}")
endif()

enable_language(HIP)

find_package(hip REQUIRED CONFIG
    PATHS "${ROCM_PATH}" "${ROCM_PATH}/lib/cmake/hip" "${ROCM_PATH}/cmake")

include_directories(${HIP_INCLUDE_DIRS})

message(STATUS "HIP_INCLUDE_DIRS: ${HIP_INCLUDE_DIRS}")

# Default architectures: MI200, MI300, RDNA3
if(NOT DEFINED CMAKE_HIP_ARCHITECTURES OR CMAKE_HIP_ARCHITECTURES STREQUAL "")
    set(CMAKE_HIP_ARCHITECTURES "gfx90a;gfx942;gfx1100;gfx1101")
endif()

# Define HIP platform for .cpp files that include hip_runtime.h
add_definitions(-D__HIP_PLATFORM_AMD__)

message(STATUS "HIP architectures: ${CMAKE_HIP_ARCHITECTURES}")

# Store HIP link target for use by realsense2 target
set(HIP_LINK_LIBRARIES hip::device CACHE INTERNAL "HIP libraries to link")
