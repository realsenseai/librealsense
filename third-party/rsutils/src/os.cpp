// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include <rsutils/os/os.h>

#if defined( _M_X64 ) || defined( _M_IX86 )
#include <intrin.h>
#elif defined( __x86_64__ ) || defined( __i386__ )
#include <cpuid.h>
#endif

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

        bool cpu_supports_avx2()
        {
            // AVX2 needs CPUID.7:EBX[5], plus AVX (CPUID.1:ECX[28]) with OS-saved YMM state (OSXSAVE, XCR0[2:1])
            #if defined( _M_X64 ) || defined( _M_IX86 )
            int info[4];
            __cpuid( info, 0 );
            if( info[0] < 7 )
                return false;
            __cpuid( info, 1 );
            if( ( info[2] & ( 3 << 27 ) ) != ( 3 << 27 ) || ( _xgetbv( 0 ) & 6 ) != 6 )
                return false;
            __cpuidex( info, 7, 0 );
            return ( info[1] & ( 1 << 5 ) ) != 0;
            #elif defined( __x86_64__ ) || defined( __i386__ )
            unsigned int eax, ebx, ecx, edx;
            if( __get_cpuid_max( 0, nullptr ) < 7 )
                return false;
            __cpuid( 1, eax, ebx, ecx, edx );
            if( ( ecx & ( 3u << 27 ) ) != ( 3u << 27 ) )
                return false;
            unsigned int xcr0, xcr0_hi;
            __asm__ volatile( "xgetbv" : "=a"( xcr0 ), "=d"( xcr0_hi ) : "c"( 0 ) );  // _xgetbv needs -mxsave on GCC
            if( ( xcr0 & 6 ) != 6 )
                return false;
            __cpuid_count( 7, 0, eax, ebx, ecx, edx );
            return ( ebx & ( 1u << 5 ) ) != 0;
            #else
            return false;
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

