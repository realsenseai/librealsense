cmake_minimum_required(VERSION 3.16.3)  # same as in FastDDS (U20)
include(FetchContent)

# We use a function to enforce a scoped variables creation only for FastDDS build (i.e turn off BUILD_SHARED_LIBS which is used on LRS build as well)
function(get_fastdds)

    # Mark new options from FetchContent as advanced options
    mark_as_advanced(FETCHCONTENT_QUIET)
    mark_as_advanced(FETCHCONTENT_BASE_DIR)
    mark_as_advanced(FETCHCONTENT_FULLY_DISCONNECTED)
    mark_as_advanced(FETCHCONTENT_UPDATES_DISCONNECTED)

    message(CHECK_START  "Fetching fastdds...")
    list(APPEND CMAKE_MESSAGE_INDENT "  ")  # Indent outputs

    if(APPLE)
        get_filename_component(_FASTDDS_MACOS_REUSEPORT_PATCH
            "${CMAKE_SOURCE_DIR}/CMake/patches/fastdds-v2.10.4-macos-reuseport.patch"
            ABSOLUTE)
    endif()

    FetchContent_Declare(
      fastdds
      GIT_REPOSITORY https://github.com/eProsima/Fast-DDS.git
      # 2.10.x is eProsima's last LTS version that still supports U20
      # 2.10.4 has specific modifications based on support provided, but it has some incompatibility
      # with the way we clone (which works with v2.11+), so they made a fix and tagged it for us:
      # Once they have 2.10.5 we should move to it
      GIT_TAG        v2.10.4-realsense
      GIT_SUBMODULES ""     # Submodules will be cloned as part of the FastDDS cmake configure stage
      GIT_SHALLOW ON        # No history needed
      SOURCE_DIR ${CMAKE_BINARY_DIR}/third-party/fastdds
    )

    # Set FastDDS internal variables
    # We use cached variables so the default parameter inside the sub directory will not override the required values
    # We add "FORCE" so that is a previous cached value is set our assignment will override it.
    set(THIRDPARTY_Asio FORCE CACHE INTERNAL "" FORCE)
    set(THIRDPARTY_fastcdr FORCE CACHE INTERNAL "" FORCE)
    set(THIRDPARTY_TinyXML2 FORCE CACHE INTERNAL "" FORCE)
    set(COMPILE_TOOLS OFF CACHE INTERNAL "" FORCE)
    set(BUILD_TESTING OFF CACHE INTERNAL "" FORCE)
    set(SQLITE3_SUPPORT OFF CACHE INTERNAL "" FORCE)
    #set(ENABLE_OLD_LOG_MACROS OFF CACHE INTERNAL "" FORCE)  doesn't work
    set(FASTDDS_STATISTICS OFF CACHE INTERNAL "" FORCE)
    # Enforce NO_TLS to disable SSL: if OpenSSL is found, it will be linked to, and we don't want it!
    set(NO_TLS ON CACHE INTERNAL "" FORCE)

    # Set special values for FastDDS sub directory. This function scope keeps
    # BUILD_SHARED_LIBS from leaking back into librealsense. On Apple, FastDDS
    # must be shared: linking its static archives into librealsense2.dylib
    # crashes in DomainParticipantFactory::create_participant at runtime.
    if(APPLE)
        set(BUILD_SHARED_LIBS ON)
        message(STATUS "FastDDS: shared libraries (required for macOS librealsense2.dylib + DDS)")
    else()
        set(BUILD_SHARED_LIBS OFF)
    endif()
    set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/fastdds/fastdds_install)
    set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/fastdds/fastdds_install)

    # Populate first so the Apple transport patch is applied before FastDDS is
    # configured or compiled.
    FetchContent_GetProperties(fastdds)
    if(NOT fastdds_POPULATED)
        FetchContent_Populate(fastdds)
    endif()

    if(APPLE)
        if(NOT EXISTS "${_FASTDDS_MACOS_REUSEPORT_PATCH}")
            message(FATAL_ERROR "FastDDS macOS SO_REUSEPORT patch is missing: ${_FASTDDS_MACOS_REUSEPORT_PATCH}")
        endif()
        if(NOT EXISTS "${fastdds_SOURCE_DIR}/src/cpp/rtps/transport/UDPv4Transport.cpp")
            message(FATAL_ERROR "FastDDS source tree is incomplete: ${fastdds_SOURCE_DIR}")
        endif()

        # BSD patch can auto-detect a reversed patch even with -R --dry-run,
        # producing a false "already applied" result on clean sources. Check
        # the semantic marker in both transport files instead.
        set(_fastdds_udp4 "${fastdds_SOURCE_DIR}/src/cpp/rtps/transport/UDPv4Transport.cpp")
        set(_fastdds_udp6 "${fastdds_SOURCE_DIR}/src/cpp/rtps/transport/UDPv6Transport.cpp")
        file(READ "${_fastdds_udp4}" _fastdds_udp4_contents)
        file(READ "${_fastdds_udp6}" _fastdds_udp6_contents)
        string(FIND "${_fastdds_udp4_contents}" "defined(__QNX__) || defined(__APPLE__)" _fastdds_udp4_marker)
        string(FIND "${_fastdds_udp6_contents}" "defined(__QNX__) || defined(__APPLE__)" _fastdds_udp6_marker)
        if(NOT _fastdds_udp4_marker EQUAL -1 AND NOT _fastdds_udp6_marker EQUAL -1)
            message(STATUS "FastDDS macOS SO_REUSEPORT patch already applied")
        else()
            execute_process(
                COMMAND patch -p1 --forward --dry-run -i "${_FASTDDS_MACOS_REUSEPORT_PATCH}"
                WORKING_DIRECTORY "${fastdds_SOURCE_DIR}"
                RESULT_VARIABLE _fastdds_patch_dry_run_rc
                OUTPUT_VARIABLE _fastdds_patch_dry_run_out
                ERROR_VARIABLE _fastdds_patch_dry_run_err)
            if(NOT _fastdds_patch_dry_run_rc EQUAL 0)
                message(FATAL_ERROR
                    "FastDDS macOS SO_REUSEPORT patch dry-run failed (rc=${_fastdds_patch_dry_run_rc}): "
                    "${_fastdds_patch_dry_run_err}${_fastdds_patch_dry_run_out}")
            endif()
            execute_process(
                COMMAND patch -p1 --forward -i "${_FASTDDS_MACOS_REUSEPORT_PATCH}"
                WORKING_DIRECTORY "${fastdds_SOURCE_DIR}"
                RESULT_VARIABLE _fastdds_patch_rc
                OUTPUT_VARIABLE _fastdds_patch_out
                ERROR_VARIABLE _fastdds_patch_err)
            if(NOT _fastdds_patch_rc EQUAL 0)
                message(FATAL_ERROR
                    "FastDDS macOS SO_REUSEPORT patch failed (rc=${_fastdds_patch_rc}): "
                    "${_fastdds_patch_err}${_fastdds_patch_out}")
            endif()
            message(STATUS "FastDDS macOS SO_REUSEPORT patch applied")
        endif()
    endif()

    if(NOT TARGET fastrtps)
        add_subdirectory(${fastdds_SOURCE_DIR} ${fastdds_BINARY_DIR})
    endif()

    # GCC 14 / libstdc++-15 (Ubuntu 26.04 "resolute") removed transitive <cstdint>
    # includes from many std headers. FastDDS 2.10.4 uses uint8_t (e.g. in
    # DDSFilterCompoundCondition.hpp) without explicitly including <cstdint>,
    # which fails to compile. Force-include <cstdint> ONLY on FastDDS's own
    # targets, and only for their C++ translation units: <cstdint> is a C++
    # header, so applying it directory-wide (add_compile_options) breaks C
    # compilation elsewhere in the tree (e.g. third-party/glad/glad.c in
    # realsense2-gl fatal-errors with "cstdint: No such file"). PRIVATE keeps it
    # off consumers; harmless on older toolchains.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        foreach(_fastdds_tgt fastcdr fastrtps foonathan_memory)
            if(TARGET ${_fastdds_tgt})
                target_compile_options(${_fastdds_tgt} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-include cstdint>)
            endif()
        endforeach()
    endif()

    # Mark new options from FetchContent as advanced options
    mark_as_advanced(FETCHCONTENT_SOURCE_DIR_FASTDDS)
    mark_as_advanced(FETCHCONTENT_UPDATES_DISCONNECTED_FASTDDS)

    # place FastDDS project with other 3rd-party projects
    set_target_properties(fastcdr fastrtps foonathan_memory PROPERTIES
                          FOLDER "3rd Party/fastdds")

    list(POP_BACK CMAKE_MESSAGE_INDENT) # Unindent outputs

    add_library(dds INTERFACE)
    target_link_libraries( dds INTERFACE fastcdr fastrtps )
    
    disable_third_party_warnings(fastcdr)  
    disable_third_party_warnings(fastrtps)  

    add_definitions(-DBUILD_WITH_DDS)

    if(APPLE)
        # foonathan_memory remains a static vendor archive in the observed
        # FastDDS build; only fastrtps and fastcdr need dylib RPATH metadata.
        foreach(_dds_tgt fastrtps fastcdr)
            if(TARGET ${_dds_tgt})
                set_target_properties(${_dds_tgt} PROPERTIES
                    BUILD_RPATH "@loader_path"
                    INSTALL_RPATH "@loader_path"
                    MACOSX_RPATH ON
                    INSTALL_NAME_DIR "@rpath")
            endif()
        endforeach()
    endif()

    install(TARGETS dds fastrtps eProsima_atomic EXPORT realsense2Targets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    
    # fastcdr is installed separately because it cannot be exported to realsense2Targets - it is already exported in fastdds
    install(TARGETS fastcdr
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    message(CHECK_PASS "Done")
endfunction()


pop_security_flags()

# Trigger the FastDDS build
get_fastdds()

if(BUILD_WITH_DDS)
   set(REALSENSE2_DDS_DEPENDENCIES
   	 "include(CMakeFindDependencyMacro)\n
	  find_dependency(fastcdr CONFIG REQUIRED)\n
	  find_dependency(foonathan_memory CONFIG REQUIRED)\n"
	  )
else()
  set(REALSENSE2_DDS_DEPENDENCIES "")
endif()

push_security_flags()
