# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019-2024 RealSense, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.10)

project(fw)

file(READ "firmware-version.h" ver)

message(STATUS "Fetching recommended firmwares:")

set(REALSENSE_FIRMWARE_URL "https://librealsense.realsenseai.com" CACHE STRING
    "URL to download firmware binaries from")

string(REGEX MATCH "D4XX_RECOMMENDED_FIRMWARE_VERSION \"([0-9]+.[0-9]+.[0-9]+.[0-9]+)\"" _ ${ver})
set(D4XX_FW_VERSION ${CMAKE_MATCH_1})
#message(STATUS "D4XX_FW_VERSION: ${D4XX_FW_VERSION}")
set(D4XX_FW_SHA1 e7a67224fd0bd823df03dc7f1135a59db93746fd)
set(D4XX_FW_URL "${REALSENSE_FIRMWARE_URL}/Releases/RS4xx/FW")

add_library(${PROJECT_NAME} STATIC empty.c)

# disable link time optimization for fw by adding -fno-lto to disable -flto flag
# jammy debian has build errors without it
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  message(STATUS "disable link time optimization for fw project")
  target_compile_options(${PROJECT_NAME} PRIVATE -fno-lto)
endif()

if (MSVC)
  # lib.exe can't handle multiple .res files, so include them in one.
  # even then, the linker won't grab a .res out of a .lib object, so it needs to be explicitly listed
  # and to find the name of the .res file (across cmake generators) we need to create our own rule. :(
  add_custom_command(TARGET ${PROJECT_NAME} BYPRODUCTS ${PROJECT_NAME}.res COMMAND ${CMAKE_RC_COMPILER} ${CMAKE_RC_FLAGS} /I . /fo "${PROJECT_NAME}.res" "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.rc" POST_BUILD)
  target_link_libraries(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.res")
endif()

target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER Resources)

function(target_binary url version sha1 symbol ext)
  set(binary "${CMAKE_CURRENT_BINARY_DIR}/${symbol}-${version}${ext}")
  message(STATUS "... ${url}/${symbol}-${version}${ext}")
  
  # Check if file exists and has correct hash
  set(need_download TRUE)
  if(EXISTS "${binary}")
    file(SHA1 "${binary}" existing_hash)
    if("${existing_hash}" STREQUAL "${sha1}")
      set(need_download FALSE)
      message(STATUS "Firmware file already exists with correct hash: ${symbol}-${version}${ext}")
    else()
      message(STATUS "Firmware file exists but hash mismatch, re-downloading: ${symbol}-${version}${ext}")
    endif()
  endif()
  
  if(need_download)
    file(DOWNLOAD "${url}/${symbol}-${version}${ext}" "${binary}"
         EXPECTED_HASH SHA1=${sha1}
         STATUS status)
    list(GET status 0 error_code)
    if (NOT ${error_code} EQUAL 0)
        message(FATAL_ERROR "FAILED with status ${status}")
    else()
        message(STATUS "Downloaded firmware ${symbol}-${version}${ext}")
    endif()
  endif()
  string(TOUPPER ${symbol} SYMBOL)
  string(REPLACE "." "," version_commas ${version})
  string(REPLACE "\\" "\\\\" binary_escaped "${binary}")
  configure_file(fw.c.in  "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.c" @ONLY)
  configure_file(fw.h.in  "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.h" @ONLY)
  configure_file(fw.rc.in "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.rc" @ONLY)
  if (MSVC)
    enable_language(RC)
    set_source_files_properties("${PROJECT_NAME}.rc" OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.rc")
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/${symbol}.rc" OBJECT_DEPENDS "${binary}")
  else()
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/${symbol}.c" OBJECT_DEPENDS "${binary}")
  endif()
  target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.c")
endfunction()

target_binary( "${D4XX_FW_URL}"  "${D4XX_FW_VERSION}"  "${D4XX_FW_SHA1}" D4XX_FW_Image .bin)
