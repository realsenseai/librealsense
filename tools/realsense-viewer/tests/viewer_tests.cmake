# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#
# Build configuration for realsense-viewer-tests
# Included from tools/realsense-viewer/CMakeLists.txt when BUILD_VIEWER_TESTS is ON

include(FetchContent)
FetchContent_Declare(imgui_test_engine
    GIT_REPOSITORY https://github.com/ocornut/imgui_test_engine.git
    GIT_TAG v1.90.8 GIT_SHALLOW ON)
FetchContent_MakeAvailable(imgui_test_engine)
set(TE_DIR ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine)

file(GLOB_RECURSE VIEWER_TEST_CASES tests/test-*.cpp)
list(REMOVE_ITEM VIEWER_TEST_CASES ${CMAKE_CURRENT_SOURCE_DIR}/tests/hw-reset/test-hw-reset.cpp)
set(VIEWER_TEST_SOURCES
    tests/viewer-test-main.cpp
    tests/viewer-test-helpers.cpp
    tests/hw-reset/test-hw-reset.cpp  # explicit ordering: runs first to check device connected
    ${VIEWER_TEST_CASES}
)
file(GLOB TE_SOURCES ${TE_DIR}/*.cpp)

add_executable(realsense-viewer-tests
    ${RS_VIEWER_CPP} ${VIEWER_TEST_SOURCES} ${TE_SOURCES})
tools_target_config(realsense-viewer-tests)
set_target_properties(realsense-viewer-tests PROPERTIES FOLDER "Tools/Tests")

target_compile_definitions(realsense-viewer-tests PRIVATE
    IMGUI_ENABLE_TEST_ENGINE IMGUI_TEST_ENGINE_ENABLE_STD_FUNCTION
    IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1)
target_include_directories(realsense-viewer-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${TE_DIR} ${TE_DIR}/thirdparty/Str)

# Suppress warnings in third-party test engine sources
if(MSVC)
    set_source_files_properties(${TE_SOURCES} PROPERTIES COMPILE_OPTIONS "/W0")
else()
    set_source_files_properties(${TE_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")
endif()

source_group("Test Files" FILES ${VIEWER_TEST_SOURCES})
