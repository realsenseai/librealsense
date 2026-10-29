librealsense can move some frame processing off the plain CPU path: CUDA kernels for format conversion, alignment and point clouds, a zero-copy GPU frame API for integrated GPUs, OpenGL (GLSL) versions of several processing blocks, and AVX2 kernels for YUY2 color conversion on x86-64. This page is for developers who build the SDK from source and want to know which switches turn these paths on and what they actually accelerate.

## Build with CUDA

CUDA support is off by default. The option is declared in [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake):

| Option | Default | Effect |
|---|---|---|
| `BUILD_WITH_CUDA` | `OFF` | Defines `RS2_USE_CUDA`, includes [`CMake/cuda_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/cuda_config.cmake) and compiles `src/cuda` and `src/proc/cuda` into the library. |
| `BUILD_WITH_CUDA_ZEROCOPY` | same value as `BUILD_WITH_CUDA` | Defines `RS2_USE_CUDA_ZEROCOPY` (see [Zero-copy frames](#zero-copy-frames)). Configuring fails with `BUILD_WITH_CUDA_ZEROCOPY requires BUILD_WITH_CUDA=ON` if CUDA is off. |
| `BUILD_WITH_HIP` | `OFF` | AMD HIP/ROCm backend. Cannot be combined with CUDA: configuring fails with `BUILD_WITH_CUDA and BUILD_WITH_HIP are mutually exclusive. Please enable only one.` |
| `BUILD_WITH_HIP_ZEROCOPY` | `OFF` | Zero-copy path for HIP; requires `BUILD_WITH_HIP`. |

Configure and build:

```sh
mkdir build && cd build
cmake .. -DBUILD_WITH_CUDA=ON
cmake --build . --config Release
```

To keep CUDA but disable the zero-copy frame memory, add `-DBUILD_WITH_CUDA_ZEROCOPY=OFF`.

Notes from `cuda_config.cmake`:

- It calls `enable_language( CUDA )` and `find_package(CUDA REQUIRED)`, so the CUDA toolkit must be installed and visible to CMake.
- The GPU architecture list depends on the toolkit version (for example sm_72 for Jetson Xavier with CUDA 11.x/12.x, sm_87 for Orin with CUDA 11.4 to 12.x, sm_110 for Jetson Thor with CUDA 13.0 and later). With CMake 3.18 or newer it is passed as `CMAKE_CUDA_ARCHITECTURES`; with older CMake it is turned into `-gencode` flags.

For all other build switches see [[Build Configuration]].

### Runtime check

A CUDA build still runs on machines without a usable GPU. Every CUDA processing block first asks `rsutils::rs2_is_cuda_available()` ([`third-party/rsutils/include/rsutils/accelerators/gpu.h`](https://github.com/realsenseai/librealsense/blob/development/third-party/rsutils/include/rsutils/accelerators/gpu.h)), which loads the NVIDIA driver at runtime and returns true only if it reports at least one device. The result is cached for the process. When it returns false the SDK uses the CPU (SSE / NEON / plain C++) implementation.

On NVIDIA Jetson (detected through `/etc/nv_tegra_release`) the RealSense Viewer shows a notification when it was built with CUDA but the CUDA runtime failed to initialize, and, in a build without CUDA, one suggesting to install the CUDA runtime (no `/usr/local/cuda`) or to rebuild with `-DBUILD_WITH_CUDA=ON` ([`common/viewer.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/viewer.cpp)).

## What CUDA accelerates

Only the paths below call CUDA code at `development`. Each one checks `rs2_is_cuda_available()` first and falls back to the CPU otherwise.

