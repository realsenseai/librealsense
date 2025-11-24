cmake_minimum_required(VERSION 3.10)

project(realsense-file)

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

add_library(${PROJECT_NAME}
    ${AllSources}
    ${HEADER_FILES_ROSBAG}
    ${SOURCE_FILES_ROSBAG}
    ${SOURCE_FILES_COMPRESSION}
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${ROSBAG_HEADER_DIRS}
        $<$<NOT:$<BOOL:${USE_EXTERNAL_LZ4}>>:${LZ4_INCLUDE_PATH}>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        $<$<BOOL:${USE_EXTERNAL_LZ4}>:lz4::lz4>
)

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${REALSENSE_VERSION_STRING} SOVERSION "${REALSENSE_VERSION_MAJOR}.${REALSENSE_VERSION_MINOR}")

set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER Library)

set(CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/../cmake/realsense2")

install(TARGETS ${PROJECT_NAME}
    EXPORT realsense2Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
