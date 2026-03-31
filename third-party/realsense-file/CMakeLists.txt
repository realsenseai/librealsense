cmake_minimum_required(VERSION 3.10)

project(realsense-file)

set(ROSBAG2_COMPILE_FLAGS)
if(BUILD_ROSBAG2)
    include(CMake/external_sqlite3.cmake)
    include(CMake/external_yaml_cpp.cmake)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/config.cmake)

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

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_14)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${ROSBAG_HEADER_DIRS}
        $<$<NOT:$<BOOL:${USE_EXTERNAL_LZ4}>>:${LZ4_INCLUDE_PATH}>
)

disable_third_party_warnings(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PUBLIC
        ${ROSBAG2_COMPILE_FLAGS}
)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13.0) # GCC 13 and above require cstdint to be included for stdint types
    target_compile_options(${PROJECT_NAME} PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>
      $<$<COMPILE_LANGUAGE:CXX>:-include cstdint>
      $<$<COMPILE_LANGUAGE:C>:-include stdint.h>
    )
  endif()
endif()

if(BUILD_ROSBAG2)
    add_dependencies(${PROJECT_NAME} sqlite3_lib)
    add_dependencies(${PROJECT_NAME} yaml_cpp)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
        $<$<BOOL:${USE_EXTERNAL_LZ4}>:lz4::lz4>
)
if(BUILD_ROSBAG2)
    target_link_libraries(${PROJECT_NAME} PRIVATE sqlite3_lib)
endif()

#set_target_properties(${PROJECT_NAME} PROPERTIES VERSION "${LIBVERSION}" SOVERSION "${LIBSOVERSION}")

set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER Library)

set(CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/../cmake/realsense2")

set(_install_targets ${PROJECT_NAME})
if(BUILD_ROSBAG2)
    list(APPEND _install_targets sqlite3_lib)
endif()
install(TARGETS ${_install_targets}
    EXPORT realsense2Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
