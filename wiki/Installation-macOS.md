This page explains how to build the RealSense SDK 2.0 (librealsense) from source on macOS with CMake, which USB backend the library uses there, and how to run the resulting tools and examples. It is for developers who want to use a RealSense camera on a Mac, either through the RealSense Viewer or from their own application.

## Overview

On macOS the SDK always uses the RS USB backend, which talks to the camera through libusb. There is no native camera-framework backend. The Unix configuration ([`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/unix_config.cmake)) forces it:

```cmake
if(APPLE)
    set(FORCE_RSUSB_BACKEND ON)
endif()
```

`FORCE_RSUSB_BACKEND` then selects the `RS2_USE_LIBUVC_BACKEND` compile definition. This builds the shared RS USB, UVC and HID sources ([`src/rsusb-backend/`](https://github.com/realsenseai/librealsense/tree/development/src/rsusb-backend), [`src/uvc/`](https://github.com/realsenseai/librealsense/tree/development/src/uvc), [`src/hid/`](https://github.com/realsenseai/librealsense/tree/development/src/hid), [`src/libuvc/`](https://github.com/realsenseai/librealsense/tree/development/src/libuvc)) together with the libusb layer in [`src/libusb/`](https://github.com/realsenseai/librealsense/tree/development/src/libusb) ([`src/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/src/CMakeLists.txt)). You do not need to pass `-DFORCE_RSUSB_BACKEND=ON` yourself on macOS. Passing it does no harm.

