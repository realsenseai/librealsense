This page explains how to install the RealSense SDK 2.0 (librealsense) on Linux: from the prebuilt Debian packages, or by building it from source with CMake. It is for anyone who wants to run the RealSense tools, such as `realsense-viewer` and `rs-enumerate-devices`, or develop applications against `librealsense2` on a Linux PC. For NVIDIA Jetson boards see [[Installation Jetson]], for Raspberry Pi see [[Installation Raspberry Pi]], and for containers see [[Docker]].

## Overview

On Linux, librealsense can talk to cameras through one of two backends. CMake picks the backend in [`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/unix_config.cmake):

| Backend | Selected when | How it reaches the camera |
|---|---|---|
| V4L2 (`RS2_USE_V4L2_BACKEND`) | default (`FORCE_RSUSB_BACKEND=OFF`) | Through the kernel's `uvcvideo` (Video4Linux2) and HID drivers ([`src/linux/`](https://github.com/realsenseai/librealsense/tree/development/src/linux)). |
| RSUSB (`RS2_USE_LIBUVC_BACKEND`) | `-DFORCE_RSUSB_BACKEND=ON` (or the deprecated `FORCE_LIBUVC`) | A user-space UVC and HID implementation over libusb ([`src/uvc/`](https://github.com/realsenseai/librealsense/tree/development/src/uvc), [`src/hid/`](https://github.com/realsenseai/librealsense/tree/development/src/hid), [`src/rsusb-backend/`](https://github.com/realsenseai/librealsense/tree/development/src/rsusb-backend)), so it does not depend on kernel driver patches. |

Both paths need udev rules so that a normal user can open the camera without `sudo`.

You can install librealsense on Linux in three ways:

1. **Prebuilt packages** from the RealSense apt repository (Ubuntu).
2. **Build from source** with the default V4L2 backend. Optionally patch the kernel modules with the scripts in [`scripts/`](https://github.com/realsenseai/librealsense/tree/development/scripts).
3. **Build from source with the RSUSB backend**, which needs no kernel patches.

The installation guide [`doc/installation.md`](https://github.com/realsenseai/librealsense/blob/development/doc/installation.md) lists Ubuntu 20/22/24/26 LTS as the supported versions. It also says the librealsense team does not support installation in a virtual machine, because of the USB 3.0 translation layer between the hardware and the VM.

## Install the prebuilt packages

The commands in this section come from the repository guide [`doc/distribution_linux.md`](https://github.com/realsenseai/librealsense/blob/development/doc/distribution_linux.md). The packages are built and published outside this repository, so the source code cannot confirm what the server provides. Check that guide for the current details.

The repository guide says to:

1. Register the server's public key:
   ```sh
   sudo mkdir -p /etc/apt/keyrings
   curl -sSf https://librealsense.realsenseai.com/Debian/librealsenseai.asc | \
   gpg --dearmor | sudo tee /etc/apt/keyrings/librealsenseai.gpg > /dev/null
   ```
2. Make sure apt HTTPS support is installed:
   ```sh
   sudo apt-get install apt-transport-https
   ```
3. Add the repository and refresh the package list:
   ```sh
   echo "deb [signed-by=/etc/apt/keyrings/librealsenseai.gpg] https://librealsense.realsenseai.com/Debian/apt-repo `lsb_release -cs` main" | \
   sudo tee /etc/apt/sources.list.d/librealsense.list
   sudo apt-get update
   ```
4. Install the kernel driver package and the tools:
   ```sh
   sudo apt-get install librealsense2-dkms
   sudo apt-get install librealsense2-utils
   ```
5. Optionally, install the developer and debug packages:
   ```sh
   sudo apt-get install librealsense2-dev
   sudo apt-get install librealsense2-dbg
   ```

Then reconnect the camera and run `realsense-viewer` to check the installation. The repository guide also covers upgrading and removing the packages, and lists what each package contains.

The code refers to the packages in two places:

- `realsense-viewer` treats `/lib/udev/rules.d/60-librealsense2-udev-rules.rules` as the rules file installed by the Debian packages. For missing or outdated rules, it suggests `sudo apt-get upgrade/install librealsense2-udev-rules` ([`common/viewer.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/viewer.cpp)).
- When a firmware update fails with "Permission Denied!", the library tells Debian package users to run `sudo apt-get install librealsense2-dkms` ([`src/fw-update/fw-update-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/fw-update/fw-update-device.cpp)).

Do not combine a package installation with a source installation on the same machine. See [Troubleshooting](#troubleshooting).

## Build from source

### 1. Prerequisites

Unplug any connected RealSense camera, then update the system:

```sh
sudo apt-get update && sudo apt-get upgrade
```

Install the build tools and libraries. The package names below are the Ubuntu packages listed in the installation guide and used by the repository scripts:

```sh
sudo apt-get install git wget cmake build-essential
sudo apt-get install libssl-dev libusb-1.0-0-dev libudev-dev pkg-config
sudo apt-get install libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev
sudo apt-get install v4l-utils
```

What the build does with each of these:

- **CMake**: the top-level [`CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/CMakeLists.txt) requires CMake 3.10 or newer.
- **git and network access**: the default configuration downloads third-party sources while it configures or builds:
  - Fast-CDR is fetched with `git clone` because `BUILD_ROSBAG2` is `ON` ([`CMake/external_fastcdr.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/external_fastcdr.cmake)).
  - nlohmann/json is downloaded unless you pass `USE_EXTERNAL_NLOHMANN_JSON=ON`.
  - libcurl is built from its git repository when `CHECK_FOR_UPDATES`, `ENABLE_AI_ASSISTANT` or `ENABLE_STATS` is on. All three default to `ON` on Linux ([`CMake/external_libcurl.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/external_libcurl.cmake)).
- **OpenSSL (`libssl-dev`)**: the libcurl build uses OpenSSL on Linux (`-DCURL_USE_OPENSSL=ON`).
- **libusb (`libusb-1.0-0-dev`)**: used if found. Otherwise CMake prints `libusb not found; using internal version` and builds libusb from source ([`CMake/libusb_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/libusb_config.cmake)).
- **libudev (`libudev-dev`, found through `pkg-config`)**: optional for the V4L2 backend. Without it, CMake prints `UDEV not found; using polling device-watcher!` and the library polls for device changes instead of using udev events ([`src/linux/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/src/linux/CMakeLists.txt)).
- **OpenGL and GLFW**: needed whenever the examples are built (`BUILD_EXAMPLES`, `ON` by default). The examples link OpenGL, and the `realsense2-gl` library (`BUILD_GLSL_EXTENSIONS`, `ON` by default) is built with them. OpenGL is a required package in [`CMake/opengl_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/opengl_config.cmake). If GLFW 3.3 is not found, CMake prints `GLFW 3.3 not found; using internal version` and builds the bundled copy in `third-party/glfw`, which needs the X11 development headers.
- **v4l-utils**: `scripts/setup_udev_rules.sh` stops with `v4l2-ctl not found, install with: sudo apt install v4l-utils` when `v4l2-ctl` is missing.

### 2. Get the source

```sh
git clone https://github.com/realsenseai/librealsense.git
cd librealsense
```

Run the scripts below from this repository root. They use relative paths such as `config/` and `./scripts/`.

### 3. Install the udev rules

```sh
./scripts/setup_udev_rules.sh
```

What [`scripts/setup_udev_rules.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/setup_udev_rules.sh) does:

- It runs with `bash -e`, so it stops at the first failing command. It exits right away if `v4l2-ctl` is not installed.
- If any `/dev/video*` node exists, it asks you to remove all RealSense cameras and waits until you press Enter.
- It copies [`config/99-realsense-libusb.rules`](https://github.com/realsenseai/librealsense/blob/development/config/99-realsense-libusb.rules) to `/etc/udev/rules.d/`. This file sets `MODE:="0666", GROUP:="plugdev"` for the RealSense USB product IDs.
- If `v4l2-ctl --list-devices` reports a `tegra` or `ipu6` device, it also installs:
  - [`config/99-realsense-d4xx-mipi-dfu.rules`](https://github.com/realsenseai/librealsense/blob/development/config/99-realsense-d4xx-mipi-dfu.rules) into `/etc/udev/rules.d/`
  - `scripts/rs-enum.sh` and `scripts/rs_ipu6_d457_bind.sh` into `/usr/local/bin/`

  These are for MIPI cameras.
- It reloads and triggers udev (`udevadm control --reload-rules && udevadm trigger`) and prints `udev-rules successfully installed`.

To remove the rules again, run:

```sh
./scripts/setup_udev_rules.sh --uninstall
```

### 4. Kernel patch (optional) or RSUSB backend

With the default V4L2 backend, librealsense uses the kernel's `uvcvideo` driver. The installation guide says most RealSense cameras work with the stock kernel. It says patching is only needed for newly released camera models that the distribution kernel does not recognize yet: without the patch, streaming works but per-frame metadata is not available.

If you need the patch, run the script that matches your Ubuntu version and kernel:

```sh
./scripts/patch-realsense-ubuntu-lts-hwe.sh
```

[`scripts/patch-realsense-ubuntu-lts-hwe.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/patch-realsense-ubuntu-lts-hwe.sh) runs with `set -e`. It does the following:

1. Installs the kernel headers and the build packages it needs.
2. Fetches the Ubuntu kernel source tag that matches your kernel from `git.launchpad.net` into an `ubuntu-<codename>` folder in the current directory.
3. Applies the RealSense patches from `scripts/`.
4. Builds the `uvcvideo`, `videodev` and related modules, and copies them into your home folder as `<kernel>-<module>.ko`.
5. Unloads the running modules and inserts the patched ones.

If a patched module fails to load, the script restores the original module and stops with `Failed to insert the patched module. Operation is aborted, the original module is restored` ([`scripts/patch-utils-hwe.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/patch-utils-hwe.sh)). On success it prints `Script has completed. Please consult the installation guide for further instruction.`

The script picks the patch set from the Ubuntu codename and the kernel's major.minor version. It exits with an `Unsupported kernel version` or `Unsupported distribution` error for anything else:

| Ubuntu | Codename | Kernels accepted by `patch-realsense-ubuntu-lts-hwe.sh` |
|---|---|---|
| 20.04 | `focal` | 5.4, 5.8, 5.11, 5.13, 5.15 |
| 22.04 | `jammy` | 5.15, 5.19, 6.2, 6.5, 6.8 |
| 24.04 | `noble` | 6.8, 6.11, 6.14 |

The script accepts these arguments:

| Argument | Effect |
|---|---|
| `-h`, `--help` | Print the flag list and exit. |
| `reset` | Rebuild and reinstall the stock Ubuntu modules without the RealSense patches. |
| `ko` | Rebuild the modules from the existing source tree without fetching or patching it again. |
| `uvc_dbg` | Also apply the `uvcvideo`/`videobuf2` debug patch. |
| `xhci-patch` | Apply the xHCI and videobuf2 backports. Requires kernel 4.4, which the supported-kernel check rejects, so the script always exits. |
| `build_usbcore_modules` | Also rebuild the usbcore, USB host and usbhid modules. Implies `xhci-patch`, so the script also always exits. |
| `ubuntu <codename>` | Use this codename instead of the running system's, and only build the modules (do not load them). |
| `kernel <version>` | Build for this kernel version instead of `uname -r`, and only build the modules. |

The older [`scripts/patch-realsense-ubuntu-lts.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/patch-realsense-ubuntu-lts.sh) uses [`scripts/patch-utils.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/patch-utils.sh). It accepts only Ubuntu `focal` with kernel 5.4, 5.8 or 5.11. Other scripts in `scripts/` target other kernels and distributions (`patch-arch.sh`, `patch-opensuse-leap15.sh`, `patch-opensusetumbleweed.sh`, `patch-realsense-ubuntu-L4T.sh`, `patch-uvcvideo-ubuntu-mainline.sh` and others); this page does not cover them.

If you cannot or do not want to patch the kernel, build with the RSUSB backend instead. See [RSUSB backend](#rsusb-libuvc-backend) below.

### 5. Configure, build and install

From the repository root:

```sh
mkdir build && cd build
cmake ..
make -j$(($(nproc)-1))
sudo make install
```

What the default configuration does:

- **Build type**: when `CMAKE_BUILD_TYPE` is not set, [`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/unix_config.cmake) sets it to `Release`. Libraries and executables go to `build/Release/`, or to `build/<build type>/` for the type you pass. For a debug build, use `cmake .. -DCMAKE_BUILD_TYPE=Debug`.
- **Backend**: V4L2. CMake prints `using RS2_USE_V4L2_BACKEND`, or `using RS2_USE_LIBUVC_BACKEND` with `FORCE_RSUSB_BACKEND=ON`.
- **What gets built** ([`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake)):
  - The shared library `librealsense2` (`BUILD_SHARED_LIBS=ON`).
  - The examples (`BUILD_EXAMPLES=ON`).
  - The graphical examples and tools such as `realsense-viewer` and the depth quality tool (`BUILD_GRAPHICAL_EXAMPLES=ON`, built only together with `BUILD_EXAMPLES`).
  - The command-line tools such as `rs-enumerate-devices` and `rs-fw-update` (`BUILD_TOOLS=ON`).
  - Unit tests (`BUILD_UNIT_TESTS`), Python bindings (`BUILD_PYTHON_BINDINGS`) and DDS support (`BUILD_WITH_DDS`) are `OFF`.

Common variations:

```sh
# No realsense-viewer or depth quality tool: build the library, non-graphical examples and tools only
cmake .. -DBUILD_GRAPHICAL_EXAMPLES=OFF

# Library and command-line tools only, no examples or graphical tools (no OpenGL/GLFW needed)
cmake .. -DBUILD_EXAMPLES=OFF

# Debug build
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

With `BUILD_GRAPHICAL_EXAMPLES=OFF`, CMake also turns off `CHECK_FOR_UPDATES` and `ENABLE_AI_ASSISTANT` ([`CMake/global_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/global_config.cmake)).

