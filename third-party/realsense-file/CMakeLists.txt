cmake_minimum_required(VERSION 3.10)

set(REALSENSE_FILE_TARGET realsense-file)

set(ROSBAG2_COMPILE_FLAGS)
if(BUILD_ROSBAG2)
    include(${PROJECT_SOURCE_DIR}/CMake/external_sqlite3.cmake)
    include(${PROJECT_SOURCE_DIR}/CMake/external_yaml_cpp.cmake)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/config.cmake)

FILE(GLOB_RECURSE AllSources
        ${ROSBAG_DIR}/*.h
        ${ROSBAG_DIR}/*.cpp
        ${ROSBAG_DIR}/*.c
        )

if(USE_EXTERNAL_LZ4)
    find_package(lz4 REQUIRED)
    add_library(rs_lz4 INTERFACE)
    target_link_libraries(rs_lz4 INTERFACE lz4::lz4)
else()
    add_library(rs_lz4 STATIC ${LZ4_DIR}/lz4.c ${LZ4_DIR}/lz4.h)
    target_include_directories(rs_lz4 PUBLIC "$<BUILD_INTERFACE:${LZ4_DIR}>")
    source_group("Header Files\\lz4" FILES ${LZ4_DIR}/lz4.h)
    source_group("Source Files\\lz4" FILES ${LZ4_DIR}/lz4.c)
    disable_third_party_warnings(rs_lz4)
    set_target_properties(rs_lz4 PROPERTIES FOLDER Library)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_library(${REALSENSE_FILE_TARGET} STATIC
    ${AllSources}
    ${HEADER_FILES_ROSBAG}
    ${SOURCE_FILES_ROSBAG}
    ${SOURCE_FILES_COMPRESSION}
)

target_compile_features(${REALSENSE_FILE_TARGET} PUBLIC cxx_std_14)

foreach(_dir ${ROSBAG_HEADER_DIRS})
    target_include_directories(${REALSENSE_FILE_TARGET} PUBLIC $<BUILD_INTERFACE:${_dir}>)
endforeach()

disable_third_party_warnings(${REALSENSE_FILE_TARGET})

target_compile_definitions(${REALSENSE_FILE_TARGET} PUBLIC
        ${ROSBAG2_COMPILE_FLAGS}
)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13.0) # GCC 13 and above require cstdint to be included for stdint types
    target_compile_options(${REALSENSE_FILE_TARGET} PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>
      $<$<COMPILE_LANGUAGE:CXX>:-include cstdint>
      $<$<COMPILE_LANGUAGE:C>:-include stdint.h>
    )
  endif()
endif()

if(BUILD_ROSBAG2)
    add_dependencies(${REALSENSE_FILE_TARGET} sqlite3_lib)
    add_dependencies(${REALSENSE_FILE_TARGET} yaml_cpp)
endif()

target_link_libraries(${REALSENSE_FILE_TARGET} PUBLIC rs_lz4)
if(BUILD_ROSBAG2)
    target_link_libraries(${REALSENSE_FILE_TARGET} PRIVATE sqlite3_lib)
endif()

#set_target_properties(${REALSENSE_FILE_TARGET} PROPERTIES VERSION "${LIBVERSION}" SOVERSION "${LIBSOVERSION}")

set_target_properties (${REALSENSE_FILE_TARGET} PROPERTIES FOLDER Library)

set(CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/../cmake/realsense2")

set(_install_targets ${REALSENSE_FILE_TARGET} rs_lz4)
if(BUILD_ROSBAG2)
    list(APPEND _install_targets sqlite3_lib)
endif()
install(TARGETS ${_install_targets}
    EXPORT realsense2Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
