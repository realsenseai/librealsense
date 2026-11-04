This page is a short orientation for using the RealSense SDK 2.0 (librealsense) on Windows: building it from source with CMake and Visual Studio, and enabling per-frame metadata through the Windows registry. It is for developers who build the SDK themselves or who need frame metadata from a camera on a Windows host.

## When you need it

- You want to build librealsense, its examples or tools from source on Windows.
- You need frame metadata (timestamps, exposure and similar attributes) and must add the registry keys Windows requires for each device.
- You build for Windows 7 or want the RS USB backend instead of the Windows Media Foundation backend.
- The camera is not recognized by the SDK on a Windows host.

## Key facts

- The top-level [`CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/master/CMakeLists.txt) requires CMake 3.10 or newer (`cmake_minimum_required(VERSION 3.10)`).
- By default the Windows build uses the Media Foundation backend (`RS2_USE_WMF_BACKEND`). With `FORCE_RSUSB_BACKEND=ON`, or when CMake detects Windows 7 (system version 6.1), it uses `RS2_USE_WINUSB_UVC_BACKEND` instead ([`CMake/windows_config.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/windows_config.cmake)). `FORCE_WINUSB_UVC` is deprecated; the top-level `CMakeLists.txt` turns it into `FORCE_RSUSB_BACKEND`.
- `BUILD_EXAMPLES` and `BUILD_GRAPHICAL_EXAMPLES` are `ON` by default ([`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/lrs_options.cmake)). With MSVC, Debug libraries get a trailing `d` (`CMAKE_DEBUG_POSTFIX "d"`).
- Metadata needs WinSDK 10.0.10586.0 or later. With an older SDK, [`src/mf/mf-uvc.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/mf/mf-uvc.cpp) prints `Librealsense notification: Featuring UVC Metadata requires WinSDK 10.0.10586.0 toolset.` at compile time and the library is built without metadata support.
- `ENFORCE_METADATA` (default `OFF`, Windows only) turns that notification into a compile error, so a build without metadata support fails instead.
- The registry keys are added by [`scripts/realsense_metadata_win10.ps1`](https://github.com/realsenseai/librealsense/blob/master/scripts/realsense_metadata_win10.ps1), run from an administrator PowerShell. It accepts `-op install` (the default), `install_all`, `remove` and `remove_all`.
- The repository guide says installation in a virtual machine is not recommended or supported, and that Windows 7 is no longer maintained.

## Full guide

- Build and metadata setup: https://github.com/realsenseai/librealsense/blob/master/doc/installation_windows.md
- Installing the prebuilt SDK and Viewer: https://github.com/realsenseai/librealsense/blob/master/doc/distribution_windows.md
- Windows 7: https://github.com/realsenseai/librealsense/blob/master/doc/installation_win7.md

## Related pages

- [[Build Configuration]]
- [[Frame Metadata]]
- [[Realsense Viewer Manual]]
- [[Installation Linux]]
- [[Troubleshooting Q&A]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
