info("Building with CUDA requires CMake v3.8+")
cmake_minimum_required(VERSION 3.10)
enable_language( CUDA )

find_package(CUDA REQUIRED)

include_directories(${CUDA_INCLUDE_DIRS})
SET(ALL_CUDA_LIBS ${CUDA_LIBRARIES} ${CUDA_cusparse_LIBRARY} ${CUDA_cublas_LIBRARY})
SET(LIBS ${LIBS} ${ALL_CUDA_LIBS})

message(STATUS "CUDA_LIBRARIES: ${CUDA_INCLUDE_DIRS} ${ALL_CUDA_LIBS}")

set(CUDA_PROPAGATE_HOST_FLAGS OFF)
set(CUDA_SEPARABLE_COMPILATION ON)

# Build CUDA architecture list based on CUDA version
if(CUDA_VERSION VERSION_LESS "13.0")
    set(CUDA_ARCH_LIST 62)  # Pascal (CUDA 8.0-12.x, dropped in CUDA 13.0)
else()
    set(CUDA_ARCH_LIST)
endif()
if(CUDA_VERSION VERSION_GREATER_EQUAL "10.0")
    list(APPEND CUDA_ARCH_LIST 75)  # Turing
endif()
if(CUDA_VERSION VERSION_GREATER_EQUAL "11.0")
    list(APPEND CUDA_ARCH_LIST 80 86)  # Ampere
endif()
if(CUDA_VERSION VERSION_GREATER_EQUAL "11.8")
    list(APPEND CUDA_ARCH_LIST 89)  # Ada Lovelace
endif()
if(CUDA_VERSION VERSION_GREATER_EQUAL "12.0")
    list(APPEND CUDA_ARCH_LIST 90)  # Hopper
endif()
if(CUDA_VERSION VERSION_GREATER_EQUAL "12.8")
    list(APPEND CUDA_ARCH_LIST 100 120)  # B200/RTX 50/DGX Spark
endif()
if(CUDA_VERSION VERSION_GREATER_EQUAL "13.0")
    list(APPEND CUDA_ARCH_LIST 110)  # Jetson Thor
endif()

# Check if variable is available (means CMake >= 3.18)
if(POLICY CMP0104)
    # Use modern approach
    cmake_policy(SET CMP0104 NEW)
    set(CMAKE_CUDA_ARCHITECTURES ${CUDA_ARCH_LIST})
else()
    # Fallback for older CMake: build NVCC flags from architecture list
    foreach(ARCH ${CUDA_ARCH_LIST})
        set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} -gencode arch=compute_${ARCH},code=sm_${ARCH}")
    endforeach()
endif()
