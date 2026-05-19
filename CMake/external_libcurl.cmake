if(CHECK_FOR_UPDATES)

    pop_security_flags() # remove security flags
    
    include(ExternalProject)
    message(STATUS "Building libcurl enabled")
    
    set(CURL_FLAGS -DBUILD_CURL_EXE=OFF -DBUILD_SHARED_LIBS=OFF -DUSE_WIN32_LDAP=OFF -DHTTP_ONLY=ON -DCURL_ZLIB=OFF -DCURL_DISABLE_CRYPTO_AUTH=ON -DCURL_USE_LIBSSH2=OFF -DBUILD_TESTING=OFF -DBUILD_LIBCURL_DOCS=OFF -DBUILD_MISC_DOCS=OFF -DENABLE_CURL_MANUAL=OFF -DCURL_USE_LIBPSL=OFF )
    if (WIN32)
        set(CURL_FLAGS ${CURL_FLAGS} -DCURL_STATIC_CRT=ON )
    endif()
    
    # Add SSL library flag
    if (WIN32)
        set(CURL_FLAGS ${CURL_FLAGS} -DCURL_USE_SCHANNEL=ON )
    else()
        set(CURL_FLAGS ${CURL_FLAGS} -DCURL_USE_OPENSSL=ON )
    endif()
    
    ExternalProject_Add(
        libcurl
        PREFIX libcurl
        GIT_REPOSITORY "https://github.com/curl/curl.git"
        GIT_TAG "curl-8_8_0"
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
                    -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/libcurl/libcurl_install
                    -DCMAKE_INSTALL_LIBDIR=lib
                    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
                    -DANDROID_ABI=${ANDROID_ABI}
                    -DANDROID_STL=${ANDROID_STL} ${CURL_FLAGS}
        UPDATE_COMMAND ""
        PATCH_COMMAND ""
        TEST_COMMAND ""
    )

    set(CURL_DEBUG_TARGET_NAME "libcurl-d")
    set(CURL_RELEASE_TARGET_NAME "libcurl")
    add_library(curl INTERFACE)
    add_definitions(-DCURL_STATICLIB) # Mandatory for building libcurl as static lib

    target_include_directories(curl INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/libcurl/libcurl_install/include>)
        
    target_link_libraries(curl INTERFACE debug ${CMAKE_CURRENT_BINARY_DIR}/libcurl/libcurl_install/lib/${CURL_DEBUG_TARGET_NAME}${CMAKE_STATIC_LIBRARY_SUFFIX})
    target_link_libraries(curl INTERFACE optimized ${CMAKE_CURRENT_BINARY_DIR}/libcurl/libcurl_install/lib/${CURL_RELEASE_TARGET_NAME}${CMAKE_STATIC_LIBRARY_SUFFIX})
    
   # libcurl required libs per OS 
   # (Linux require that the link dependency will be added after the libcurl link target that use it)
    if (WIN32)
        target_link_libraries(curl INTERFACE ws2_32.lib crypt32.lib)
    else()
        find_package(OpenSSL REQUIRED)
        target_link_libraries(curl INTERFACE OpenSSL::SSL OpenSSL::Crypto)
        # libcurl may be built with libidn2 support — check and link if available
        find_library(IDN2_LIBRARY idn2)
        if(IDN2_LIBRARY)
            target_link_libraries(curl INTERFACE ${IDN2_LIBRARY})
        else()
            message(WARNING "libidn2 not found. If you get linker errors (idn2_free, idn2_lookup_ul), either:\n"
                            "  1. Install it: sudo apt-get install libidn2-dev\n"
                            "  2. Disable update checks: cmake .. -DCHECK_FOR_UPDATES=OFF")
        endif()
        if (APPLE)
            target_link_libraries(curl INTERFACE "-framework CoreFoundation -framework SystemConfiguration")
        endif()
    endif()

    push_security_flags()
endif() #CHECK_FOR_UPDATES
