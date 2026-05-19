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