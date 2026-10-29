This page explains how to get the RealSense SDK 2.0 (librealsense) running on Windows: installing the prebuilt release, building the library, tools and examples from source with CMake and Visual Studio, and enabling per-frame metadata. It is for anyone who uses a RealSense camera on a Windows PC, either to run the RealSense Viewer or to develop against the SDK.

## Overview

On Windows, librealsense talks to the camera through one of two backends, chosen at CMake time in [`CMake/windows_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/windows_config.cmake):

| Backend | Selected when | Sources |
|---|---|---|
| Windows Media Foundation (`RS2_USE_WMF_BACKEND`) | Default | [`src/mf/`](https://github.com/realsenseai/librealsense/tree/development/src/mf) |
| RS USB backend over WinUSB (`RS2_USE_WINUSB_UVC_BACKEND`) | `FORCE_RSUSB_BACKEND=ON`, or the target system is Windows 7 (`CMAKE_SYSTEM_VERSION` 6.1) | [`src/win7/`](https://github.com/realsenseai/librealsense/tree/development/src/win7), [`src/rsusb-backend/`](https://github.com/realsenseai/librealsense/tree/development/src/rsusb-backend) |

The Media Foundation backend uses the camera through the Windows inbox drivers. Most users need nothing beyond the default.

The repository guide ([`doc/installation_windows.md`](https://github.com/realsenseai/librealsense/blob/development/doc/installation_windows.md)) covers Windows 10 and Windows 11. It also says the librealsense team does not recommend or support installing in a virtual machine, because of the USB 3.0 translation layer between the hardware and the VM.

## Install the prebuilt SDK

Windows installers and the standalone RealSense Viewer are published as assets on the [GitHub releases page](https://github.com/realsenseai/librealsense/releases). Download the asset you need from the release you want and run it.

After you connect the camera to a USB 3 port, it shows up in the Windows Device Manager. If you only want to look at the camera, see [[Realsense Viewer Manual]].

## Build from source with CMake and Visual Studio

### Requirements

- **CMake 3.10 or newer.** The top-level [`CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/CMakeLists.txt) and `CMake/windows_config.cmake` both call `cmake_minimum_required(VERSION 3.10)`. Some optional features need a newer CMake. For example, `BUILD_WITH_DDS` requires CMake 3.16.3 (see [[Build Configuration]]).
- **Visual Studio with C++14 support.** The library is compiled with `cxx_std_14` ([`CMake/lrs_macros.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_macros.cmake)). The repository guide asks for Visual Studio 2019 or newer.
- **A Windows 10 SDK.** You need one for frame metadata (see [Compiling with metadata support](#compiling-with-metadata-support)).
- **Git**, to clone the repository.
- **Network access during configuration and build.** nlohmann/json is cloned at configure time (`CMake/external_json.cmake`, unless `USE_EXTERNAL_NLOHMANN_JSON=ON`). Other components are cloned during the build. For example, libcurl (`CMake/external_libcurl.cmake`) is pulled in because `CHECK_FOR_UPDATES`, `ENABLE_AI_ASSISTANT` and `ENABLE_STATS` are `ON` by default on Windows.

### Steps

1. Clone the repository:

   ```
   git clone https://github.com/realsenseai/librealsense.git
   cd librealsense
   ```

2. Create a build directory and configure. Use a separate build directory: an out-of-source build is also what copies the Windows 7 driver files when you use the WinUSB backend (see below).

   ```
   mkdir build
   cd build
   cmake .. -A x64
   ```

   Add `-D<OPTION>=<VALUE>` arguments to change build options. You can also use `cmake-gui` and pick the source and build folders there.

3. Build. Either open the generated `realsense2.sln` in Visual Studio and build the `ALL_BUILD` target, or build from the command line:

   ```
   cmake --build . --config Release -- -m
   ```

   This is the command the project's Windows CI uses ([`.github/actions/windows-build/action.yml`](https://github.com/realsenseai/librealsense/blob/development/.github/actions/windows-build/action.yml)). `-- -m` tells MSBuild to build projects in parallel. The library itself is also compiled with `/MP`.

4. Optionally, install headers, libraries, the CMake and pkg-config package files, and the built tools and examples by building the `INSTALL` target, or (with CMake 3.15 or newer) with `cmake --install . --config Release`. The destination comes from `CMAKE_INSTALL_PREFIX` (see [`CMake/install_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/install_config.cmake)).

