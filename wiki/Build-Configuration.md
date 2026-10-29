### Intro
The RealSense SDK uses the **CMake** eco-system to offer useful customization to customers who wish to build the SDK from source.  
> For example, to generate makefile with `BUILD_EXAMPLES` flag turned-on, use the following command line:
`cmake .. -DBUILD_EXAMPLES=true`
Alternatively, use `cmake-gui` utility to configure and generate your build files.

For build configuration details, please refer to [build-configuration documentation](https://dev.realsenseai.com/docs/build-configuration)

## CMake options

The options below are declared in [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake), which the top-level [`CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/CMakeLists.txt) includes. Pass any of them on the CMake command line with `-D`, for example:

```sh
cmake .. -DBUILD_EXAMPLES=ON -DBUILD_PYTHON_BINDINGS=ON
```

A value passed with `-D` is stored in the build folder's `CMakeCache.txt` and stays in effect for later CMake runs in that folder until you change it.

On Linux and macOS, when `CMAKE_BUILD_TYPE` is not set, the build uses `Release` and puts its libraries and executables under `<build folder>/Release` ([`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/unix_config.cmake)). To get a debug build, pass `-DCMAKE_BUILD_TYPE=Debug`.

<!-- wikigen:table-cmake-options -->
| Option | Default | Description |
|---|---|---|
| `ENABLE_CCACHE` | `ON` | Build with ccache. |
| `BUILD_WITH_CUDA` | `OFF` | Enable CUDA |
| `BUILD_WITH_CUDA_ZEROCOPY` | same as `BUILD_WITH_CUDA` | Enable zero-copy GPU memory path (Jetson/integrated GPU only, requires BUILD_WITH_CUDA). Zero-copy GPU memory path (Jetson / integrated-GPU only). Requires BUILD_WITH_CUDA. Eliminates the per-frame CPU->frame copy (capture) and the GPU host<->device round-trips. Activates at runtime only on integrated GPUs; discrete GPUs keep the copy path. Default OFF so a plain CUDA build is byte-for-byte identical to today. |
| `BUILD_WITH_HIP` | `OFF` | Enable HIP/ROCm for AMD GPUs |
| `BUILD_WITH_HIP_ZEROCOPY` | `OFF` | Enable zero-copy GPU memory path (unified-memory AMD APUs only, requires BUILD_WITH_HIP). Zero-copy GPU memory path for HIP (unified-memory AMD APUs, e.g. Ryzen AI / MI300A, only -- not applicable to discrete RDNA3/CDNA3 GPUs). Requires BUILD_WITH_HIP. Mirrors BUILD_WITH_CUDA_ZEROCOPY: shares the same RS2_USE_CUDA_ZEROCOPY macro and the vendor- neutral try_device_ptr() fast path in rscuda_utils.cuh/cuda-pointcloud.cu, which fall back to the persistent-buffer copy path whenever a buffer is not pinned/mapped -- so enabling this on discrete GPUs is safe (falls back every time) but pointless. |
| `BUILD_GLSL_EXTENSIONS` | `ON` | Build GLSL extensions API |
| `BUILD_WITH_OPENMP` | `OFF` | Use OpenMP |
| `BUILD_EASYLOGGINGPP` | `ON` | Build EasyLogging++ as a part of the build |
| `BUILD_WITH_STATIC_CRT` | `ON` | Build with static link CRT |
| `HWM_OVER_XU` | `ON` | Send HWM commands over UVC XU control |
| `COM_MULTITHREADED` | `ON` | Set OFF to initialize COM library with COINIT_APARTMENTTHREADED (Windows only) |
| `BUILD_SHARED_LIBS` | `ON` | Build shared library |
| `BUILD_UNIT_TESTS` | `OFF` | Build LibCI unit tests. If enabled, additional test data may be downloaded |
| `BUILD_VIEWER_TESTS` | `OFF` | Build automated GUI tests for realsense-viewer, will use FetchContent for the test engine files |
| `BUILD_EXAMPLES` | `ON` | Build examples (not including graphical examples -- see BUILD_GRAPHICAL_EXAMPLES) |
| `BUILD_GRAPHICAL_EXAMPLES` | `ON` | Build graphical examples (Viewer & DQT) -- Implies BUILD_GLSL_EXTENSIONS |
| `BUILD_CV_EXAMPLES` | `OFF` | Build OpenCV examples |
| `BUILD_DLIB_EXAMPLES` | `OFF` | Build DLIB examples - requires DLIB_DIR |
| `BUILD_PCL_EXAMPLES` | `OFF` | Build PCL examples |
| `BUILD_TOOLS` | `ON` | Build tools (fw-updater, etc.) that are not examples |
| `ENFORCE_METADATA` | `OFF` | Require WinSDK with Metadata support during compilation. Windows OS Only |
| `BUILD_PYTHON_BINDINGS` | `OFF` | Build Python bindings |
| `BUILD_LEGACY_PYBACKEND` | `OFF` | Build deprecated Python backend bindings |
| `BUILD_PYTHON_DOCS` | `OFF` | Build Documentation for Python bindings |
| `BUILD_CSHARP_BINDINGS` | `OFF` | Build C# bindings |
| `BUILD_MATLAB_BINDINGS` | `OFF` | Build Matlab bindings |
| `BUILD_UNITY_BINDINGS` | `OFF` | Copy the unity project to the build folder with the required dependencies |
| `BUILD_OPENVINO_EXAMPLES` | `OFF` | Build Intel OpenVINO Toolkit examples - requires INTEL_OPENVINO_DIR |
| `BUILD_OPEN3D_EXAMPLES` | `OFF` | Build Open3D examples |
| `BUILD_OPENNI2_BINDINGS` | `OFF` | Build OpenNI bindings |
| `BUILD_CV_KINFU_EXAMPLE` | `OFF` | Build OpenCV KinectFusion example |
| `FORCE_RSUSB_BACKEND` | `OFF` | Use RS USB backend, mandatory for Win7/MacOS/Android, optional for Linux |
| `FORCE_LIBUVC` | `OFF` | Explicitly turn-on libuvc backend - deprecated, use FORCE_RSUSB_BACKEND instead |
| `FORCE_WINUSB_UVC` | `OFF` | Explicitly turn-on winusb_uvc (for win7) backend - deprecated, use FORCE_RSUSB_BACKEND instead |
| `ANDROID_USB_HOST_UVC` | `OFF` | Build UVC backend for Android - deprecated, use FORCE_RSUSB_BACKEND instead |
| `CHECK_FOR_UPDATES` | `ON` if `NOT APPLE`; `OFF` otherwise | Checks for versions updates. This feature requires OpenSSL installation on Linux/OSX, OSX normally does not come with OpenSSL integrated(Thats why default is OFF on OSX) |
| `ENABLE_AI_ASSISTANT` | `ON` if `NOT APPLE`; `OFF` otherwise | Build the AI Assistant chat panel into the viewer. Same libcurl/OpenSSL dependency profile as CHECK_FOR_UPDATES above, so the same per-OS default applies. |
| `BUILD_WITH_CPU_EXTENSIONS` | `ON` | Enable compiler optimizations using CPU extensions (such as AVX) |
| `BUILD_WITH_NEON` | `ON` if `CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64\|ARM64\|aarch64\|AARCH64)"` | Enable ARM64 NEON optimizations. Enable NEON option only on 64-bit ARM platforms (ARM64/AArch64) |
| `BUILD_WITH_CLOSE_RANGE_DEPTH` | `OFF` if `CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64\|ARM64\|aarch64\|AARCH64)"` | Enable Improved Close Range Depth (Jetson only, requires enhanced-depth library) |
| `UNIT_TESTS_ARGS` | `""` (string) | Command-line arguments to pass to unit-tests-config.py, e.g. '-t <tag> -r <regex>' |
| `ENABLE_EASYLOGGINGPP_ASYNC` | `ON` if `UNIX AND (NOT ANDROID_NDK_TOOLCHAIN_INCLUDED)`; `OFF` otherwise | Switch Logger to Asynchronous Mode (set OFF for Synchronous Mode). Performance improvement with Ubuntu 18/20 |
| `BUILD_PC_STITCHING` | `OFF` | Build pointcloud-stitching example |
| `BUILD_WITH_DDS` | `OFF` | Access camera devices through DDS topics (requires CMake 3.16.3) |
| `BUILD_RS2_ALL` | `ON` | Build realsense2-all static bundle containing all realsense libraries (with BUILD_SHARED_LIBS=OFF) |
| `ENABLE_SECURITY_FLAGS` | `OFF` | Enable additional compiler security flags to enhance the build's security |
| `USE_EXTERNAL_LZ4` | `OFF` | Use externally build LZ4 library instead of building and using the in this repo provided version |
| `USE_EXTERNAL_NLOHMANN_JSON` | `OFF` | Use an externally built nlohmann-json development package instead of downloading it as part of this build |
| `BUILD_ASAN` | `OFF` | Enable AddressSanitizer |
| `BUILD_ROSBAG2` | `ON` | Build and use rosbag2 recording system. temporary flag, should be removed when deprecated ROSBAG1 recording system is removed |
| `ENABLE_STATS` | `ON` | Enable RUM (Real User Monitoring) usage-statistics collection |
<!-- /wikigen:table-cmake-options -->

The table lists each option where `lrs_options.cmake` declares it; some options are declared only inside an `if(...)` block, shown in the Default column. Other files under [`CMake/`](https://github.com/realsenseai/librealsense/tree/development/CMake) can still adjust an option after it is declared, so check them when an option does not behave as listed.

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`0d4fcab`](https://github.com/realsenseai/librealsense/commit/0d4fcabf634650b821200ad38eedaa13aa4cb704)._
<!-- /wikigen:footer -->