The repository guide ([`doc/installation_osx.md`](https://github.com/realsenseai/librealsense/blob/development/doc/installation_osx.md)) says macOS support for the full range of SDK functionality is not yet complete. For macOS it describes building from source only.

## Build from source

### Requirements

- **Xcode or the Xcode Command Line Tools**, for the compiler and the macOS frameworks. When the bundled libusb is built, the build needs the `CoreFoundation`, `IOKit` and `Security` frameworks. Without them, configuration stops with `CoreFoundation, IOKit, and Security frameworks are required on macOS` ([`CMake/external_libusb.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/external_libusb.cmake)).
- **CMake 3.10 or newer.** The top-level [`CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/CMakeLists.txt) calls `cmake_minimum_required(VERSION 3.10)`. `BUILD_WITH_DDS` needs CMake 3.16.3 ([`CMake/external_fastdds.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/external_fastdds.cmake)).
- **libusb** (optional). [`CMake/libusb_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/libusb_config.cmake) looks for an installed `usb-1.0` library and `libusb.h`. If it finds them, it links that libusb. Otherwise it prints `libusb not found; using internal version` and builds libusb statically from `libusb-cmake` (tag `v1.0.27-1`).
- **OpenSSL.** It is needed with the default options. `ENABLE_STATS` is `ON` on every platform, and `CHECK_FOR_UPDATES`, `ENABLE_AI_ASSISTANT` or `ENABLE_STATS` each pull in a bundled libcurl. On macOS that libcurl is built against OpenSSL (`find_package(OpenSSL REQUIRED)` in [`CMake/external_libcurl.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/external_libcurl.cmake)). To build without OpenSSL, configure with `-DENABLE_STATS=OFF` and leave `CHECK_FOR_UPDATES` and `ENABLE_AI_ASSISTANT` at their macOS default (`OFF`).
- **GLFW 3.3 or newer** (optional). This is for the graphical examples, the RealSense Viewer and the Depth Quality Tool. If `find_package(glfw3 3.3)` fails, the build prints `GLFW 3.3 not found; using internal version` and uses [`third-party/glfw`](https://github.com/realsenseai/librealsense/tree/development/third-party/glfw).
- **Network access during configuration and build.** Several dependencies are downloaded by CMake. nlohmann/json (unless `USE_EXTERNAL_NLOHMANN_JSON=ON`), and FastCDR and libdeflate (`BUILD_ROSBAG2` is `ON` by default), are cloned at configure time. libcurl, yaml-cpp and SQLite (both for `BUILD_ROSBAG2`), and libusb when no system libusb is found, are downloaded during the build.

The repository guide installs the tools with Homebrew:

```bash
sudo xcode-select --install
brew install cmake libusb pkg-config openssl
```

The project's macOS CI job (`Mac_DDS_cpp` in [`.github/workflows/buildsCI.yaml`](https://github.com/realsenseai/librealsense/blob/development/.github/workflows/buildsCI.yaml), on a `macos-15` runner) installs `coreutils`, `glfw` and `libusb` (if missing) with Homebrew before building.

### Steps (Makefiles)

1. Clone the repository:

   ```bash
   git clone https://github.com/realsenseai/librealsense.git
   cd librealsense
   ```

2. Create a build directory and configure. If CMake reports `Could NOT find OpenSSL`, the repository guide ([`doc/installation_osx.md`](https://github.com/realsenseai/librealsense/blob/development/doc/installation_osx.md)) points it at Homebrew's OpenSSL:

   ```bash
   mkdir build && cd build
   export OPENSSL_ROOT_DIR=`brew --prefix openssl`
   cmake .. -DCMAKE_BUILD_TYPE=Release
   ```

   If `CMAKE_BUILD_TYPE` is left empty, `CMake/unix_config.cmake` sets it to `Release`. Add `-D<OPTION>=<VALUE>` arguments to change build options (see [What gets built by default](#what-gets-built-by-default)).

3. Build:

   ```bash
   make -j4
   ```

   The CI job uses `cmake --build . -- -j4`, which does the same with the Makefile generator.

4. Optionally, install headers, libraries and the CMake and pkg-config package files ([`CMake/install_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/install_config.cmake)), plus the built tools and examples (each one's own `CMakeLists.txt`), into `CMAKE_INSTALL_PREFIX`:

   ```bash
   sudo make install
   ```

   The installed `realsense-viewer` gets the run-time search path `@loader_path;@loader_path/../lib`, so it finds `librealsense2.dylib` in its own folder or in the `lib` folder next to its `bin` folder ([`tools/realsense-viewer/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/tools/realsense-viewer/CMakeLists.txt)).

For reference, the CI job configures with:

```bash
cmake .. -DCMAKE_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=Debug -DBUILD_EXAMPLES=true -DBUILD_WITH_OPENMP=false -DHWM_OVER_XU=false -DCHECK_FOR_UPDATES=true -DBUILD_WITH_DDS=true
```

### Using Xcode

CMake can also generate an Xcode project (`cmake .. -G Xcode`), which you then build in Xcode or with `cmake --build . --config Release`. The CI does not cover this generator, and the Unix configuration is written with single-configuration generators in mind:

- `CMake/unix_config.cmake` derives the output folders from `CMAKE_BUILD_TYPE`. With an empty build type it sets `Release`, so outputs go under `build/Release`.
- The bundled libcurl and libusb are built with the configure-time `CMAKE_BUILD_TYPE`. The bundled libcurl is linked as `libcurl-d` in the Debug configuration and as `libcurl` otherwise, but only the variant matching `CMAKE_BUILD_TYPE` is built.

Build the `Release` configuration unless you configured with `-DCMAKE_BUILD_TYPE=Debug`. If in doubt, use the Makefile steps above.

### Where outputs land

`CMake/unix_config.cmake` puts the library, archives and all executables in `build/<CMAKE_BUILD_TYPE>/`, for example `build/Release/librealsense2.dylib`, `build/Release/realsense-viewer` and `build/Release/rs-enumerate-devices`. Examples and tools end up in the same folder as the library, so they run from there without installing.

### What gets built by default

These defaults come from [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake):

| Option | Default on macOS | Effect |
|---|---|---|
| `BUILD_SHARED_LIBS` | `ON` | Build `librealsense2.dylib` |
| `BUILD_EXAMPLES` | `ON` | Examples. It must be `ON` for the graphical examples, the RealSense Viewer and the Depth Quality Tool too |
| `BUILD_GRAPHICAL_EXAMPLES` | `ON` | Graphical examples, the RealSense Viewer and the Depth Quality Tool (only with `BUILD_EXAMPLES=ON`) |
| `BUILD_TOOLS` | `ON` | Tools such as the firmware updater |
| `FORCE_RSUSB_BACKEND` | `OFF` (forced `ON` on macOS) | RS USB (libusb) backend |
| `CHECK_FOR_UPDATES` | `OFF` | Viewer version check (needs libcurl and OpenSSL). `ON` on other platforms |
| `ENABLE_AI_ASSISTANT` | `OFF` | AI Assistant chat panel in the Viewer (needs libcurl and OpenSSL). `ON` on other platforms |
| `ENABLE_STATS` | `ON` | Usage-statistics collection (see [[Usage Statistics RUM]]). Pulls in libcurl and OpenSSL |
| `BUILD_ROSBAG2` | `ON` | ROS 2 bag recording support, builds FastCDR |
| `BUILD_WITH_DDS` | `OFF` | Ethernet/DDS camera support (see [[Ethernet and DDS Cameras]]) |

A comment in `CMake/lrs_options.cmake` explains the `OFF` defaults: macOS normally does not come with OpenSSL. Language bindings are off by default (`BUILD_PYTHON_BINDINGS`, `BUILD_CSHARP_BINDINGS` and others).

Two OpenGL examples are skipped on macOS even with `BUILD_GRAPHICAL_EXAMPLES=ON`: `rs-gl` ([`examples/gl/`](https://github.com/realsenseai/librealsense/tree/development/examples/gl)) and `rs-align-gl` ([`examples/align-gl/`](https://github.com/realsenseai/librealsense/tree/development/examples/align-gl)). Both CMake files test `NOT APPLE`.

For the full list of options, see [[Build Configuration]].

## Running tools and examples

### USB access and sudo

The repository guide says that on macOS 12 (Monterey) and newer, most librealsense tools that use libusb must be run with elevated privileges. It gives macOS USB security changes and the need to override the default UVC driver as the reason. From the build output folder:

```bash
cd build/Release
sudo ./rs-enumerate-devices
sudo ./rs-hello-realsense
sudo ./rs-depth
sudo ./rs-multicam
sudo ./realsense-viewer
```

The code does what the guide describes. When the library first opens a camera on macOS, it disables libusb's automatic kernel-driver detach and explicitly detaches the macOS driver from the interface being opened (`libusb_detach_kernel_driver`). If a driver was detached, it waits 500 ms for the device to re-enumerate. Later opens of the same physical device share this capture. When the last user releases the device, the library re-attaches the driver ([`src/libusb/darwin-device-capture.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/libusb/darwin-device-capture.cpp)). If the detach fails for any reason other than no driver being attached, the library throws `failed to capture USB device on Darwin`.

The same applies to your own applications built against the SDK.

### macOS differences in the RealSense Viewer

- The **Use GLSL for Rendering** and **Use GLSL for Processing** checkboxes are not shown in the Viewer settings on macOS ([`common/viewer.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/viewer.cpp)).
- On macOS, GLSL rendering and processing are off by default. If the OpenGL context does not provide OpenGL 3.0, the Viewer also forces them off. It initializes the ImGui OpenGL 3 backend with `#version 120` on macOS. It also defaults to font oversampling 2 and MSAA with 4 samples there ([`common/ux-window.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/ux-window.cpp)).

For the Viewer itself, see [[Realsense Viewer Manual]]. For the command-line tools, see [[Command Line Tools]], and for the examples, see [[Code Samples]].

## Packaging your application

When you ship an application, bundle `librealsense2.dylib` with it. Whether libusb must be bundled too depends on how the library was linked:

- If CMake found a system libusb (for example from Homebrew), `librealsense2.dylib` links to that `libusb-1.0` dynamic library. Check with `otool -L librealsense2.dylib`. The repository guide bundles it like this: copy `libusb-1.0.0.dylib` and `librealsense2.dylib` into your application's `Frameworks` folder, then rewrite the libusb reference to an `@rpath` one with `install_name_tool`, using the path that `otool -L` shows:

  ```bash
  install_name_tool -change <path shown by otool>/libusb-1.0.0.dylib @rpath/libusb-1.0.0.dylib librealsense2.dylib
  ```

- If no system libusb was found, or you configure with `-DUSE_EXTERNAL_USB=ON`, the bundled libusb is built as a static library and linked into librealsense together with the `CoreFoundation`, `IOKit` and `Security` frameworks and `objc`. You then have no separate libusb to ship.

## Troubleshooting

- **`Could NOT find OpenSSL` during configuration.** The bundled libcurl needs OpenSSL, and with the defaults `ENABLE_STATS=ON` already requires it. Point CMake at Homebrew's OpenSSL with ``export OPENSSL_ROOT_DIR=`brew --prefix openssl` `` before running `cmake`, as the repository guide and the CI job do. Or configure with `-DENABLE_STATS=OFF` and keep `CHECK_FOR_UPDATES` and `ENABLE_AI_ASSISTANT` off.
- **`ld: library not found for -lusb-1.0`.** The repository guide reports this on some Macs and works around it with `/bin/launchctl setenv LIBRARY_PATH /usr/local/lib`. Another option is `-DUSE_EXTERNAL_USB=ON`, which makes the build use its own static libusb instead of the system one.
- **No camera found, or `failed to capture USB device on Darwin`.** Run the tool with `sudo` (see [USB access and sudo](#usb-access-and-sudo)).
- **`CoreFoundation, IOKit, and Security frameworks are required on macOS`.** The bundled libusb could not find the macOS frameworks. Install Xcode or the Command Line Tools, then run `sudo xcode-select --reset`, as the repository guide does.

For more problems and answers, see [[Troubleshooting Q&A]]. For the RS USB backend on other platforms, see [[Installation Linux]] and [[Installation Windows]].

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`de7fa8c`](https://github.com/realsenseai/librealsense/commit/de7fa8cf3e05bac6f3ce6b5eeec2eb766c57259e)._
<!-- /wikigen:footer -->
