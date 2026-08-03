macro(info msg)
    message(STATUS "Info: ${msg}")
endmacro()

macro(infoValue variableName)
    info("${variableName}=\${${variableName}}")
endmacro()

macro(config_cxx_flags)
    # We require that the current project (e.g., librealsense) use C++14. However, projects using
    # the library don't need to be C++14 -- they can use C++11. Hence this is PRIVATE and not PUBLIC:
    target_compile_features( ${PROJECT_NAME} PRIVATE cxx_std_14 )
    target_compile_features( ${PROJECT_NAME} INTERFACE cxx_std_11 )
    #set( CMAKE_CUDA_STANDARD ${LRS_CXX_STANDARD} )
endmacro()

# Keep symbols of bundled static archives out of the shared library's dynamic symbol
# table. Otherwise a process that also loads another copy of the same library (e.g.
# ROS 2's FastCDR/FastDDS, sqlite3, lz4) gets its calls interposed across mismatched
# ABIs and crashes. Windows is unaffected (exports are restricted by src/realsense.def).
# Skips targets that don't exist in the current configuration or aren't static archives.
macro(hide_bundled_archive_symbols)
    if(BUILD_SHARED_LIBS AND UNIX AND NOT APPLE)
        foreach(_archive_target ${ARGN})
            if(TARGET ${_archive_target})
                get_target_property(_archive_target_type ${_archive_target} TYPE)
                if(_archive_target_type STREQUAL "STATIC_LIBRARY")
                    target_link_libraries(${LRS_TARGET} PRIVATE "-Wl,--exclude-libs,$<TARGET_FILE_NAME:${_archive_target}>")
                endif()
            endif()
        endforeach()
    endif()
endmacro()

# Companion to hide_bundled_archive_symbols for our own sources that include bundled
# third-party headers: without it, inline/template/vtable symbols of those headers get
# re-emitted (and exported) from our translation units, bypassing the archive exclusion.
macro(hide_sources_symbols)
    if(BUILD_SHARED_LIBS AND UNIX AND NOT APPLE)
        set_source_files_properties(${ARGN} PROPERTIES COMPILE_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden")
    endif()
endmacro()

macro(config_crt)
    if(BUILD_WITH_STATIC_CRT)
        foreach(flag_var
                CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
                CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
            if(${flag_var} MATCHES "/MD")
                string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
            endif(${flag_var} MATCHES "/MD")
        endforeach(flag_var)
    endif(BUILD_WITH_STATIC_CRT)
endmacro()
