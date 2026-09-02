// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "rsutils/accelerators/gpu.h"
#include <rsutils/easylogging/easyloggingpp.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace rsutils {

    // Probe whether a CUDA-capable GPU is usable on this machine.
    //
    // We dlopen / LoadLibrary the CUDA Driver library (libcuda.so.1 / nvcuda.dll) and
    // call cuInit(0) + cuDeviceGetCount().  This is deliberately decoupled from the
    // build-time RS2_USE_CUDA flag and from libcudart:
    //   - A binary built without CUDA can still detect a working GPU and surface a
    //     hint to rebuild for GPU acceleration.
    //   - A binary built with CUDA on Jetson can still load (and answer "no GPU here")
    //     on a system without the CUDA stack -- the function itself has no link-time
    //     dependency on libcuda / libcudart, so the dynamic linker does not fail at
    //     process startup.
    //
    // Both cuInit() and cuDeviceGetCount() are called: cuInit() alone returns success
    // when the driver loads, even with zero visible devices (e.g. CUDA_VISIBLE_DEVICES
    // is empty or all devices are masked).  Requiring count > 0 matches the prior
    // semantics of cudaGetDeviceCount() > 0.  Result is cached for the lifetime of
    // the process.
    //
    // Calling convention: CUDA's CUDAAPI macro is __stdcall on 32-bit Windows and is
    // a no-op on x64 Windows / POSIX.  Wrong convention on x86 would corrupt the
    // stack across the call.
    //
    // Not unit-tested directly: probe_cuda_driver() is static, its result is captured
    // in a magic-static cache on first call, and it talks to the live OS driver.
    // Mocking would require dependency injection of cuInit/cuDeviceGetCount pointers
    // through the public API, which is too much surface for a one-shot probe.  Coverage
    // comes from CI smoke tests on the Jetson runners (positive: driver+device present)
    // and on x86 / Windows CI agents without an NVIDIA driver (negative: dlopen returns
    // NULL).
    static bool probe_cuda_driver()
    {
#ifdef _WIN32
        using cu_init_t          = int ( __stdcall * )( unsigned int );
        using cu_device_count_t  = int ( __stdcall * )( int * );
        HMODULE handle = LoadLibraryA( "nvcuda.dll" );
        if( ! handle )
        {
            LOG_INFO( "CUDA driver library (nvcuda.dll) not found - GPU acceleration unavailable." );
            return false;
        }
        auto cu_init  = reinterpret_cast< cu_init_t         >( GetProcAddress( handle, "cuInit" ) );
        auto cu_count = reinterpret_cast< cu_device_count_t >( GetProcAddress( handle, "cuDeviceGetCount" ) );
#else
        using cu_init_t          = int ( * )( unsigned int );
        using cu_device_count_t  = int ( * )( int * );
        void * handle = dlopen( "libcuda.so.1", RTLD_LAZY );
        if( ! handle )
        {
            LOG_INFO( "CUDA driver library (libcuda.so.1) not found - GPU acceleration unavailable." );
            return false;
        }
        auto cu_init  = reinterpret_cast< cu_init_t         >( dlsym( handle, "cuInit" ) );
        auto cu_count = reinterpret_cast< cu_device_count_t >( dlsym( handle, "cuDeviceGetCount" ) );
#endif

        // Sentinel -1 marks "call not made yet" so the diagnostic below can
        // distinguish a missing symbol from a non-zero CUresult.
        int init_rc  = -1;
        int count_rc = -1;
        int count    = 0;

        if( cu_init )
            init_rc = cu_init( 0 );
        if( init_rc == 0 && cu_count )
            count_rc = cu_count( &count );

#ifdef _WIN32
        FreeLibrary( handle );
#else
        dlclose( handle );
#endif

        bool have_device = ( init_rc == 0 && count_rc == 0 && count > 0 );

        if( have_device )
            LOG_INFO( "CUDA driver detected with " << count << " visible device(s) - GPU acceleration available." );
        else if( ! cu_init )
            LOG_INFO( "CUDA driver loaded but cuInit symbol not found - GPU acceleration unavailable." );
        else if( init_rc != 0 )
            LOG_INFO( "cuInit returned CUresult " << init_rc << " - GPU acceleration unavailable." );
        else if( ! cu_count )
            LOG_INFO( "CUDA driver initialised but cuDeviceGetCount symbol not found - GPU acceleration unavailable." );
        else if( count_rc != 0 )
            LOG_INFO( "cuDeviceGetCount returned CUresult " << count_rc << " - GPU acceleration unavailable." );
        else
            LOG_INFO( "CUDA driver initialised but zero visible devices - GPU acceleration unavailable." );
        return have_device;
    }

    // Probe whether the first CUDA device is an integrated GPU (unified memory / Tegra).
    //
    // Uses the CUDA Driver API attribute CU_DEVICE_ATTRIBUTE_INTEGRATED (value 18 - we
    // hard-code it to avoid a compile-time dependency on cuda.h, matching the dlopen
    // approach of probe_cuda_driver()). cuDeviceGet(0) returns the first device's handle
    // (CUdevice is a plain int); cuDeviceGetAttribute fills 1 for integrated, 0 for
    // discrete. Any failure (no driver, no device, missing symbol) yields false, so the
    // zero-copy path stays off unless we positively confirm an integrated GPU.
    static bool probe_cuda_integrated()
    {
        // Cheap short-circuit: no usable device -> definitely not integrated.
        if( ! rs2_is_cuda_available() )
            return false;

        // CU_DEVICE_ATTRIBUTE_INTEGRATED from cuda.h's CUdevice_attribute enum.
        constexpr int CU_DEVICE_ATTRIBUTE_INTEGRATED = 18;

#ifdef _WIN32
        using cu_init_t       = int ( __stdcall * )( unsigned int );
        using cu_device_get_t = int ( __stdcall * )( int *, int );
        using cu_device_attr_t = int ( __stdcall * )( int *, int, int );
        HMODULE handle = LoadLibraryA( "nvcuda.dll" );
        if( ! handle )
            return false;
        auto cu_init    = reinterpret_cast< cu_init_t        >( GetProcAddress( handle, "cuInit" ) );
        auto cu_dev_get = reinterpret_cast< cu_device_get_t  >( GetProcAddress( handle, "cuDeviceGet" ) );
        auto cu_dev_attr = reinterpret_cast< cu_device_attr_t >( GetProcAddress( handle, "cuDeviceGetAttribute" ) );
#else
        using cu_init_t        = int ( * )( unsigned int );
        using cu_device_get_t  = int ( * )( int *, int );
        using cu_device_attr_t = int ( * )( int *, int, int );
        void * handle = dlopen( "libcuda.so.1", RTLD_LAZY );
        if( ! handle )
            return false;
        auto cu_init     = reinterpret_cast< cu_init_t        >( dlsym( handle, "cuInit" ) );
        auto cu_dev_get  = reinterpret_cast< cu_device_get_t  >( dlsym( handle, "cuDeviceGet" ) );
        auto cu_dev_attr = reinterpret_cast< cu_device_attr_t >( dlsym( handle, "cuDeviceGetAttribute" ) );
#endif

        bool integrated = false;
        bool probed = false;  // did we actually read the INTEGRATED attribute?
        if( cu_init && cu_dev_get && cu_dev_attr && cu_init( 0 ) == 0 )
        {
            int dev = 0;
            int value = 0;
            if( cu_dev_get( &dev, 0 ) == 0
                && cu_dev_attr( &value, CU_DEVICE_ATTRIBUTE_INTEGRATED, dev ) == 0 )
            {
                probed = true;
                integrated = ( value != 0 );
            }
        }

#ifdef _WIN32
        FreeLibrary( handle );
#else
        dlclose( handle );
#endif

        // Distinguish a genuine discrete GPU from a probe that could not run (missing driver
        // symbols, cuInit/cuDeviceGet failure) - both leave `integrated` false, but only the
        // former is really "discrete". Otherwise diagnostics are misleading.
        if( ! probed )
            LOG_INFO( "Could not probe CUDA device integrated attribute (driver/symbol/device unavailable) - zero-copy GPU path disabled." );
        else if( integrated )
            LOG_INFO( "CUDA device is integrated (unified memory) - zero-copy GPU path eligible." );
        else
            LOG_INFO( "CUDA device is discrete - zero-copy GPU path disabled (would be a loss over PCIe)." );
        return integrated;
    }

    bool rs2_is_cuda_integrated()
    {
        static bool const cached = probe_cuda_integrated();
        return cached;
    }

    //
    // Probe whether a HIP-capable AMD GPU is usable on this machine.
    //
    // Mirrors probe_cuda_driver() but loads the AMD HIP runtime
    // (libamdhip64.so.7 / amdhip64.dll) at runtime, with the same goals:
    //   - No link-time dependency on libamdhip64; an AMD-less host still loads
    //     this binary cleanly.
    //   - A build with RS2_USE_HIP off can still detect an AMD GPU and surface
    //     a "rebuild for GPU acceleration" hint.
    //
    // HIP exposes a C ABI with the platform default calling convention on both
    // Windows and Linux (no __stdcall shim), so the function pointer types are
    // simpler than the CUDA side.  hipSuccess == 0, matching the CUDA check.
    //
    // We try the SONAME-versioned libamdhip64.so.7 first (ROCm 6.x/7.x ships
    // that as the canonical link target) and fall back to the unversioned
    // libamdhip64.so for older / dev installs.
    static bool probe_hip_driver()
    {
#ifdef _WIN32
        using hip_init_t         = int ( * )( unsigned int );
        using hip_device_count_t = int ( * )( int * );
        HMODULE handle = LoadLibraryA( "amdhip64.dll" );
        if( ! handle )
        {
            LOG_INFO( "HIP runtime library (amdhip64.dll) not found - AMD GPU acceleration unavailable." );
            return false;
        }
        auto hip_init  = reinterpret_cast< hip_init_t         >( GetProcAddress( handle, "hipInit" ) );
        auto hip_count = reinterpret_cast< hip_device_count_t >( GetProcAddress( handle, "hipGetDeviceCount" ) );
#else
        using hip_init_t         = int ( * )( unsigned int );
        using hip_device_count_t = int ( * )( int * );
        void * handle = dlopen( "libamdhip64.so.7", RTLD_LAZY );
        if( ! handle )
            handle = dlopen( "libamdhip64.so", RTLD_LAZY );
        if( ! handle )
        {
            LOG_INFO( "HIP runtime library (libamdhip64.so.7) not found - AMD GPU acceleration unavailable." );
            return false;
        }
        auto hip_init  = reinterpret_cast< hip_init_t         >( dlsym( handle, "hipInit" ) );
        auto hip_count = reinterpret_cast< hip_device_count_t >( dlsym( handle, "hipGetDeviceCount" ) );
#endif

        int init_rc  = -1;
        int count_rc = -1;
        int count    = 0;

        if( hip_init )
            init_rc = hip_init( 0 );
        if( init_rc == 0 && hip_count )
            count_rc = hip_count( &count );

#ifdef _WIN32
        FreeLibrary( handle );
#else
        dlclose( handle );
#endif

        bool have_device = ( init_rc == 0 && count_rc == 0 && count > 0 );

        if( have_device )
            LOG_INFO( "HIP runtime detected with " << count << " visible AMD device(s) - GPU acceleration available." );
        else if( ! hip_init )
            LOG_INFO( "HIP runtime loaded but hipInit symbol not found - AMD GPU acceleration unavailable." );
        else if( init_rc != 0 )
            LOG_INFO( "hipInit returned hipError_t " << init_rc << " - AMD GPU acceleration unavailable." );
        else if( ! hip_count )
            LOG_INFO( "HIP runtime initialised but hipGetDeviceCount symbol not found - AMD GPU acceleration unavailable." );
        else if( count_rc != 0 )
            LOG_INFO( "hipGetDeviceCount returned hipError_t " << count_rc << " - AMD GPU acceleration unavailable." );
        else
            LOG_INFO( "HIP runtime initialised but zero visible devices - AMD GPU acceleration unavailable." );
        return have_device;
    }

    bool rs2_is_hip_available()
    {
        static bool const cached = probe_hip_driver();
        return cached;
    }

    // Function-pointer bundle resolved from the HIP runtime library by
    // open_hip_library_and_get_symbols(), consumed by query_hip_integrated_attribute().
    // Not used outside probe_hip_integrated()'s helpers.
    struct hip_integrated_symbols
    {
        using init_t         = int ( * )( unsigned int );
        using device_count_t = int ( * )( int * );
        using device_attr_t  = int ( * )( int *, int, int );

#ifdef _WIN32
        HMODULE handle = nullptr;
#else
        void * handle = nullptr;
#endif
        init_t         init  = nullptr;
        device_count_t count = nullptr;
        device_attr_t  attr  = nullptr;

        bool resolved() const { return handle && init && count && attr; }
    };

    // Open the HIP runtime and resolve hipInit / hipGetDeviceCount / hipDeviceGetAttribute.
    // Mirrors the dlopen/LoadLibrary fallback chain in probe_hip_driver(); returns a
    // symbols struct with handle == nullptr if the library could not be opened.
    static hip_integrated_symbols open_hip_library_and_get_symbols()
    {
        hip_integrated_symbols s;
#ifdef _WIN32
        s.handle = LoadLibraryA( "amdhip64.dll" );
        if( ! s.handle )
            return s;
        s.init  = reinterpret_cast< hip_integrated_symbols::init_t         >( GetProcAddress( s.handle, "hipInit" ) );
        s.count = reinterpret_cast< hip_integrated_symbols::device_count_t >( GetProcAddress( s.handle, "hipGetDeviceCount" ) );
        s.attr  = reinterpret_cast< hip_integrated_symbols::device_attr_t  >( GetProcAddress( s.handle, "hipDeviceGetAttribute" ) );
#else
        s.handle = dlopen( "libamdhip64.so.7", RTLD_LAZY );
        if( ! s.handle )
            s.handle = dlopen( "libamdhip64.so", RTLD_LAZY );
        if( ! s.handle )
            return s;
        s.init  = reinterpret_cast< hip_integrated_symbols::init_t         >( dlsym( s.handle, "hipInit" ) );
        s.count = reinterpret_cast< hip_integrated_symbols::device_count_t >( dlsym( s.handle, "hipGetDeviceCount" ) );
        s.attr  = reinterpret_cast< hip_integrated_symbols::device_attr_t  >( dlsym( s.handle, "hipDeviceGetAttribute" ) );
#endif
        return s;
    }

    // Counterpart to open_hip_library_and_get_symbols(); no-op if the library was never
    // opened (handle == nullptr).
    static void close_hip_library( const hip_integrated_symbols & s )
    {
        if( ! s.handle )
            return;
#ifdef _WIN32
        FreeLibrary( s.handle );
#else
        dlclose( s.handle );
#endif
    }

    // Runs hipInit -> hipGetDeviceCount -> hipDeviceGetAttribute(attribute_id, device 0)
    // using already-resolved symbols. Sets *probed = true only if the attribute was
    // actually read, so the caller can tell "could not probe" (missing driver/symbol,
    // zero devices) apart from a genuine positive/negative result.
    static bool query_hip_integrated_attribute( const hip_integrated_symbols & s, int attribute_id, bool * probed )
    {
        *probed = false;
        int count = 0;
        if( ! ( s.resolved() && s.init( 0 ) == 0 && s.count( &count ) == 0 && count > 0 ) )
            return false;

        // Unlike the CUDA Driver API (cuDeviceGet then cuDeviceGetAttribute), the HIP
        // Runtime API takes a plain device index directly in hipDeviceGetAttribute, so
        // there is no separate "get device handle" step -- device 0 is used directly.
        int value = 0;
        if( s.attr( &value, attribute_id, 0 ) != 0 )
            return false;

        *probed = true;
        return value != 0;
    }

    // Probe whether the first HIP device is an integrated GPU (unified memory AMD APU,
    // e.g. Ryzen AI / MI300A -- NOT applicable to discrete RDNA3/CDNA3 GPUs). Mirrors
    // probe_cuda_integrated() exactly, using the HIP Runtime API attribute
    // hipDeviceAttributeIntegrated instead of the CUDA Driver API equivalent. Broken into
    // open_hip_library_and_get_symbols() / query_hip_integrated_attribute() /
    // close_hip_library() so no single function mixes library loading, symbol resolution,
    // device probing, and logging.
    static bool probe_hip_integrated()
    {
        // Cheap short-circuit: no usable device -> definitely not integrated.
        if( ! rs2_is_hip_available() )
            return false;

        // hipDeviceAttributeIntegrated from hip_runtime_api.h's hipDeviceAttribute_t enum.
        // Verified = 16 against ROCm 7.2.0 headers. Hard-coded (like
        // CU_DEVICE_ATTRIBUTE_INTEGRATED above) to avoid a compile-time dependency on
        // hip_runtime_api.h, matching the dlopen approach of probe_hip_driver(). Unlike
        // CUDA's Driver API, HIP's runtime-API enum is not a formally ABI-frozen surface
        // across ROCm major versions, so this value should be re-verified if a future ROCm
        // release is found to disagree (e.g. via a one-line program that prints
        // (int)hipDeviceAttributeIntegrated compiled against that release's headers).
        constexpr int HIP_DEVICE_ATTRIBUTE_INTEGRATED = 16;

        hip_integrated_symbols symbols = open_hip_library_and_get_symbols();
        bool probed = false;
        bool integrated = query_hip_integrated_attribute( symbols, HIP_DEVICE_ATTRIBUTE_INTEGRATED, &probed );
        close_hip_library( symbols );

        // Distinguish a genuine discrete GPU from a probe that could not run (missing driver
        // symbols, hipInit/hipGetDeviceCount failure) - both leave `integrated` false, but
        // only the former is really "discrete". Otherwise diagnostics are misleading.
        if( ! probed )
            LOG_INFO( "Could not probe HIP device integrated attribute (driver/symbol/device unavailable) - zero-copy GPU path disabled." );
        else if( integrated )
            LOG_INFO( "HIP device is integrated (unified memory) - zero-copy GPU path eligible." );
        else
            LOG_INFO( "HIP device is discrete - zero-copy GPU path disabled (would be a loss over PCIe)." );
        return integrated;
    }

    bool rs2_is_hip_integrated()
    {
        static bool const cached = probe_hip_integrated();
        return cached;
    }

    bool rs2_is_cuda_available()
    {
        static bool const cached = probe_cuda_driver();
        return cached;
    }

    // Vendor-agnostic entry point used by the 12 frame-conversion call sites in
    // librealsense (align.cpp, pointcloud.cpp, the *-formats-converter.cpp files,
    // backend-v4l2.cpp, backend-hid.cpp).  True if either probe finds a device;
    // each probe caches internally so this is two cheap atomic loads after warm-up.
    bool rs2_is_gpu_available()
    {
        return rs2_is_cuda_available() || rs2_is_hip_available();
    }

} // namespace rsutils
