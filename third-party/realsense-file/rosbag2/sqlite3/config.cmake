cmake_minimum_required(VERSION 3.10)

# this function is based off https://github.com/sjinks/sqlite3-cmake/releases/tag/v3.49.1 
# with some modifications to fit our needs
# License file: https://github.com/sjinks/sqlite3-cmake/blob/42e4aad2da70e5be84349915882a117742b042c2/LICENSE

function(get_sqlite3)
    set(SQLITE3_VERSION "3.49.1")
    set(SQLITE3_DOWNLOAD_URL "https://sqlite.org/2025/sqlite-amalgamation-3490100.zip")
    set(SQLITE3_SHA3_256 "e7eb4cfb2d95626e782cfa748f534c74482f2c3c93f13ee828b9187ce05b2da7")

    project(sqlite3 VERSION ${SQLITE3_VERSION} LANGUAGES C)

    set(CMAKE_VERBOSE_MAKEFILE ON)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel")
    if(CMAKE_GENERATOR MATCHES "Ninja Multi-Config")
        set(CMAKE_DEFAULT_BUILD_TYPE "Release")
    endif()

    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()

    if(BUILD_SHARED_LIBS)
        set(sqlite3_BUILD_SHARED_LIBS_DEFAULT ON)
    else()
        set(sqlite3_BUILD_SHARED_LIBS_DEFAULT OFF)
    endif()

    option(sqlite3_BUILD_SHARED_LIBS "Build shared libraries" ${sqlite3_BUILD_SHARED_LIBS_DEFAULT})
    option(sqlite3_ENABLE_THREADSAFE "Build a thread-safe library" OFF)
    option(sqlite3_ENABLE_DYNAMIC_EXTENSIONS "Support loadable extensions" ON)
    option(sqlite3_ENABLE_MATH "SQL math functions" ON)
    option(sqlite3_ENABLE_FTS3 "Include FTS3 support" OFF)
    option(sqlite3_ENABLE_FTS4 "Include FTS4 support" ON)
    option(sqlite3_ENABLE_FTS5 "Include FTS5 support" ON)
    option(sqlite3_ENABLE_RTREE "Include rtree support" ON)
    option(sqlite3_ENABLE_SESSION "Enable the session extension" OFF)
    option(sqlite3_OMIT_DEPRECATED "Omit deprecated stuff" ON)

    if (sqlite3_BUILD_SHARED_LIBS)
        set(BUILD_SHARED_LIBS ON)
    else()
        set(BUILD_SHARED_LIBS OFF)
    endif()

    add_library(SQLite3)
    add_library(SQLite::SQLite3 ALIAS SQLite3)

    include(CMakePackageConfigHelpers)
    include(CheckCSourceRuns)
    include(CheckFunctionExists)
    include(CheckIncludeFiles)
    include(CheckLibraryExists)
    include(CheckSymbolExists)
    include(CheckTypeSize)
    include(FetchContent)
    include(GNUInstallDirs)

    FetchContent_Declare(sqlite3_ext URL ${SQLITE3_DOWNLOAD_URL} URL_HASH SHA3_256=${SQLITE3_SHA3_256})
    FetchContent_MakeAvailable(sqlite3_ext)

    target_include_directories(SQLite3
        PUBLIC
            $<INSTALL_INTERFACE:include>
            $<BUILD_INTERFACE:${sqlite3_ext_SOURCE_DIR}>
        PRIVATE
            ${sqlite3_ext_SOURCE_DIR}
    )

    #function(check_and_define items check_function)
    #    foreach(item ${${items}})
    #        string(TOUPPER ${item} item_upper)
    #        string(MAKE_C_IDENTIFIER ${item_upper} item_upper)
    #        if(${check_function} STREQUAL "check_include_files")
    #            check_include_files(${item} ${item_upper})
    #        elseif(${check_function} STREQUAL "check_type_size")
    #            check_type_size(${item} ${item_upper})
    #        elseif(${check_function} STREQUAL "check_function_exists")
    #            check_function_exists(${item} ${item_upper})
    #        endif()
    #        if(${item_upper})
    #            target_compile_definitions(SQLite3 PRIVATE "HAVE_${item_upper}=1")
    #        endif()
    #    endforeach()
    #    unset(${items})
    #endfunction()
    #
    #set(HEADERS "dlfcn.h" "stdint.h" "inttypes.h" "utime.h")
    #check_and_define(HEADERS check_include_files)
    #
    #set(TYPES int8_t uint8_t int16_t uint16_t uint32_t)
    #check_and_define(TYPES check_type_size)
    #
    #set(FUNCS fdatasync usleep fullfsync localtime_r gmtime_r strerror_r posix_fallocate nanosleep)
    #check_and_define(FUNCS check_function_exists)
    #
    #if(STRERROR_R)
    #    target_compile_definitions(SQLite3 PRIVATE HAVE_STRERROR_R=1 HAVE_DECL_STRERROR_R=1)
    #    check_c_source_runs("
    #    #include <string.h>
    #    int main() {
    #        return (strerror_r(0, (char*)0, 0) == (char*)0);
    #    }" HAVE_GNU_STRERROR_R)
    #    if(HAVE_GNU_STRERROR_R)
    #        target_compile_definitions(SQLite3 PRIVATE STRERROR_R_CHAR_P=1)
    #    endif()
    #endif()

    target_sources(SQLite3 PRIVATE ${sqlite3_ext_SOURCE_DIR}/sqlite3.c)

    #if(ENABLE_DYNAMIC_EXTENSIONS)
    #    check_library_exists(dl dlopen "" HAVE_DLOPEN)
    #    if(NOT HAVE_DLOPEN)
    #        message(FATAL "No dlopen() in dl")
    #    endif()
    #else()
    #    set(HAVE_DLOPEN OFF)
    #endif()
    #
    #if(ENABLE_MATH)
    #    check_library_exists(m ceil "" HAVE_CEIL_IN_M)
    #    if(NOT HAVE_CEIL_IN_M)
    #        message(FATAL "No ceil() in m")
    #    endif()
    #else()
    #    set(HAVE_CEIL_IN_M OFF)
    #endif()
    #
    #if(ENABLE_FTS5)
    #    check_library_exists(m log "" HAVE_LOG_IN_M)
    #    if(NOT HAVE_LOG_IN_M)
    #        message(FATAL "No log() in m")
    #    endif()
    #else()
    #    set(HAVE_LOG_IN_M OFF)
    #endif()

    target_compile_definitions(SQLite3 PRIVATE
        $<IF:$<BOOL:${sqlite3_ENABLE_THREADSAFE}>,SQLITE_THREADSAFE=1 _REENTRANT=1,SQLITE_THREADSAFE=0>
        $<$<NOT:$<BOOL:${sqlite3_ENABLE_DYNAMIC_EXTENSIONS}>>:SQLITE_OMIT_LOAD_EXTENSION=1>
        $<$<BOOL:${sqlite3_ENABLE_MATH}>:SQLITE_ENABLE_MATH_FUNCTIONS>
        $<$<AND:$<BOOL:${sqlite3_ENABLE_FTS3}>,$<NOT:$<BOOL:${sqlite3_ENABLE_FTS4}>>>:SQLITE_ENABLE_FTS3>
        $<$<BOOL:${sqlite3_ENABLE_FTS4}>:SQLITE_ENABLE_FTS4>
        $<$<BOOL:${sqlite3_ENABLE_FTS5}>:SQLITE_ENABLE_FTS5>
        $<$<BOOL:${sqlite3_ENABLE_RTREE}>:SQLITE_ENABLE_RTREE SQLITE_ENABLE_GEOPOLY>
        $<$<BOOL:${sqlite3_ENABLE_SESSION}>:SQLITE_ENABLE_SESSION SQLITE_ENABLE_PREUPDATE_HOOK>
        $<$<BOOL:${sqlite3_OMIT_DEPRECATED}>:SQLITE_OMIT_DEPRECATED>
        $<$<BOOL:${HAVE_DLOPEN}>:HAVE_DLOPEN=1>
        $<$<CONFIG:Debug>:SQLITE_DEBUG SQLITE_ENABLE_SELECTTRACE SQLITE_ENABLE_WHERETRACE>
    )

    target_compile_definitions(SQLite3 PRIVATE SQLITE_ENABLE_EXPLAIN_COMMENTS SQLITE_DQS=0 SQLITE_ENABLE_DBPAGE_VTAB SQLITE_ENABLE_STMTVTAB SQLITE_ENABLE_DBSTAT_VTAB)

    target_link_libraries(SQLite3
        $<$<BOOL:${HAVE_DLOPEN}>:dl>
        $<$<OR:$<BOOL:${HAVE_CEIL_IN_M}>,$<BOOL:${HAVE_LOG_IN_M}>>:m>
    )

    set(SQLITE3_HEADERS ${sqlite3_ext_SOURCE_DIR}/sqlite3.h ${sqlite3_ext_SOURCE_DIR}/sqlite3ext.h)
    set_target_properties(SQLite3 PROPERTIES
        VERSION "0.8.6"
        SOVERSION "0"
        PUBLIC_HEADER "${SQLITE3_HEADERS}"
        OUTPUT_NAME ${PROJECT_NAME}
        WINDOWS_EXPORT_ALL_SYMBOLS ON # For Windows DLLs - our custom addition
    )

    install(TARGETS SQLite3
        EXPORT sqlite3-targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT sqlite3-targets
        FILE sqlite3-targets.cmake
        NAMESPACE SQLite::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sqlite3
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/sqlite3-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/sqlite3-config-version.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3-config.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sqlite3
    )
endfunction()

get_sqlite3()