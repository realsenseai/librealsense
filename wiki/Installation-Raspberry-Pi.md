This page is a short orientation for building the RealSense SDK 2.0 (librealsense) from source on a Raspberry Pi. It is for developers who want to use a RealSense camera on a Raspberry Pi board running Raspbian/Raspberry Pi OS or Ubuntu.

## When you need it

- You want to build librealsense, its tools or the Python bindings on a Raspberry Pi.
- You need the udev rules so that the camera can be opened without root.
- The build runs out of memory on a board with little RAM.

## Key facts

- The Raspberry Pi build is the Linux build: [`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/unix_config.cmake) detects the compiler target. On 32-bit ARM it prints `Building for 32-bit ARM - using compiler default FPU settings`; on ARM64/AArch64 it prints `Building for ARM64 with NEON optimizations` or `Building for ARM64 without NEON optimizations`.
- `BUILD_WITH_NEON` exists only when `CMAKE_SYSTEM_PROCESSOR` is arm64/aarch64 and defaults to `ON` there ([`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/lrs_options.cmake)).
- The Raspbian guide builds with `-DFORCE_LIBUVC=true` (according to doc/installation_raspbian.md). That option is deprecated: the top-level [`CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/master/CMakeLists.txt) turns it into `FORCE_RSUSB_BACKEND=ON`, which selects the RS USB (libusb) backend instead of V4L2.
- The udev rules for the RS USB backend are in [`config/99-realsense-libusb.rules`](https://github.com/realsenseai/librealsense/blob/master/config/99-realsense-libusb.rules).
- When the build type is not set, it becomes `Release` (`CMake/unix_config.cmake`). Python bindings are built only with `BUILD_PYTHON_BINDINGS=ON` (default `OFF`).
- On a Raspberry Pi 3 (USB2 only) camera capabilities are severely reduced for lack of bandwidth, and boards with little RAM may need a larger swap file to compile the SDK (according to doc/RaspberryPi3.md and doc/installation_raspbian.md).

## Full guide

- Raspbian build steps: https://github.com/realsenseai/librealsense/blob/master/doc/installation_raspbian.md
- D400 cameras on Raspberry Pi 3 with Ubuntu MATE: https://github.com/realsenseai/librealsense/blob/master/doc/RaspberryPi3.md

## Related pages

- [[Installation Linux]]
- [[Build Configuration]]
- [[Python Wrapper]]
- [[Installation Jetson]]
- [[Troubleshooting Q&A]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
