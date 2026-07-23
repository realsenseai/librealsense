// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include <rsutils/os/os.h>

namespace rsutils
{
    namespace os 
    {
        std::string get_os_name()
        {
            #ifdef _WIN32
            return "Windows";
            #else
            #ifdef __APPLE__
            return "Mac OS";
            #else
            #ifdef __linux__
            return "Linux";
            #else
            return "Unknown";
            #endif
            #endif
            #endif
        }

        std::string cpu_arch()
        {
            #if defined( _M_X64 ) || defined( __x86_64__ )
            return "x86_64";
            #elif defined( _M_ARM64 ) || defined( __aarch64__ )
            return "arm64";
            #elif defined( _M_IX86 ) || defined( __i386__ )
            return "x86";
            #elif defined( __arm__ )
            return "arm";
            #else
            return "unknown";
            #endif
        }

        std::string get_platform_name()
        {
            #ifdef _WIN64
            return "Windows amd64";
            #elif _WIN32
            return "Windows x86";
            #elif __linux__
            #ifdef __arm__
            return "Linux arm";
            #else
            return "Linux amd64";
            #endif
            #elif __APPLE__
            return "Mac OS";
            #elif __ANDROID__
            return "Linux arm";
            #else
            return "";
            #endif

        }

    }  
}