![Windows CMake](https://raw.githubusercontent.com/realsenseai/librealsense/development/doc/img/windows_cmake.png)

### Build type and where outputs land

- Visual Studio generators are multi-configuration. The configuration (`Release`, `Debug`, `RelWithDebInfo`, ...) is chosen at build time with `--config`, or in the Visual Studio toolbar. `CMake/windows_config.cmake` sets no default build type. (On Linux, an empty `CMAKE_BUILD_TYPE` becomes `Release`; Windows has no equivalent.)
- `CMake/windows_config.cmake` sets the library, runtime and archive output directories to the build directory. Visual Studio then places the binaries in a per-configuration subfolder, for example `build\Release\realsense2.dll` and `build\Release\realsense-viewer.exe`. The library, examples and tools all end up together, so examples run straight from that folder.
- With MSVC, Debug libraries get a `d` suffix (`CMAKE_DEBUG_POSTFIX "d"`, so `realsense2d.dll`). Executables keep their names. This lets Release and Debug libraries be installed side by side.

### What gets built by default

These defaults come from [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake):

| Option | Default | Effect |
|---|---|---|
| `BUILD_SHARED_LIBS` | `ON` | Build `realsense2` as a DLL |
| `BUILD_EXAMPLES` | `ON` | Examples. It must be `ON` for the graphical examples, the RealSense Viewer and the Depth Quality Tool too |
| `BUILD_GRAPHICAL_EXAMPLES` | `ON` | Graphical examples, the RealSense Viewer and the Depth Quality Tool (only with `BUILD_EXAMPLES=ON`) |
| `BUILD_TOOLS` | `ON` | Tools such as the firmware updater |
| `ENFORCE_METADATA` | `OFF` | Fail the build if the Windows SDK cannot provide metadata |
| `FORCE_RSUSB_BACKEND` | `OFF` | Use the RS USB (WinUSB) backend instead of Media Foundation |
| `COM_MULTITHREADED` | `ON` | Set `OFF` to initialize COM with `COINIT_APARTMENTTHREADED` (Windows only) |

Examples and tools are built by default. You do not need to switch on `BUILD_EXAMPLES` to get them. Language bindings are off by default: `BUILD_PYTHON_BINDINGS`, `BUILD_CSHARP_BINDINGS`, `BUILD_MATLAB_BINDINGS`, `BUILD_UNITY_BINDINGS` and `BUILD_OPENNI2_BINDINGS`. On Windows, `DOTNET_VERSION_LIBRARY` defaults to `3.5` and `DOTNET_VERSION_EXAMPLES` to `4.0`.

`ENABLE_SECURITY_FLAGS=ON` adds `/Gy /DYNAMICBASE /GS` with MSVC. Unless `CMAKE_BUILD_TYPE` is `Debug` at configure time, it also adds `/WX /sdl`. With Visual Studio generators `CMAKE_BUILD_TYPE` is empty, so that means every configuration, Debug included. It also appends `/INCREMENTAL:NO /LTCG /NXCOMPAT` to `CMAKE_LINKER_FLAGS`.

For ARM builds (for example `cmake .. -A ARM64`), the SSE optimizations (`__SSSE3__`) are left out, because MSVC's SSE headers are x86/x64-only *(new in the next release)*. The project's CI builds the `realsense2` library target for ARM64 Windows.

For the full list of options, see [[Build Configuration]].

## Enabling metadata on Windows

Per-frame metadata (timestamps, exposure, gain and similar attributes, see [[Frame Metadata]]) is delivered through Media Foundation. Windows provides it only when each camera interface has a `MetadataBufferSizeInKB<n>` value in its registry entry. These values are device-specific: you need to add them again for every new camera, or when a camera appears as a new device instance.

There are two ways to add the registry values: let the RealSense Viewer do it, or run the repository's PowerShell script.

### From the RealSense Viewer

The Windows build of the RealSense Viewer checks the registry for every connected camera whose product line is `D400` or `D500` ([`common/metadata-helper.h`](https://github.com/realsenseai/librealsense/blob/development/common/metadata-helper.h), [`common/subdevice-model.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/subdevice-model.cpp)). If the values are missing, the Viewer shows a **Frame Metadata Disabled** notification with an **Enable** button ([`common/notifications.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/notifications.cpp)).

When you press **Enable** and the Viewer is not running as administrator, it starts a second copy of itself, elevated, with the `--enable_metadata` argument ([`common/windows-app-bootstrap.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/windows-app-bootstrap.cpp)). That copy writes the values for all connected D400/D500 devices and exits. Windows shows a UAC prompt for the elevation. For each interface, the Viewer writes `MetadataBufferSizeInKB0` up to `MetadataBufferSizeInKB<n-1>`, each as a `DWORD` with value `5`, where `n` is the number of media pins the code assigns to that interface ([`common/metadata-helper.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/metadata-helper.cpp)).

### With the PowerShell script

The script is [`scripts/realsense_metadata_win10.ps1`](https://github.com/realsenseai/librealsense/blob/development/scripts/realsense_metadata_win10.ps1). Open PowerShell, go to the `scripts` folder and run:

```
.\realsense_metadata_win10.ps1 -op install
```

| `-op` value | What the script does |
|---|---|
| `install` (default when `-op` is omitted) | Adds the values for RealSense devices that are currently connected |
| `install_all` | Adds the values for all RealSense devices listed in the registry, connected or not |
| `remove` | Removes `MetadataBufferSizeInKB0`…`MetadataBufferSizeInKB3` for connected RealSense devices |
| `remove_all` | Removes them for all RealSense devices listed in the registry |

If you pass any other value, the script prints the allowed values and exits.

What the script does, step by step:

1. If it is not running as administrator, it starts itself again elevated (`Start-Process powershell -Verb runAs`) with the same `-op` value.
2. It finds connected UVC devices under `HKLM:\SYSTEM\CurrentControlSet\Services\usbvideo` and keeps those with `VID_8086` or `VID_38E5` in their device instance.
3. It processes two registry branches:
   - `HKLM:\SYSTEM\CurrentControlSet\Control\DeviceClasses\{e5323777-f976-4f5b-9b55-b94699c46e44}`
   - `HKLM:\SYSTEM\CurrentControlSet\Control\DeviceClasses\{65E8773D-8F56-11D0-A3B9-00A0C9223196}`

   In each branch, for each matching device, it works on the `#global\Device Parameters` subkey.
4. When installing, it sets `MetadataBufferSizeInKB0` = `5` on every matching interface. It also sets `MetadataBufferSizeInKB1`, `MetadataBufferSizeInKB2` and `MetadataBufferSizeInKB3` = `5` on:
   - the `MI_00` interface of every product ID in the script's multi-pin list (`$MultiPinDevices`)
   - every `VID_38E5` interface

   Values that already exist and are non-zero are left alone.
5. It prints `Task Completed`.

The script suppresses PowerShell errors (`$ErrorActionPreference = "silentlycontinue"`), so a failed registry write does not stop it. Check its output for the `adding metadata key` lines.

If PowerShell refuses to run the script because script execution is disabled, the repository guide suggests running `Set-ExecutionPolicy RemoteSigned` in an administrator PowerShell, then running the script again.

### Adding the registry values by hand

If neither the Viewer nor the script can be used, you can create the same values yourself with `regedit`:

1. In Device Manager, open each RealSense camera interface and note its **Device instance path**.
2. Under each of the two `DeviceClasses` branches listed above, open the subkey whose name contains that device instance, then `#GLOBAL` → `Device Parameters`.
3. Add a `DWORD (32-bit)` value `MetadataBufferSizeInKB0` with value `5`. For the `MI_00` (depth) interface of multi-pin devices, and for every interface of a `VID_38E5` camera, also add `MetadataBufferSizeInKB1`, `MetadataBufferSizeInKB2` and `MetadataBufferSizeInKB3` with value `5`, as the script does.
4. Repeat for every RealSense interface (depth, color, and so on).

![Registry value](https://raw.githubusercontent.com/realsenseai/librealsense/development/doc/img/win_md_reg_key.png)

### Compiling with metadata support

Metadata support in the Media Foundation backend depends on the Windows SDK used for the build ([`src/mf/mf-uvc.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/mf/mf-uvc.cpp)):

- With a Windows SDK newer than 8.1, metadata support is compiled in.
- With Windows SDK 8.1 or older and `ENFORCE_METADATA=OFF` (the default), the compiler prints this message and the library is built without metadata:

  ```
  Librealsense notification: Featuring UVC Metadata requires WinSDK 10.0.10586.0 toolset. The library will be compiled without the metadata support!
  ```

- With `ENFORCE_METADATA=ON`, the same situation stops the build with the error `Librealsense Error!: Featuring UVC Metadata requires WinSDK 10.0.10586.0. ...`.

To make sure your build includes metadata, configure with `-DENFORCE_METADATA=ON`. If several Windows SDKs are installed and the wrong one is picked, retarget the solution in Visual Studio, or set the SDK version CMake should use (the Visual Studio generators take it from `CMAKE_SYSTEM_VERSION`).

## Windows 7 and the WinUSB (RS USB) backend

The WinUSB-based backend is still in the code. CMake selects it in two cases:

- automatically, when the target system version is Windows 7 (`CMAKE_SYSTEM_VERSION` equal to `6.1`)
- on any Windows version, when you configure with `FORCE_RSUSB_BACKEND=ON`:

  ```
  cmake .. -A x64 -DFORCE_RSUSB_BACKEND=ON
  ```

The older option `FORCE_WINUSB_UVC` is deprecated. Setting it prints a CMake deprecation message and turns on `FORCE_RSUSB_BACKEND` (top-level `CMakeLists.txt`). Use `FORCE_RSUSB_BACKEND` instead.

With this backend:

- The library is built with `RS2_USE_WINUSB_UVC_BACKEND`, from [`src/win7/`](https://github.com/realsenseai/librealsense/tree/development/src/win7) plus the shared RS USB, UVC and HID sources. It links against libusb. A system libusb is used if CMake finds one; otherwise the bundled external libusb is built ([`CMake/libusb_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/libusb_config.cmake)).
- In an out-of-source build, the files in [`src/win7/drivers/`](https://github.com/realsenseai/librealsense/tree/development/src/win7/drivers) are copied to `<build>\drivers\`, through a `realsense-driver` target that the library depends on. The repository contains only the driver `.inf` file (`IntelRealSense_D400_series_win7.inf`). The catalog file it references is not in the repository.
- The Media Foundation metadata registry setup described above does not apply, because this backend does not use Media Foundation.

The repository guide ([`doc/installation_win7.md`](https://github.com/realsenseai/librealsense/blob/development/doc/installation_win7.md)) states that Windows 7 is no longer maintained and that not all product features are enabled there. Official validation covers Windows 10/11 and Linux.

## Build options

All CMake options, with their defaults and descriptions, are listed in [[Build Configuration]]. Pass them on the configure line, for example `cmake .. -A x64 -DBUILD_PYTHON_BINDINGS=ON`. Bindings have their own pages: [[Python Wrapper]] and [[CSharp Wrapper]].

## Troubleshooting

- **The camera is not detected by the SDK.** The repository guide suggests checking that Windows allows desktop apps to use the camera: **Settings → Privacy → Camera → Allow desktop apps to access your camera** must be on.
- **No metadata attributes on frames.** Check that the registry values exist (see [Enabling metadata on Windows](#enabling-metadata-on-windows)). Run the script or press **Enable** in the Viewer again after you connect a new camera. Also check that the library was built with a Windows 10 SDK; configure with `-DENFORCE_METADATA=ON` to be sure.
- **Errors caused by long file paths.** The project's Windows CI enables long paths (`LongPathsEnabled` = `1` under `HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem`) before building. Doing the same, or using a short build path, can help.

For more problems and answers, see [[Troubleshooting Q&A]].

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`0d4fcab`](https://github.com/realsenseai/librealsense/commit/0d4fcabf634650b821200ad38eedaa13aa4cb704)._
<!-- /wikigen:footer -->
