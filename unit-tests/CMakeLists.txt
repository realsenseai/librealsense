cmake_minimum_required(VERSION 3.10)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(DEPENDENCIES realsense2)

find_package (Python3 COMPONENTS Interpreter Development)
if (NOT ${BUILD_EASYLOGGINGPP})
    message(FATAL_ERROR "Unit tests are not supported without BUILD_EASYLOGGINGPP; Check BUILD_EASYLOGGINGPP to run them.")
endif()

# Set <empty> on exit
function(is_file_empty empty filename)
    set (data "")
    set(res TRUE)
    if(EXISTS ${filename})
        # Travis has CMake 3.5.1... no SIZE support!
        #file(SIZE ${filename} data_length)
        file(READ ${filename} data LIMIT 2 HEX)
        string(LENGTH "${data}" data_length)
        if (NOT ${data_length} STREQUAL "0")
            set(res FALSE)
    #else()
        #    message(STATUS "${data_length}  bytes from  ${filename}")
        endif()
    endif()
    set(empty ${res} PARENT_SCOPE)
endfunction()

# General function for downloading files
function(dl_file src dir filename sha1)
    get_filename_component( dest ${dir} ABSOLUTE BASE_DIR ${CMAKE_CURRENT_BINARY_DIR} )
    is_file_empty( empty ${dest}/${filename} )
    #message(STATUS "empty= ${empty}")
    if( ${empty} )
        message(STATUS "Downloading '${filename}' into '${dest}'")
        #message(STATUS "... from '${src}/${filename}'")
        if( NOT sha1 )
            file( DOWNLOAD "${src}/${filename}" "${dest}/${filename}"
                  STATUS status )
        else()
            file(DOWNLOAD "${src}/${filename}" "${dest}/${filename}"
                EXPECTED_HASH SHA1=${sha1}
                STATUS status)
        endif()
        list( GET status 0 code )
        list( GET status 1 message )
        #message( STATUS " status = ${status}  code = ${code}  message = ${message}" )
        if( code )
            #message( STATUS "... from '${src}/${filename}'" )
            message( SEND_ERROR " ${message}")
        endif()
    endif()
endfunction()


if(BUILD_UNIT_TESTS)
    # Unit tests such as test-algo-projection-distortion/-yuy2-conversion/-cuda-align
    # #include the shared src/cuda/*.cuh headers directly (to exercise
    # rscuda::pointcloud_cuda_helper etc.) and are compiled as plain C++ translation
    # units, not through the HIP compiler -- so they do not automatically inherit the
    # target-scoped `target_include_directories(${LRS_TARGET} PRIVATE ${HIP_INCLUDE_DIRS})`
    # set up for the realsense2 library itself in CMake/global_config.cmake. Re-declaring
    # it here with directory-scoped include_directories() only affects targets created in
    # this subdirectory (the unit-tests tree) and below, so it does not reintroduce the
    # whole-build include pollution that was previously flagged and removed from
    # CMake/hip_config.cmake.
    if(BUILD_WITH_HIP)
        include_directories(${HIP_INCLUDE_DIRS})
    endif()

    include(./unit-tests.cmake)
endif()