`sudo make install` installs under `CMAKE_INSTALL_PREFIX`, which is `/usr/local` unless you set it ([`CMake/install_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/install_config.cmake)):

| What | Where (relative to the prefix) |
|---|---|
| `librealsense2` library (and `realsense2-gl` when built) | `CMAKE_INSTALL_LIBDIR` (usually `lib`) |
| Headers | `include/librealsense2` (and `include/librealsense2-gl`) |
| Tools and examples | `CMAKE_INSTALL_BINDIR` (`bin`) |
| CMake package files (`find_package(realsense2)`) | `<libdir>/cmake/realsense2` |
| pkg-config file `realsense2.pc` | `<libdir>/pkgconfig` |

The install step finishes by running `ldconfig`. To remove the files listed in the build folder's `install_manifest.txt`, run `sudo make uninstall` from the same build folder.

After installing, connect the camera and run:

```sh
rs-enumerate-devices
realsense-viewer
```

`realsense-viewer` is installed only when the graphical tools are built. See [[Realsense Viewer Manual]] and [[Command Line Tools]].

## Build options

Every CMake option, with its default, is listed on [[Build Configuration]]. Pass options on the command line as `cmake .. -DNAME=ON`. Options that often matter on Linux:

| Option | Default | Use |
|---|---|---|
| `FORCE_RSUSB_BACKEND` | `OFF` | Use the RSUSB (libusb) backend instead of V4L2. |
| `BUILD_EXAMPLES` | `ON` | Build the examples. When it is `OFF`, the command-line tools are still built if `BUILD_TOOLS` is `ON`, but the graphical tools are not. |
| `BUILD_GRAPHICAL_EXAMPLES` | `ON` | Build the OpenGL tools and examples (`realsense-viewer`, depth quality tool and others). Takes effect only when `BUILD_EXAMPLES` is `ON`. |
| `BUILD_TOOLS` | `ON` | Build the command-line tools. |
| `BUILD_PYTHON_BINDINGS` | `OFF` | Build `pyrealsense2`. See [[Python Wrapper]]. |
| `BUILD_WITH_CUDA` | `OFF` | Enable CUDA. See [[GPU Acceleration]]. |
| `BUILD_WITH_DDS` | `OFF` | Access cameras through DDS. See [[Ethernet and DDS Cameras]]. |
| `CHECK_FOR_UPDATES` | `ON` | Checks for version updates. Needs libcurl (built with OpenSSL); turned off when `BUILD_GRAPHICAL_EXAMPLES` is `OFF`. |
| `ENABLE_STATS` | `ON` | RUM (Real User Monitoring) usage-statistics collection; also pulls in libcurl. See [[Usage Statistics RUM]]. |

## RSUSB (libuvc) backend

The RSUSB backend reads the camera through libusb from user space, so it works without kernel patches. The option text in [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake) calls it "optional for Linux". The libuvc installation guide [`doc/libuvc_installation.md`](https://github.com/realsenseai/librealsense/blob/development/doc/libuvc_installation.md) says this method is not officially validated, but it can run on a wider range of platforms and kernel versions.

To build it yourself, follow the steps in [Build from source](#build-from-source). Install the udev rules and skip the kernel patch. Then configure with:

```sh
cmake .. -DFORCE_RSUSB_BACKEND=ON
```

`FORCE_LIBUVC`, `FORCE_WINUSB_UVC` and `ANDROID_USB_HOST_UVC` are deprecated. Setting any of them prints a deprecation message and turns on `FORCE_RSUSB_BACKEND` ([`CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/CMakeLists.txt)).

