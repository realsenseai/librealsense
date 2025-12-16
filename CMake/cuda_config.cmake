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
set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS}; -O3 -gencode arch=compute_75,code=sm_75 -gencode arch=compute_80,code=sm_80 -gencode arch=compute_110,code=sm_110 -gencode arch=compute_120,code=sm_120;")
