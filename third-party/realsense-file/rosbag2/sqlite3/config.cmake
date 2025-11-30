set(SQLITE3_VERSION "3.49.1")
set(SQLITE3_DOWNLOAD_URL "https://sqlite.org/2025/sqlite-amalgamation-3490100.zip")
set(SQLITE3_SHA3_256 "e7eb4cfb2d95626e782cfa748f534c74482f2c3c93f13ee828b9187ce05b2da7")

include(FetchContent)
FetchContent_Declare(sqlite3_ext 
            URL ${SQLITE3_DOWNLOAD_URL} 
            URL_HASH SHA3_256=${SQLITE3_SHA3_256}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE )
FetchContent_MakeAvailable(sqlite3_ext)

# Just set the variable with the main sqlite3 source and header files
set(SOURCE_FILES_SQLITE3 
    ${sqlite3_ext_SOURCE_DIR}/sqlite3.c
)

set(HEADER_FILES_SQLITE3 
    ${sqlite3_ext_SOURCE_DIR}/sqlite3.h
    ${sqlite3_ext_SOURCE_DIR}/sqlite3ext.h
)

set(HEADER_DIR_SQLITE3 
    ${sqlite3_ext_SOURCE_DIR}
)
