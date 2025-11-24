# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022 RealSense, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.10)  # source_group(TREE)
project( rsutils )

add_library( ${PROJECT_NAME} "" )
# We cannot directly interface with nlohmann_json (doesn't work on bionic)
#target_link_libraries( ${PROJECT_NAME} PUBLIC nlohmann_json )
target_compile_features( ${PROJECT_NAME} PUBLIC cxx_std_14 )
set_target_properties( ${PROJECT_NAME} PROPERTIES FOLDER Library )
set_target_properties( ${PROJECT_NAME} PROPERTIES VERSION ${REALSENSE_VERSION_STRING} SOVERSION "${REALSENSE_VERSION_MAJOR}.${REALSENSE_VERSION_MINOR}")

target_include_directories( ${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/third-party/json/include>
)

# Headers -----------------------------------------------------------------------------------
#
set( UTILITIES_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/include/rsutils" )
file(GLOB_RECURSE UTILITIES_HEADER_FILES
    LIST_DIRECTORIES false
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    "${UTILITIES_INCLUDE_DIR}/*.h"
)
if( BUILD_EASYLOGGINGPP )
    target_sources( ${PROJECT_NAME}
        PRIVATE "${REPO_ROOT}/third-party/easyloggingpp/src/easylogging++.h" )
endif()
target_sources( ${PROJECT_NAME} PRIVATE ${UTILITIES_HEADER_FILES} )
source_group(
    TREE ${UTILITIES_INCLUDE_DIR}
    PREFIX "Header Files"
    FILES ${UTILITIES_HEADER_FILES} )


# Sources -----------------------------------------------------------------------------------
#
set( UTILITIES_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src" )
file(GLOB_RECURSE UTILITIES_SOURCE_FILES
    LIST_DIRECTORIES false
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    "${UTILITIES_SRC_DIR}/*"
)
if( BUILD_EASYLOGGINGPP )
    target_sources( ${PROJECT_NAME}
        PRIVATE "${REPO_ROOT}/third-party/easyloggingpp/src/easylogging++.cc" )
    # We want to disable any default ELPP log-file!
    target_compile_definitions( ${PROJECT_NAME}
        PUBLIC ELPP_NO_DEFAULT_LOG_FILE )
endif()
target_sources( ${PROJECT_NAME} PRIVATE ${UTILITIES_SOURCE_FILES} )
source_group(
    TREE ${UTILITIES_SRC_DIR}
    PREFIX "Source Files"
    FILES ${UTILITIES_SOURCE_FILES} )


# EasyLogging++ uses global variables that need to be declared somewhere. It therefore
# introduces a macro, INITIALIZE_EASYLOGGINGPP, that needs to be inserted in the code.
# If a client of ours wants to use the easylogging macros, they must do this, or they
# can use this macro in their CMake project file:
#
macro( _using_easyloggingpp include_dir )
    #
    # using_easyloggingpp( <project-name> [SHARED | STATIC] )
    #       [SHARED] only initialize when BUILD_SHARED_LIBS is ON
    #       [STATIC] only initialize when BUILD_SHARED_LIBS is OFF
    #
    # Projects that depend on realsense2 (and want to use ELPP themselves) need [SHARED]
    # because realsense2 always includes the initialization.
    #
    function( using_easyloggingpp target_name )
        set( func_ARGN ARGN )  # otherwise we get the outside macro's ARGN
        cmake_parse_arguments( ARG "SHARED;STATIC" "" "" ${${func_ARGN}} )
        if( ARG_STATIC AND ARG_SHARED )
            message( FATAL_ERROR "Can't be both STATIC and SHARED" )
        elseif( ARG_SHARED )
            if( NOT BUILD_SHARED_LIBS )
                return()
            endif()
        elseif( ARG_STATIC )
            if( BUILD_SHARED_LIBS )
                return()
            endif()
        endif()

        target_sources( ${target_name}
            PRIVATE "${include_dir}/easylogging/elpp-init.cpp" )
    endfunction()
endmacro()
_using_easyloggingpp( ${UTILITIES_INCLUDE_DIR} )


# Install -----------------------------------------------------------------------------------
#
install( TARGETS ${PROJECT_NAME}
    EXPORT realsense2Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )


# Python ------------------------------------------------------------------------------------
#
if( BUILD_PYTHON_BINDINGS )
    add_subdirectory( py )
endif()

