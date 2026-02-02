set(SQLITE3_VERSION "3.49.1")
set(SQLITE3_DOWNLOAD_URL "https://sqlite.org/2025/sqlite-amalgamation-3490100.zip")


if(POLICY CMP0135) # suppress warning for cmake 3.24+
    cmake_policy(SET CMP0135 NEW)
endif()

if (NOT TARGET sqlite3)
    ExternalProject_Add(sqlite3
        URL ${SQLITE3_DOWNLOAD_URL}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
    )
endif()

ExternalProject_Get_Property(sqlite3 SOURCE_DIR)
set(sqlite3_SOURCE_DIR ${SOURCE_DIR})
set(HEADER_DIR_SQLITE3 ${sqlite3_SOURCE_DIR})
set(SQLITE3_SOURCES "${sqlite3_SOURCE_DIR}/sqlite3.c")

add_library(sqlite3_lib STATIC ${SQLITE3_SOURCES})
set_source_files_properties(${SQLITE3_SOURCES} PROPERTIES GENERATED TRUE)

add_dependencies(sqlite3_lib sqlite3)

target_include_directories(sqlite3_lib PUBLIC $<BUILD_INTERFACE:${sqlite3_SOURCE_DIR}>)

if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(sqlite3_lib PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()

