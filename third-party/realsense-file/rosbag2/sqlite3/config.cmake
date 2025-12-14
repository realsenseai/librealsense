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
set(HEADER_DIR_SQLITE3 
    ${sqlite3_SOURCE_DIR}
)
