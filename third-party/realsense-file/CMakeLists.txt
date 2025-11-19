cmake_minimum_required(VERSION 3.10)

project(realsense-file)

set(ROSBAG2_COMPILE_FLAGS)
include(config.cmake)

FILE(GLOB_RECURSE AllSources
        ${ROSBAG_DIR}/*.h
        ${ROSBAG_DIR}/*.cpp
        ${ROSBAG_DIR}/*.c
        )

if (USE_EXTERNAL_LZ4)
        find_package(lz4 REQUIRED)
else()
        LIST(APPEND AllSources ${LZ4_DIR}/lz4.h ${LZ4_DIR}/lz4.c)
        source_group("Header Files\\lz4" FILES
                lz4/lz4.h
        )
        source_group("Source Files\\lz4" FILES
                lz4/lz4.c
        )
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_library(${PROJECT_NAME} STATIC
    ${AllSources}
    ${HEADER_FILES_ROSBAG}
    ${SOURCE_FILES_ROSBAG}
    ${SOURCE_FILES_COMPRESSION}
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_14)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${ROSBAG_HEADER_DIRS}
        $<$<NOT:$<BOOL:${USE_EXTERNAL_LZ4}>>:${LZ4_INCLUDE_PATH}>
)

message(STATUS "ROSBAG2_COMPILE_FLAGS: ${ROSBAG2_COMPILE_FLAGS}")
target_compile_definitions(${PROJECT_NAME} PUBLIC
        ${ROSBAG2_COMPILE_FLAGS}
)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13.0)
    target_compile_options(realsense-file PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>
      $<$<COMPILE_LANGUAGE:CXX>:-include cstdint>
      $<$<COMPILE_LANGUAGE:C>:-include stdint.h>
    )
    target_link_libraries(realsense-file PRIVATE stdc++fs)
  endif()
endif()


target_link_libraries(${PROJECT_NAME} PRIVATE
        $<$<BOOL:${USE_EXTERNAL_LZ4}>:lz4::lz4>
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        yaml-cpp
        SQLite3
)

#set_target_properties(${PROJECT_NAME} PROPERTIES VERSION "${LIBVERSION}" SOVERSION "${LIBSOVERSION}")

set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER Library)

set(CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/../cmake/realsense2")

install(TARGETS ${PROJECT_NAME}
    yaml-cpp
    EXPORT realsense2Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