| Processing | Where the switch is | CUDA code |
|---|---|---|
| Align (`rs2::align`, the "Align" block) | `align::create_align` in [`src/proc/align.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/align.cpp) | `align_cuda`, [`src/proc/cuda/cuda-align.cu`](https://github.com/realsenseai/librealsense/blob/development/src/proc/cuda/cuda-align.cu) |
| Point cloud (`rs2::pointcloud`) | `pointcloud::create` in [`src/proc/pointcloud.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/pointcloud.cpp) | `pointcloud_cuda`, [`src/cuda/cuda-pointcloud.cu`](https://github.com/realsenseai/librealsense/blob/development/src/cuda/cuda-pointcloud.cu) |
| YUY2 unpacking to Y16, RGB8, RGBA8, BGR8, BGRA8 | `unpack_yuy2` in [`src/proc/color-formats-converter.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/color-formats-converter.cpp) | `rscuda::unpack_yuy2_cuda`, [`src/cuda/cuda-conversion.cu`](https://github.com/realsenseai/librealsense/blob/development/src/cuda/cuda-conversion.cu) |
| Y8I to two Y8 streams (USB and MIPI layouts) | [`src/proc/y8i-to-y8y8.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/y8i-to-y8y8.cpp), [`src/proc/y8i-to-y8y8-mipi.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/y8i-to-y8y8-mipi.cpp) | `split_frame_y8_y8_from_y8i_cuda`, `split_frame_y8_y8_from_y8i_mipi_cuda` |
| Y12I to two Y16 streams (USB and MIPI layouts) | [`src/proc/y12i-to-y16y16.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/y12i-to-y16y16.cpp), [`src/proc/y12i-to-y16y16-mipi.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/y12i-to-y16y16-mipi.cpp) | `split_frame_y16_y16_from_y12i_cuda` |
| D401 raw dual-RGB debayer (RAW10 to RGB8, crop and scale) | `rggb_converter` in [`src/proc/rggb-converter.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/rggb-converter.cpp) | `rscuda::rggb_debayer_scale_raw10_cuda`, [`src/cuda/cuda-rggb.cu`](https://github.com/realsenseai/librealsense/blob/development/src/cuda/cuda-rggb.cu) |
| "RGB Rectification" filter for dual-RGB devices | `dual_rgb_rectify_filter` in [`src/proc/dual-rgb-rectify-filter.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/dual-rgb-rectify-filter.cpp) | `rscuda::rggb_remap_rgb8_cuda` |

The align and point-cloud blocks log `Using CUDA-optimized align implementation` and `Using CUDA-optimized pointcloud implementation` (SDK log, INFO level) when the CUDA version is selected. Other processing blocks, including the depth post-processing filters described in [[Post Processing Filters]], have no CUDA implementation.

## Zero-copy frames

With `BUILD_WITH_CUDA_ZEROCOPY` the SDK can allocate frame buffers in mapped, pinned host memory so a CUDA kernel can read the frame without a host-to-device copy ([`src/cuda/cuda-frame-memory.cu`](https://github.com/realsenseai/librealsense/blob/development/src/cuda/cuda-frame-memory.cu), [`src/core/frame-data-allocator.h`](https://github.com/realsenseai/librealsense/blob/development/src/core/frame-data-allocator.h)). This is enabled at runtime only when the first CUDA device is an integrated GPU sharing memory with the CPU (`rsutils::rs2_is_cuda_integrated()`, true on Jetson). On a discrete GPU the SDK keeps ordinary memory and the copy path.

The public API ([`include/librealsense2/hpp/rs_frame.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_frame.hpp), [`include/librealsense2/h/rs_frame.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_frame.h)):

| C++ | C | Returns |
|---|---|---|
| `frame::get_data()` | `rs2_get_frame_data` | Host pointer, always valid. |
| `frame::as<rs2::gpu_frame>()`, then `gpu_frame::get_gpu_data()` | `rs2_is_frame_extendable_to(..., RS2_EXTENSION_GPU_FRAME, ...)`, `rs2_get_frame_gpu_data` | The cast succeeds only when the frame is in zero-copy GPU memory; the pointer is a CUDA device pointer to the same buffer. `rs2_get_frame_gpu_data` returns NULL otherwise. |
| `frame::get_gpu_data_or_upload(bool* copied = nullptr)` | `rs2_get_frame_gpu_data_or_upload` | On a CUDA build, a device pointer: zero-copy (`copied` false) or an SDK-managed upload (`copied` true). NULL on a non-CUDA build or when the upload fails (for example, no usable GPU). |

The device pointer refers to the shared frame buffer: treat it as read-only and keep the frame alive until your GPU work is finished.

```cpp
rs2::frame color = pipe.wait_for_frames().get_color_frame();
if( auto gf = color.as< rs2::gpu_frame >() )
    use_on_gpu( gf.get_gpu_data() );                 // zero-copy, no transfer
else if( const void * dev = color.get_gpu_data_or_upload() )
    use_on_gpu( dev );                               // SDK copied the frame to the GPU
else
    use_on_cpu( color.get_data() );                  // built without CUDA, or no usable GPU
```

`use_on_gpu` and `use_on_cpu` stand for your own code.

## The rs-gpu-frame example

[`examples/gpu-frame/rs-gpu-frame.cpp`](https://github.com/realsenseai/librealsense/blob/development/examples/gpu-frame/rs-gpu-frame.cpp) shows which path your build and hardware give you. It is built with the other examples (`BUILD_EXAMPLES`) and needs no CUDA toolchain itself, because it only uses the public API.

The example streams color, skips 30 frames, then prints five lines with the host pointer, whether `as<rs2::gpu_frame>()` succeeded, the device pointer from `get_gpu_data_or_upload()`, and the path:

- `ZERO-COPY (no copy)`: CUDA build with zero-copy on an integrated GPU.
- `UPLOAD (host->device copy)`: CUDA build where frames are not GPU-resident (discrete GPU, or zero-copy not built).
- `NONE (non-CUDA build)`: library built without CUDA, or no GPU the upload could use.

It ends with a summary that starts with `ZERO-COPY ACTIVE`, `CUDA build, but frames are NOT zero-copy-resident` or `Non-CUDA build`. See also the example's [readme](https://github.com/realsenseai/librealsense/blob/development/examples/gpu-frame/readme.md).

## OpenGL (GLSL) processing

`BUILD_GLSL_EXTENSIONS` (default `ON`) builds the separate `realsense2-gl` library from [`src/gl`](https://github.com/realsenseai/librealsense/tree/development/src/gl). The top-level `CMakeLists.txt` adds `src/gl` only when `BUILD_GLSL_EXTENSIONS` is on and `BUILD_EXAMPLES` or `BUILD_PC_STITCHING` is on. `BUILD_PC_STITCHING` switches `BUILD_GLSL_EXTENSIONS` on; `BUILD_GRAPHICAL_EXAMPLES` needs it (the graphical tools link `realsense2-gl`) but sets it only inside `tools/CMakeLists.txt`, too late to add `src/gl`.

The API is in [`include/librealsense2-gl/rs_processing_gl.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2-gl/rs_processing_gl.hpp), namespace `rs2::gl`. Call `rs2::gl::init_processing(...)` (and `rs2::gl::init_rendering()` if you render) first, then use the GL blocks in place of the CPU ones:

- `rs2::gl::yuy_decoder`, `m420_decoder`, `nv12_decoder`, `y411_decoder`
- `rs2::gl::colorizer`, `rs2::gl::pointcloud`, `rs2::gl::align`
- `rs2::gl::uploader` (copies a frame to the GPU), `rs2::gl::camera_renderer`, `rs2::gl::pointcloud_renderer`

Frames produced on the GPU can be detected with `frame.as<rs2::gl::gpu_frame>()`. This is a different extension from the CUDA `rs2::gpu_frame` above. See [`examples/gl/rs-gl.cpp`](https://github.com/realsenseai/librealsense/blob/development/examples/gl/rs-gl.cpp).

## AVX2 path (x86-64)

On x86-64, YUY2 unpacking to RGB8, RGBA8, BGR8 and BGRA8 can use AVX2 kernels from [`src/image-avx.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/image-avx.cpp) *(changed in the next release)*. In v2.59.1 these kernels were compiled only when the compiler defined `__AVX2__`, and Y8 and Y16 output also had AVX2 versions.

How the path is chosen in [`src/proc/color-formats-converter.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/color-formats-converter.cpp):

1. On a CUDA build with a GPU available, the CUDA kernel is used and the CPU paths are skipped.
2. Otherwise, if the build has SSSE3 (`__SSSE3__`), is not Android and targets x86-64, the AVX2 kernels are compiled in (`AVX2_SUPPORT` in [`src/image-avx.h`](https://github.com/realsenseai/librealsense/blob/development/src/image-avx.h)). Only these functions get the AVX2 target attribute, so the rest of the library does not require an AVX2 CPU.
3. At runtime the AVX2 kernel runs when `rsutils::os::cpu_supports_avx2()` is true, the pixel count is a multiple of 32, and the output is not Y8 or Y16. Otherwise the SSSE3 loop is used.

On Unix builds for targets other than ARM, PowerPC and RISC-V, `CMake/unix_config.cmake` adds `-mssse3`; on Windows for non-ARM targets, `CMake/windows_config.cmake` defines `__SSSE3__` *(changed in the next release)*. In v2.59.1 Windows defined it for ARM targets too. In both cases CMake prints `Building with SSE optimizations`. No CMake option is needed to enable the AVX2 path.

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`84e42c1`](https://github.com/realsenseai/librealsense/commit/84e42c1d2ce717c57dcf11f155bbe68bb41bbd1c)._
<!-- /wikigen:footer -->
