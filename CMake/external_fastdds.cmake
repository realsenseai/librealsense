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

    # Set special values for FastDDS sub directory
    #
    # Link model (important on Apple):
    # - Default (Linux/Windows): static FastDDS, as upstream historically does.
    # - APPLE: build FastDDS as *shared* dylibs. Archiving FastDDS into
    #   librealsense2.dylib breaks DomainParticipantFactory::create_participant
    #   at runtime (EXC_BAD_ACCESS / null PC). Fully-static tools (rs-dds-sniffer)
    #   still work; anything that loads librealsense2 as a shared library does not.
    #   Alternative not chosen: force_load of static archives into the dylib —
    #   more fragile with FetchContent and still couples singleton lifetime to
    #   the host binary. Shared FastDDS matches normal dylib dependency practice.
    if(APPLE)
        # Function-scoped; does not leak to the caller. Of FastDDS's bundled
        # third-parties only fastrtps/fastcdr become dylibs — foonathan_memory
        # is built by its vendor project and stays a static archive
        # (libfoonathan_memory-*.a), verified in the installed prefix.
        set(BUILD_SHARED_LIBS ON)
        message(STATUS "FastDDS: shared libraries (required for macOS librealsense2.dylib + DDS)")
    else()
        set(BUILD_SHARED_LIBS OFF)
    endif()
    set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/fastdds/fastdds_install) 
    set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/fastdds/fastdds_install)  

    # Get fastdds
    FetchContent_MakeAvailable(fastdds)
    
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
        # Runtime search path so tools/python next to / under lib find fastrtps without DYLD_LIBRARY_PATH
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

    # Keep dds INTERFACE in the export set (realdds depends on it) on all platforms.
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
