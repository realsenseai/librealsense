This page is a short orientation for building the RealSense SDK 2.0 (librealsense) from source on macOS. It is for developers who want to use a RealSense camera on a Mac from the SDK tools, examples or their own application.

## When you need it

- You want to build librealsense, its examples or tools on macOS.
- You want to know which USB backend the library uses on a Mac and which dependencies the build needs.
- You package an application that links librealsense on macOS.

## Key facts

- On macOS the build always uses the RS USB backend over libusb: [`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/unix_config.cmake) sets `FORCE_RSUSB_BACKEND ON` when `APPLE`, which selects `RS2_USE_LIBUVC_BACKEND`. Passing `-DFORCE_RSUSB_BACKEND=ON` yourself is not needed.
- The top-level [`CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/master/CMakeLists.txt) requires CMake 3.10 or newer.
- If no installed libusb is found, [`CMake/libusb_config.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/libusb_config.cmake) reports `libusb not found; using internal version` and the build uses its own copy.
- `CHECK_FOR_UPDATES` and `ENABLE_AI_ASSISTANT` default to `OFF` on macOS and `ON` elsewhere ([`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/lrs_options.cmake)). `ENABLE_STATS` is `ON` on every platform; any of the three pulls in libcurl, which [`CMake/external_libcurl.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/external_libcurl.cmake) links against OpenSSL (`find_package(OpenSSL REQUIRED)`).
- Motion sensors (IMU) are compiled out on macOS: the D400 and D500 motion code is guarded with `#if !defined(__APPLE__) // Motion sensors not supported on macOS` (for example [`src/ds/d400/d400-motion.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d400/d400-motion.cpp)).
- When the build type is not set, it becomes `Release` (`CMake/unix_config.cmake`).
- The repository guide says macOS support for the full range of SDK functionality is not yet complete, that the RealSense Viewer is not supported on macOS, and that on macOS 12 and newer most tools that use libusb must be run with `sudo`.

## Full guide

- Build steps, known errors and packaging: https://github.com/realsenseai/librealsense/blob/master/doc/installation_osx.md

## Related pages

- [[Build Configuration]]
- [[Installation Linux]]
- [[Command Line Tools]]
- [[Code Samples]]
- [[Troubleshooting Q&A]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
