The librealsense repository ships a set of small C and C++ sample programs in the [`examples/`](https://github.com/realsenseai/librealsense/tree/development/examples) folder. Each one shows a single SDK concept (streaming, alignment, point clouds, recording, sensor control and more) and is a good starting point for your own application. This page explains how to build the samples and lists every sample that the CMake build adds.

For task-oriented snippets see [[API How To]]; for Python samples see [[Python Wrapper]]; for the full list of CMake options see [[Build Configuration]].

## Building the samples

The samples are built as part of the main librealsense CMake build. Two options in [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake) control them:

| Option | Default | Effect |
|---|---|---|
| `BUILD_EXAMPLES` | `ON` | Adds the `examples/` folder (and the `tools/` folder) to the build. |
| `BUILD_GRAPHICAL_EXAMPLES` | `ON` | Builds the samples that open a window (OpenGL/GLFW/ImGui). When it is `OFF`, only the console samples are built. |

The top-level [`CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/CMakeLists.txt) runs `add_subdirectory(examples)` only when `BUILD_EXAMPLES` is on. Inside [`examples/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/examples/CMakeLists.txt), every sample folder is added, and each folder's own `CMakeLists.txt` decides whether its target is created:

- **Always built** (with `BUILD_EXAMPLES`): `rs-hello-realsense`, `rs-gpu-frame`, `rs-callback`, `rs-save-to-disk`, the C samples (`rs-depth`, `rs-color`, `rs-distance`, `rs-infrared`, `rs-composite-ctl`), `rs-on-chip-calib`, `rs-eth-config`, `rs-embedded-filters`, `rs-object-detection`, `rs-composite-option-mock`, `rs-composite-option` and `rs-hdrd-control`.
- **Only with `BUILD_GRAPHICAL_EXAMPLES`**: `rs-software-device`, `rs-capture`, `rs-multicam`, `rs-pointcloud`, `rs-labeled-pointcloud`, `rs-align`, `rs-align-advanced`, `rs-sensor-control`, `rs-measure`, `rs-post-processing`, `rs-record-playback`, `rs-motion` and `rs-hdr`.
- **Only with `BUILD_GRAPHICAL_EXAMPLES` and not on macOS**: `rs-gl` and `rs-align-gl`. These link against the `realsense2-gl` library, which is built when `BUILD_GLSL_EXTENSIONS` is on (default `ON`).

On Android builds, [`CMake/android_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/android_config.cmake) forces both `BUILD_EXAMPLES` and `BUILD_GRAPHICAL_EXAMPLES` to `OFF`.

Example configuration from a build directory inside the repository:

```bash
mkdir build && cd build
# console samples only
cmake .. -DBUILD_EXAMPLES=ON -DBUILD_GRAPHICAL_EXAMPLES=OFF
# or: all samples, including the graphical ones (default)
cmake .. -DBUILD_EXAMPLES=ON -DBUILD_GRAPHICAL_EXAMPLES=ON
cmake --build .
```

Other sample-related options in `CMake/lrs_options.cmake` (`BUILD_CV_EXAMPLES`, `BUILD_PCL_EXAMPLES`, `BUILD_OPEN3D_EXAMPLES`, and others, all `OFF` by default) belong to the wrappers and integrations, not to the `examples/` folder; see [[Build Configuration]].

### Where the binaries land

- **Linux and macOS** ([`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/unix_config.cmake)): executables go to `<build>/<CMAKE_BUILD_TYPE>/`. If no build type is set, it becomes `Release`, so the default is `<build>/Release/`.
- **Windows** ([`CMake/windows_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/windows_config.cmake)): the runtime output directory is the build directory itself; with a multi-configuration generator such as Visual Studio, CMake places each configuration in its own subfolder (for example `<build>/Release/`).

In IDEs that show CMake folders, the C++ samples appear under `Examples` and the C samples under `Examples/C`.

On `make install` (or `cmake --install`), every sample target except `rs-composite-option-mock` is installed to `${CMAKE_INSTALL_BINDIR}`.

## Sample reference

One row per folder added by [`examples/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/examples/CMakeLists.txt), in the order of that file. "Graphical" means the target is built only when `BUILD_GRAPHICAL_EXAMPLES` is on.

<!-- wikigen:table-examples -->
| Example | Binary/target name | What it shows | Link |
|---|---|---|---|
| hello-realsense | `rs-hello-realsense` | Connects to a device and prints the distance to the object in the center of the depth image. | [examples/hello-realsense](https://github.com/realsenseai/librealsense/tree/development/examples/hello-realsense) |
| gpu-frame | `rs-gpu-frame` | Uses the `rs2::gpu_frame` extension to reach the GPU pointer of a frame. | [examples/gpu-frame](https://github.com/realsenseai/librealsense/tree/development/examples/gpu-frame) |
| software-device | `rs-software-device` (graphical) | Creates a `software_device` that feeds a generated depth frame and a color frame made from an embedded RealSense logo image into SDK processing (sync, point cloud). | [examples/software-device](https://github.com/realsenseai/librealsense/tree/development/examples/software-device) |
| capture | `rs-capture` (graphical) | Enables all streams, prints their frame rates and renders them (depth colorized) to the screen. | [examples/capture](https://github.com/realsenseai/librealsense/tree/development/examples/capture) |
| callback | `rs-callback` | Streams with the pipeline callback API and prints a frame counter per stream. | [examples/callback](https://github.com/realsenseai/librealsense/tree/development/examples/callback) |
| save-to-disk | `rs-save-to-disk` | Streams in a console and saves video frames to PNG and their per-frame metadata to CSV. | [examples/save-to-disk](https://github.com/realsenseai/librealsense/tree/development/examples/save-to-disk) |
| multicam | `rs-multicam` (graphical) | Streams and renders from several devices at once. | [examples/multicam](https://github.com/realsenseai/librealsense/tree/development/examples/multicam) |
| pointcloud | `rs-pointcloud` (graphical) | Generates and displays a textured 3D point cloud. | [examples/pointcloud](https://github.com/realsenseai/librealsense/tree/development/examples/pointcloud) |
| labeledpointcloud | `rs-labeled-pointcloud` (graphical) | Streams the device's labeled point cloud and displays each point colored by its label. | [examples/labeledpointcloud](https://github.com/realsenseai/librealsense/tree/development/examples/labeledpointcloud) |
| align | `rs-align` (graphical) | Introduces spatial alignment of depth and color streams. | [examples/align](https://github.com/realsenseai/librealsense/tree/development/examples/align) |
| align-gl | `rs-align-gl` (graphical, not macOS) | Spatial stream alignment accelerated on the GPU. | [examples/align-gl](https://github.com/realsenseai/librealsense/tree/development/examples/align-gl) |
| align-advanced | `rs-align-advanced` (graphical) | Uses `rs2::align` to remove the background behind a near object. | [examples/align-advanced](https://github.com/realsenseai/librealsense/tree/development/examples/align-advanced) |
| sensor-control | `rs-sensor-control` (graphical) | Tutorial for the `rs2::context`, `rs2::device` and `rs2::sensor` APIs. | [examples/sensor-control](https://github.com/realsenseai/librealsense/tree/development/examples/sensor-control) |
| measure | `rs-measure` (graphical) | Measures real-world distances between points with depth data. | [examples/measure](https://github.com/realsenseai/librealsense/tree/development/examples/measure) |
| C/depth | `rs-depth` | C API: streams depth and prints a text-based picture of it. | [examples/C/depth](https://github.com/realsenseai/librealsense/tree/development/examples/C/depth) |
| C/color | `rs-color` | C API: streams color and prints frame information. | [examples/C/color](https://github.com/realsenseai/librealsense/tree/development/examples/C/color) |
| C/distance | `rs-distance` | C API: prints the distance to the object in the center of the depth image. | [examples/C/distance](https://github.com/realsenseai/librealsense/tree/development/examples/C/distance) |
| C/infrared | `rs-infrared` | C API: streams left and right infrared and prints a text-based picture of them. | [examples/C/infrared](https://github.com/realsenseai/librealsense/tree/development/examples/C/infrared) |
| C/composite-ctl | `rs-composite-ctl` | C API: get, set and read the range of composite options on connected devices. | [examples/C/composite-ctl](https://github.com/realsenseai/librealsense/tree/development/examples/C/composite-ctl) |
| post-processing | `rs-post-processing` (graphical) | Applies the post-processing filters (decimation, spatial, temporal and others) to depth. | [examples/post-processing](https://github.com/realsenseai/librealsense/tree/development/examples/post-processing) |
| record-playback | `rs-record-playback` (graphical) | Records frames to a file (`a.db3`) with pause/resume and plays them back. | [examples/record-playback](https://github.com/realsenseai/librealsense/tree/development/examples/record-playback) |
| motion | `rs-motion` (graphical) | Computes camera rotation from gyroscope and accelerometer data. | [examples/motion](https://github.com/realsenseai/librealsense/tree/development/examples/motion) |
| gl | `rs-gl` (graphical, not macOS) | Processes depth data on the GPU. | [examples/gl](https://github.com/realsenseai/librealsense/tree/development/examples/gl) |
| hdr | `rs-hdr` (graphical) | Configures and visualizes High Dynamic Range (HDR) on the depth sensor of a D400 product line device. | [examples/hdr](https://github.com/realsenseai/librealsense/tree/development/examples/hdr) |
| on-chip-calib | `rs-on-chip-calib` | Runs the on-chip calibration flow on a device. | [examples/on-chip-calib](https://github.com/realsenseai/librealsense/tree/development/examples/on-chip-calib) |
| eth-config | `rs-eth-config` | Reads and changes Ethernet settings with `rs2::eth_config_device`. | [examples/eth-config](https://github.com/realsenseai/librealsense/tree/development/examples/eth-config) |
| embedded-filters | `rs-embedded-filters` | Reads and changes the options of a device's embedded filters with `rs2::embedded_filter`. | [examples/embedded-filters](https://github.com/realsenseai/librealsense/tree/development/examples/embedded-filters) |
| object-detection | `rs-object-detection` | Enables color and object detection streams and prints each detection. | [examples/object-detection](https://github.com/realsenseai/librealsense/tree/development/examples/object-detection) |
| composite-option-mock | `rs-composite-option-mock` | Exercises the composite-option API against an in-memory fake (no hardware needed). | [examples/composite-option-mock](https://github.com/realsenseai/librealsense/tree/development/examples/composite-option-mock) |
| composite-option | `rs-composite-option`, `rs-hdrd-control` | Get, set and read the range and metadata of composite options on connected devices; `rs-hdrd-control` prints the current value of each. | [examples/composite-option](https://github.com/realsenseai/librealsense/tree/development/examples/composite-option) |
<!-- /wikigen:table-examples -->

## Folders not built by the main CMake build

- [`examples/cmake`](https://github.com/realsenseai/librealsense/tree/development/examples/cmake) is a stand-alone CMake project (`hello_librealsense2`) that finds an installed SDK with `find_package(realsense2 REQUIRED)`. Use it as a template for building your own application outside the librealsense tree.
- [`examples/enhanced-depth-range`](https://github.com/realsenseai/librealsense/tree/development/examples/enhanced-depth-range) holds C++ and Python samples for the separate Improved Close Range Depth library; they are not part of the SDK build. See its readme.

Python samples live in [`wrappers/python/examples`](https://github.com/realsenseai/librealsense/tree/development/wrappers/python/examples); see [[Python Wrapper]].

## Shared helper headers

The graphical C++ samples include [`examples/example.hpp`](https://github.com/realsenseai/librealsense/blob/development/examples/example.hpp) (window and texture helpers on top of GLFW/OpenGL); the align, align-advanced, align-gl and hdr samples include it through [`examples/example-imgui.hpp`](https://github.com/realsenseai/librealsense/blob/development/examples/example-imgui.hpp). The C samples share [`examples/C/example.h`](https://github.com/realsenseai/librealsense/blob/development/examples/C/example.h).

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`46cf1a6`](https://github.com/realsenseai/librealsense/commit/46cf1a67cc441ad9f55ce339a0de30f2a1c6fed0)._
<!-- /wikigen:footer -->