The repository also has an all-in-one script, [`scripts/libuvc_installation.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/libuvc_installation.sh). It runs with `bash -xe` and does the following:

1. If any `/dev/video*` node exists, it asks you to remove the cameras and waits until you press Enter.
2. It runs `apt-get update` and creates a fresh `~/librealsense_build` folder.
3. If no swap is active, it creates and enables a 2 GB `/swapfile`.
4. It installs `git cmake libssl-dev freeglut3-dev libusb-1.0-0-dev pkg-config libgtk-3-dev unzip`.
5. It downloads and unpacks the **`master`** branch archive. It does not use your local checkout or the `development` branch.
6. It installs both `config/99-realsense-libusb.rules` and `config/99-realsense-d4xx-mipi-dfu.rules`, and reloads udev.
7. It configures with `cmake ../ -DFORCE_LIBUVC=true -DCMAKE_BUILD_TYPE=release`, builds with `make -j2` and runs `sudo make install`.
8. It prints `Librealsense script completed.`

To run it:

```sh
wget https://github.com/realsenseai/librealsense/raw/master/scripts/libuvc_installation.sh
chmod +x ./libuvc_installation.sh
./libuvc_installation.sh
```

When it finishes, connect the camera and run `rs-enumerate-devices`.

## Troubleshooting

Messages you may see, and where they come from:

| Message | Printed by | What to do |
|---|---|---|
| `RealSense UDEV-Rules are missing!` | `realsense-viewer` ([`common/viewer.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/viewer.cpp)) | Neither `/etc/udev/rules.d/99-realsense-libusb.rules` nor `/lib/udev/rules.d/60-librealsense2-udev-rules.rules` exists. Run `./scripts/setup_udev_rules.sh`. The viewer also writes its built-in copy of the rules to `~/.99-realsense-libusb.rules`, which you can copy to `/etc/udev/rules.d/99-realsense-libusb.rules`. |
| `Multiple realsense udev-rules were found!` | `realsense-viewer` ([`common/viewer.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/viewer.cpp)) | Both the source-install rules file and the Debian package rules file exist. Remove one of the two installations. |
| `RealSense UDEV-Rules file: ... is not up-to date!` | `realsense-viewer` ([`common/viewer.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/viewer.cpp)) | The `##Version=` line of the installed rules is older than the copy built into the viewer. Re-run `./scripts/setup_udev_rules.sh` from the current source, or update the package. |
| `Permission Denied! This is often an indication of outdated or missing udev-rules.` | librealsense during firmware update ([`src/fw-update/fw-update-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/fw-update/fw-update-device.cpp)) | Install or update the udev rules as above. |
| `v4l2-ctl not found, install with: sudo apt install v4l-utils` | `scripts/setup_udev_rules.sh` | Install `v4l-utils` and run the script again. |
| `Unsupported kernel version ...` / `Unsupported distribution ...` | `scripts/patch-utils-hwe.sh` (called by `patch-realsense-ubuntu-lts-hwe.sh`) | The kernel or Ubuntu release is not in the table above. Build with `-DFORCE_RSUSB_BACKEND=ON` instead. |
| `Failed to insert the patched module. Operation is aborted, the original module is restored` | `scripts/patch-utils-hwe.sh` | The patched module does not match the running kernel. Check `uname -r`, update and reboot, then run the script again. |

For more questions, see [[Troubleshooting Q&A]]. For supported cameras, see [[Supported Devices]]. To update camera firmware after installing, see [[Firmware Update]].

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`bc496cd`](https://github.com/realsenseai/librealsense/commit/bc496cdb16beca36c41d2b96170e309fceb38d6e)._
<!-- /wikigen:footer -->
