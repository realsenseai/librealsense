if(CHECK_FOR_UPDATES OR ENABLE_AI_ASSISTANT OR ENABLE_STATS)

    pop_security_flags() # remove security flags
    
    include(ExternalProject)
    message(STATUS "Building libcurl enabled")
    
    # HTTP(S) only, with no optional dependencies: anything curl finds on its own (e.g. Homebrew's nghttp2/brotli/zstd)
    # would need linking below. HTTP_ONLY and CURL_DISABLE_CRYPTO_AUTH are gone in newer curl, so the protocols and
    # dependencies are also switched off one by one.
    set(CURL_FLAGS -DBUILD_CURL_EXE=OFF -DBUILD_SHARED_LIBS=OFF -DUSE_WIN32_LDAP=OFF -DBUILD_TESTING=OFF -DBUILD_LIBCURL_DOCS=OFF -DBUILD_MISC_DOCS=OFF -DENABLE_CURL_MANUAL=OFF
        -DHTTP_ONLY=ON -DCURL_DISABLE_CRYPTO_AUTH=ON
        -DCURL_DISABLE_DICT=ON -DCURL_DISABLE_FILE=ON -DCURL_DISABLE_FTP=ON -DCURL_DISABLE_GOPHER=ON -DCURL_DISABLE_IMAP=ON
        -DCURL_DISABLE_LDAP=ON -DCURL_DISABLE_LDAPS=ON -DCURL_DISABLE_MQTT=ON -DCURL_DISABLE_POP3=ON -DCURL_DISABLE_RTSP=ON
        -DCURL_DISABLE_SMTP=ON -DCURL_DISABLE_TELNET=ON -DCURL_DISABLE_TFTP=ON -DCURL_DISABLE_WEBSOCKETS=ON
        -DCURL_DISABLE_DIGEST_AUTH=ON
        -DCURL_ZLIB=OFF -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF -DUSE_NGHTTP2=OFF -DCURL_USE_LIBSSH2=OFF -DCURL_USE_LIBPSL=OFF -DUSE_LIBIDN2=OFF )
    if (WIN32)
        set(CURL_FLAGS ${CURL_FLAGS} -DCURL_STATIC_CRT=ON )
    endif()
    
    # Add SSL library flag
    if (WIN32)
        set(CURL_FLAGS ${CURL_FLAGS} -DCURL_USE_SCHANNEL=ON )
    else()
        set(CURL_FLAGS ${CURL_FLAGS} -DCURL_USE_OPENSSL=ON )
        # Build curl against the same OpenSSL we link below (e.g. with both openssl@3 and openssl@4 installed)
        if(OPENSSL_ROOT_DIR)
            set(CURL_FLAGS ${CURL_FLAGS} -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR} )
        endif()
    endif()
    
    set(CURL_DEBUG_TARGET_NAME "libcurl-d")
    set(CURL_RELEASE_TARGET_NAME "libcurl")
    set(CURL_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/libcurl/libcurl_install)

    # Only the library matching the build type is ever produced; declaring both
    # as byproducts would leave one permanently missing, re-running libcurl on every build
    set(CURL_BYPRODUCT_NAME ${CURL_RELEASE_TARGET_NAME})
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CURL_BYPRODUCT_NAME ${CURL_DEBUG_TARGET_NAME})
    endif()

    ExternalProject_Add(
        libcurl
        PREFIX libcurl
        GIT_REPOSITORY "https://github.com/curl/curl.git"
        # 8.19.0: first with OpenSSL 4 support (opaque ASN1 types), last that builds with CMake < 3.18
        GIT_TAG "curl-8_19_0"
        SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/third-party/libcurl
        CMAKE_ARGS  --no-warn-unused-cli
                    -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
                    -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
                    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    -DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}
                    -DCMAKE_C_FLAGS_MINSIZEREL=${CMAKE_C_FLAGS_MINSIZEREL}
                    -DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}
                    -DCMAKE_C_FLAGS_RELWITHDEBINFO=${CMAKE_C_FLAGS_RELWITHDEBINFO}
                    -DCMAKE_CXX_STANDARD_LIBRARIES=${CMAKE_CXX_STANDARD_LIBRARIES}
                    -DCMAKE_INSTALL_PREFIX=${CURL_INSTALL_DIR}
                    -DCMAKE_INSTALL_LIBDIR=lib
                    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
                    -DANDROID_ABI=${ANDROID_ABI}
                    -DANDROID_STL=${ANDROID_STL} ${CURL_FLAGS}
        UPDATE_COMMAND ""
        PATCH_COMMAND ""
        TEST_COMMAND ""
        # Ninja needs to know these are generated; we link them by path, below
        BUILD_BYPRODUCTS ${CURL_INSTALL_DIR}/lib/${CURL_BYPRODUCT_NAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    )

    add_library(curl INTERFACE)
    add_definitions(-DCURL_STATICLIB) # Mandatory for building libcurl as static lib

    target_include_directories(curl INTERFACE $<BUILD_INTERFACE:${CURL_INSTALL_DIR}/include>)
        
    target_link_libraries(curl INTERFACE debug ${CURL_INSTALL_DIR}/lib/${CURL_DEBUG_TARGET_NAME}${CMAKE_STATIC_LIBRARY_SUFFIX})
    target_link_libraries(curl INTERFACE optimized ${CURL_INSTALL_DIR}/lib/${CURL_RELEASE_TARGET_NAME}${CMAKE_STATIC_LIBRARY_SUFFIX})
    
   # libcurl required libs per OS 
   # (Linux require that the link dependency will be added after the libcurl link target that use it)
    if (WIN32)
        target_link_libraries(curl INTERFACE ws2_32.lib crypt32.lib secur32.lib iphlpapi.lib)
    else()
        find_package(OpenSSL REQUIRED)
        target_link_libraries(curl INTERFACE OpenSSL::SSL OpenSSL::Crypto)
        if (APPLE)
            target_link_libraries(curl INTERFACE "-framework CoreFoundation -framework SystemConfiguration")
        endif()
    endif()

    push_security_flags()
endif() #CHECK_FOR_UPDATES OR ENABLE_AI_ASSISTANT OR ENABLE_STATS
