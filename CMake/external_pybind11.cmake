cmake_minimum_required(VERSION 3.10)
include(ExternalProject)



# We use a function to enforce a scoped variables creation only for the build
# (i.e turn off BUILD_SHARED_LIBS which is used on LRS build as well)
function(get_pybind11)

    message( STATUS #CHECK_START
        "Fetching pybind11..." )
    #list( APPEND CMAKE_MESSAGE_INDENT "  " )  # Indent outputs

    # We want to clone the pybind repo and build it here, during configuration, so we can use it.
    # But ExternalProject_add is limited in that it only does its magic during build.
    # This is possible in CMake 3.12+ with FetchContent and FetchContent_MakeAvailable in 3.14+ (meaning Ubuntu 20)
    # but we need to adhere to CMake 3.10 (Ubuntu 18).
    # So instead, we invoke a new CMake project just to download pybind:
    configure_file( CMake/pybind11-download.cmake.in
                    ${CMAKE_BINARY_DIR}/external-projects/pybind11-download/CMakeLists.txt )
    execute_process( COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
                     WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external-projects/pybind11-download"
                     OUTPUT_QUIET
                     RESULT_VARIABLE configure_ret )
    execute_process( COMMAND "${CMAKE_COMMAND}" --build .
                     WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external-projects/pybind11-download"
                     OUTPUT_QUIET
                     RESULT_VARIABLE build_ret )

    if( configure_ret OR build_ret )
        message( FATAL_ERROR "Failed to download pybind11" )
    endif()

    # Now that it's available, we can refer to it with an actual ExternalProject_add (but notice we're not
    # downloading anything)
    ExternalProject_Add( pybind11
        PREFIX      ${CMAKE_BINARY_DIR}/external-projects/pybind11  # just so it doesn't use build/pybind11-prefix
        SOURCE_DIR  ${CMAKE_BINARY_DIR}/third-party/pybind11
        BINARY_DIR  ${CMAKE_BINARY_DIR}/third-party/pybind11/build
        CMAKE_ARGS
                    -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
                    -DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}
                    -DBUILD_SHARED_LIBS=OFF
                    # Suppress warnings that are meant for the author of the CMakeLists.txt files
                    -Wno-dev   
                    # Just in case!
                    -DPYBIND11_INSTALL=OFF
                    -DPYBIND11_TEST=OFF

        INSTALL_COMMAND ""
        UPDATE_COMMAND ""
        PATCH_COMMAND ""
        TEST_COMMAND ""
        )

    add_subdirectory( "${CMAKE_BINARY_DIR}/third-party/pybind11"
                      "${CMAKE_BINARY_DIR}/third-party/pybind11/build" )

    set_target_properties( pybind11 PROPERTIES FOLDER "3rd Party" )

    # Besides pybind11, any python module will also need to be installed using:
    #     install(
    #         TARGETS ${PROJECT_NAME}
    #         EXPORT pyrealsense2Targets
    #         LIBRARY DESTINATION ${PYTHON_INSTALL_DIR}
    #         ARCHIVE DESTINATION ${PYTHON_INSTALL_DIR}
    #         )
    # But, to do this, we need to define PYTHON_INSTALL_DIR!
    if( CMAKE_VERSION VERSION_LESS 3.12 )
      find_package(PythonInterp REQUIRED)
      find_package(PythonLibs REQUIRED)
      set( PYTHON_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/python${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}/pyrealsense2" CACHE PATH "Installation directory for Python bindings")
    else()
      find_package(Python REQUIRED COMPONENTS Interpreter Development)
      # Default: system/venv site-packages (historical).
      # On Apple + DDS we prefer a *prefix-relative* layout so pyrealsense2,
      # librealsense2, and shared FastDDS stay under CMAKE_INSTALL_PREFIX and
      # resolve via @loader_path (no DYLD_LIBRARY_PATH, no polluting Homebrew).
      if(APPLE AND BUILD_WITH_DDS)
        # CMAKE_INSTALL_LIBDIR is only defined by GNUInstallDirs; without it the
        # path below would silently start at the filesystem root.
        if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
          include(GNUInstallDirs)
        endif()
        set( _pyrealsense2_default_install_dir
             "${CMAKE_INSTALL_LIBDIR}/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}/site-packages/pyrealsense2" )
      else()
        set( _pyrealsense2_default_install_dir "${Python_SITEARCH}/pyrealsense2" )
      endif()
      set( PYTHON_INSTALL_DIR "${_pyrealsense2_default_install_dir}"
           CACHE PATH "Installation directory for Python bindings")
    endif()

    message( STATUS #CHECK_PASS
        "Fetching pybind11 - Done" )
    #list( POP_BACK CMAKE_MESSAGE_INDENT ) # Unindent outputs (requires cmake 3.15)

endfunction()


# We also want a json-compatible pybind interface:
function(get_pybind11_json)

    message( STATUS #CHECK_START
        "Fetching pybind11_json..." )
    #list( APPEND CMAKE_MESSAGE_INDENT "  " )  # Indent outputs

    # We want to clone the pybind repo and build it here, during configuration, so we can use it.
    # But ExternalProject_add is limited in that it only does its magic during build.
    # This is possible in CMake 3.12+ with FetchContent and FetchContent_MakeAvailable in 3.14+ (meaning Ubuntu 20)
    # but we need to adhere to CMake 3.10 (Ubuntu 18).
    # So instead, we invoke a new CMake project just to download pybind:
    configure_file( CMake/pybind11-json-download.cmake.in
                    ${CMAKE_BINARY_DIR}/external-projects/pybind11-json-download/CMakeLists.txt )
    execute_process( COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
                     WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external-projects/pybind11-json-download"
                     OUTPUT_QUIET
                     RESULT_VARIABLE configure_ret )
    execute_process( COMMAND "${CMAKE_COMMAND}" --build .
                     WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external-projects/pybind11-json-download"
                     OUTPUT_QUIET
                     RESULT_VARIABLE build_ret )

    if( configure_ret OR build_ret )
        message( FATAL_ERROR "Failed to download pybind11_json" )
    endif()

    # pybind11_add_module will add the directory automatically (see below)

    message( STATUS #CHECK_PASS
        "Fetching pybind11_json - Done" )
    #list( POP_BACK CMAKE_MESSAGE_INDENT ) # Unindent outputs (requires cmake 3.15)

endfunction()

# Trigger the build
get_pybind11()
get_pybind11_json()

# This function overrides "pybind11_add_module" function,  arguments is same as "pybind11_add_module" arguments
# pybind11_add_module(<name> SHARED [file, file2, ...] )
# Must be declared after pybind11 configuration above
function( pybind11_add_module project_name library_type ...)

    # message( STATUS "adding python module --> ${project_name}" 
    
    # "_pybind11_add_module" is calling the origin pybind11 function    
    _pybind11_add_module( ${ARGV})

    # Force Pybind11 not to share pyrealsense2 resources with other pybind modules.
    # With this definition we force the ABI version to be unique and not risk crashes on different modules.
    # (workaround for RS5-10582; see also https://github.com/pybind/pybind11/issues/2898)
    # NOTE: this workaround seems to be needed for debug compilations only
    target_compile_definitions( ${project_name} PRIVATE -DPYBIND11_COMPILER_TYPE=\"_${project_name}_abi\" )

    target_include_directories( ${project_name} PRIVATE "${CMAKE_BINARY_DIR}/third-party/pybind11-json/include" )

endfunction()
