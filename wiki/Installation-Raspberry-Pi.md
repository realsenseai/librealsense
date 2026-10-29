This page explains how to build and install the RealSense SDK 2.0 (librealsense) from source on a Raspberry Pi running Raspberry Pi OS or another Debian-based Linux. It is for anyone who wants to use a RealSense camera with the SDK tools or with their own C++ or Python application on a Raspberry Pi. The build is the ordinary Linux CMake build, so read [[Installation Linux]] first; this page covers what is different on the Pi.

## Overview

The Raspberry Pi uses the same Linux build as a PC. Nothing in the code is specific to the Raspberry Pi. What changes is the CPU architecture, which [`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/unix_config.cmake) checks with `${CMAKE_C_COMPILER} -dumpmachine`:

| Compiler target | Typical OS | What CMake does |
|---|---|---|
| `aarch64-*` or `arm64-*` | 64-bit Raspberry Pi OS | Prints `Building for ARM64 with NEON optimizations` (or `... without NEON optimizations` when `BUILD_WITH_NEON=OFF`). Adds `-mstrict-align -ftree-vectorize`. |
| `arm-*` | 32-bit Raspberry Pi OS | Prints `Building for 32-bit ARM - using compiler default FPU settings`. Adds `-ftree-vectorize -latomic`. |

The x86 `-mssse3` flags and the `Building with SSE optimizations` path are not used on ARM.

The `BUILD_WITH_NEON` option (default `ON`) exists only when `CMAKE_SYSTEM_PROCESSOR` matches `arm64`, `ARM64`, `aarch64` or `AARCH64` ([`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake)). When it is on, [`CMake/global_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/global_config.cmake) defines `BUILD_WITH_NEON`. The NEON code paths in [`src/proc/neon/`](https://github.com/realsenseai/librealsense/tree/development/src/proc/neon) and the alignment, point cloud and color conversion code are compiled only when both `__ARM_NEON` and `BUILD_WITH_NEON` are defined. A 32-bit build therefore never uses these NEON paths.

The two Linux backends are both available:

- **V4L2** (default, `FORCE_RSUSB_BACKEND=OFF`) uses the kernel's `uvcvideo` driver. The kernel patch scripts in [`scripts/`](https://github.com/realsenseai/librealsense/tree/development/scripts) target Ubuntu and some other distributions. None of them targets the Raspberry Pi OS kernel.
- **RSUSB** (`-DFORCE_RSUSB_BACKEND=ON`) talks to the camera over libusb from user space and does not depend on kernel patches. The repository's old Raspberry Pi guide (according to doc/installation_raspbian.md) builds with this backend, through the deprecated `FORCE_LIBUVC` option. That option still works: it prints a deprecation message and turns on `FORCE_RSUSB_BACKEND` ([`CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/CMakeLists.txt)).

The steps below use RSUSB.

## Build from source on Raspberry Pi OS

### 1. Prerequisites

Unplug the camera and update the system:

```sh
sudo apt-get update && sudo apt-get upgrade
```

Install the build tools and libraries. These are the Debian package names used for the Linux build. See [[Installation Linux]] for what each one is used for.

```sh
sudo apt-get install git wget cmake build-essential
sudo apt-get install libssl-dev libusb-1.0-0-dev libudev-dev pkg-config
sudo apt-get install libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev
sudo apt-get install v4l-utils
```

Some notes for the Pi:

- **CMake**: the top-level [`CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/CMakeLists.txt) requires CMake 3.10 or newer. Check with `cmake --version`. The old guide's steps for building CMake 3.11 by hand are not needed with a current distribution.
- **Network access**: the default configuration downloads third-party sources (Fast-CDR, nlohmann/json, libdeflate, SQLite, yaml-cpp, libcurl) while it configures or builds. The Python bindings also clone pybind11 and pybind11_json. Keep the Pi online for the first build.
- **libusb**: if `libusb-1.0-0-dev` is missing, CMake prints `libusb not found; using internal version` and builds libusb from source ([`CMake/libusb_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/libusb_config.cmake)).
- **GLFW**: if GLFW 3.3 is not found, CMake prints `GLFW 3.3 not found; using internal version` and builds the bundled copy, which needs the X11 development headers. You only need OpenGL and GLFW for the examples and graphical tools. To build without them, see [Headless builds](#headless-builds).
- **v4l-utils**: `scripts/setup_udev_rules.sh` needs `v4l2-ctl` even when you use the RSUSB backend.

The old repository guide ([`doc/installation_raspbian.md`](https://github.com/realsenseai/librealsense/blob/development/doc/installation_raspbian.md)) also installs protobuf, TBB and OpenCV from third-party `.deb` files and GitHub forks. The default librealsense build does not need any of these. OpenCV is used only by optional parts: the OpenCV examples, the OpenVINO examples and the `rs-pcl-color` PCL example (`BUILD_CV_EXAMPLES`, `BUILD_OPENVINO_EXAMPLES`, `BUILD_PCL_EXAMPLES`, all `OFF` by default), and `realsense-viewer` when `OpenCV_DIR` points to an existing folder. Those steps are not repeated here.

### 2. Add swap if the Pi has little RAM

The repository guides say that on a Raspberry Pi with little RAM the compiler can run out of memory. The old guide ([`doc/installation_raspbian.md`](https://github.com/realsenseai/librealsense/blob/development/doc/installation_raspbian.md)) adds swap before building: it sets `CONF_SWAPSIZE=2048` in `/etc/dphys-swapfile` and restarts the `dphys-swapfile` service. [`doc/RaspberryPi3.md`](https://github.com/realsenseai/librealsense/blob/development/doc/RaspberryPi3.md) suggests creating a swap file and compiling again if the build crashes. The swap service and its defaults belong to the OS, so check its documentation for your release.

Building with fewer parallel jobs also reduces memory use (see step 5).

### 3. Get the source

```sh
git clone https://github.com/realsenseai/librealsense.git
cd librealsense
```

Run the commands below from this repository root. The udev script uses relative paths such as `config/`.

### 4. Install the udev rules

udev rules let a normal user open the camera without `sudo`. They are needed with both backends.

```sh
./scripts/setup_udev_rules.sh
```

What [`scripts/setup_udev_rules.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/setup_udev_rules.sh) does:

- It runs with `bash -e`. It exits right away with `v4l2-ctl not found, install with: sudo apt install v4l-utils` if `v4l2-ctl` is missing.
- If any `/dev/video*` node exists, it asks you to remove all RealSense cameras and waits until you press Enter. On a Pi with other video devices, such as a camera module, you will see this prompt as well.
- It copies [`config/99-realsense-libusb.rules`](https://github.com/realsenseai/librealsense/blob/development/config/99-realsense-libusb.rules) to `/etc/udev/rules.d/`. This file sets `MODE:="0666", GROUP:="plugdev"` for the RealSense USB product IDs.
- It installs the MIPI rules and helper scripts only when `v4l2-ctl --list-devices` reports a `tegra` or `ipu6` device, so they are not installed on a Raspberry Pi.
- It runs `udevadm control --reload-rules && udevadm trigger` and prints `udev-rules successfully installed`.

To install the rules by hand instead, which is what the old Raspberry Pi guide does:

```sh
sudo cp config/99-realsense-libusb.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

To remove the rules again, run `./scripts/setup_udev_rules.sh --uninstall`.

Do not apply the Ubuntu kernel patch scripts from [[Installation Linux]] (`patch-realsense-ubuntu-lts-hwe.sh`, `patch-realsense-ubuntu-lts.sh`) on Raspberry Pi OS. They choose the patches from the Ubuntu codename and the kernel version and stop with an `Unsupported distribution` or `Unsupported kernel version` error for anything else ([`scripts/patch-utils-hwe.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/patch-utils-hwe.sh), [`scripts/patch-utils.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/patch-utils.sh)).

### 5. Configure, build and install

```sh
mkdir build && cd build
cmake .. -DFORCE_RSUSB_BACKEND=ON
make -j2
sudo make install
```

Notes:

- **Backend**: CMake prints `using RS2_USE_LIBUVC_BACKEND` ([`src/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/src/CMakeLists.txt)). Without `FORCE_RSUSB_BACKEND=ON` it prints `using RS2_USE_V4L2_BACKEND`.
- **Build type**: when `CMAKE_BUILD_TYPE` is not set, [`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/unix_config.cmake) sets it to `Release`. Outputs go to `build/Release/`.
- **Parallel jobs**: pick the `-j` value to match the Pi's RAM and swap. The old Raspberry Pi guide uses `make -j1`. [`scripts/libuvc_installation.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/libuvc_installation.sh) uses `make -j2`.
- **Install**: `sudo make install` installs to `/usr/local` (headers in `include/librealsense2`, the library in `CMAKE_INSTALL_LIBDIR`, tools and examples in `bin`) and then runs `ldconfig` ([`CMake/install_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/install_config.cmake)). Because `ldconfig` runs, you normally do not need the old guide's `LD_LIBRARY_PATH=/usr/local/lib` export. Add it only if your system's loader configuration does not include `/usr/local/lib`.

Then connect the camera and check the installation:

```sh
rs-enumerate-devices
realsense-viewer
```

`realsense-viewer` needs a desktop session with working OpenGL. See [[Realsense Viewer Manual]] and [[Command Line Tools]].

### Headless builds

On a Pi without a display, or to save build time, leave out the graphical parts:

```sh
# Library, non-graphical examples and command-line tools (no realsense-viewer)
cmake .. -DFORCE_RSUSB_BACKEND=ON -DBUILD_GRAPHICAL_EXAMPLES=OFF

# Library and command-line tools only (no examples, no OpenGL/GLFW needed)
cmake .. -DFORCE_RSUSB_BACKEND=ON -DBUILD_EXAMPLES=OFF
```

With `BUILD_GRAPHICAL_EXAMPLES=OFF`, CMake also turns off `CHECK_FOR_UPDATES` and `ENABLE_AI_ASSISTANT` and prints `CHECK_FOR_UPDATES/ENABLE_AI_ASSISTANT depend on BUILD_GRAPHICAL_EXAMPLES flag, turning them off..` ([`CMake/global_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/global_config.cmake)).

### All-in-one script

[`scripts/libuvc_installation.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/libuvc_installation.sh) does the RSUSB build in one step. It creates a 2 GB `/swapfile` if no swap is active, installs its packages with `apt-get`, downloads the **`master`** branch archive (not your checkout and not `development`), installs the udev rules, and builds with `-DFORCE_LIBUVC=true -DCMAKE_BUILD_TYPE=release` and `make -j2`. [[Installation Linux]] describes it in detail. The package list in the script is fixed. If a package name does not exist on your Raspberry Pi OS release, the script stops (it runs with `bash -xe`).

## Build options for ARM

All options are listed on [[Build Configuration]]. These are the ones that matter on a Raspberry Pi:

| Option | Default | Use on the Pi |
|---|---|---|
| `FORCE_RSUSB_BACKEND` | `OFF` | Set to `ON` to use the libusb backend, which needs no kernel patches. |
| `BUILD_WITH_NEON` | `ON` | 64-bit ARM only. Enables the NEON code paths. Set to `OFF` to build without them. The option does not exist on 32-bit ARM. |
| `BUILD_WITH_OPENMP` | `OFF` | When `ON`, CMake requires OpenMP (`find_package(OpenMP REQUIRED)`) and adds its compiler and linker flags. When `OFF`, it adds `-pthread` instead ([`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/unix_config.cmake)). |
| `BUILD_EXAMPLES` | `ON` | Set to `OFF` for a library-and-tools build without OpenGL. |
| `BUILD_GRAPHICAL_EXAMPLES` | `ON` | Set to `OFF` to skip `realsense-viewer` and the other OpenGL tools. |
| `BUILD_PYTHON_BINDINGS` | `OFF` | Build `pyrealsense2`. See below. |
| `CMAKE_BUILD_TYPE` | `Release` (when unset) | Output folder is `build/<build type>/`. |

## Python bindings

`pyrealsense2` is built with the same CMake build. For the API and examples, see [[Python Wrapper]] and [`wrappers/python/examples/`](https://github.com/realsenseai/librealsense/tree/development/wrappers/python/examples).

1. Install Python 3 and its development files. The Python guide [`wrappers/python/readme.md`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/readme.md) uses:
   ```sh
   sudo apt-get install python3 python3-dev
   ```
2. Configure with the bindings on, in the same build folder:
   ```sh
   cmake .. -DFORCE_RSUSB_BACKEND=ON -DBUILD_PYTHON_BINDINGS=ON -DPYTHON_EXECUTABLE=$(which python3)
   make -j2
   sudo make install
   ```

What the build does ([`CMake/external_pybind11.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/external_pybind11.cmake), [`wrappers/python/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/CMakeLists.txt)):

- It clones pybind11 and pybind11_json from GitHub while it configures and prints `Fetching pybind11...` and `Fetching pybind11_json...`. If a download fails, configuration stops with `Failed to download pybind11` or `Failed to download pybind11_json` ([`CMake/external_pybind11.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/external_pybind11.cmake)). If CMake's earlier connectivity check fails, [`wrappers/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/CMakeLists.txt) also warns `No internet connection. Cloning Python bindings may fail`, but only after the downloads have succeeded.
- `PYTHON_EXECUTABLE` is passed on to the pybind11 build.
- With CMake 3.12 or newer, it finds Python with `find_package(Python REQUIRED COMPONENTS Interpreter Development)`. It installs the module into `${Python_SITEARCH}/pyrealsense2`, the interpreter's own platform-specific site-packages folder, together with `pyrealsense2/__init__.py`. With an older CMake, the folder is `<libdir>/python<major>.<minor>/pyrealsense2` under the install prefix.
- The built module is also in `build/Release/`, next to `librealsense2.so`.

You can override the install folder with `-DPYTHON_INSTALL_DIR=<path>` (a cache variable). Because the module goes into the interpreter's site-packages folder, you usually do not need to set `PYTHONPATH`. The old guide's `export PYTHONPATH=$PYTHONPATH:/usr/local/lib` applies only to installs that put the module there.

Check the installation:

```sh
python3 -c "import pyrealsense2 as rs; print(rs.__version__)"
```

The old guide's Python 2 instructions, the `pyopengl` packages and the `raspi-config` GL driver setting are not needed to build or import `pyrealsense2`, so they are not repeated here. Some Python examples use OpenGL or OpenCV and need their own packages.

## Camera performance on the Pi

The following comes from the repository guide [`doc/RaspberryPi3.md`](https://github.com/realsenseai/librealsense/blob/development/doc/RaspberryPi3.md) and cannot be confirmed from the code. That guide says:

- Older boards such as the Raspberry Pi 3 Model B have only USB 2 ports, and camera capabilities are severely reduced over USB 2 because of the lower bandwidth.
- On that board it streamed depth at 480x270 and 30 FPS, and depth with infrared at 424x240 and 30 FPS, in the RealSense Viewer.

Which stream profiles are available over USB 2 depends on the camera and its firmware. Use `rs-enumerate-devices` to list the profiles your camera offers on the port it is connected to. For supported cameras, see [[Supported Devices]].

## Troubleshooting

| Problem or message | Where it comes from | What to do |
|---|---|---|
| The compiler is killed or the build stops with an internal compiler error | System out of memory | Add swap (step 2) and build with fewer jobs (`make -j1`). |
| `RealSense UDEV-Rules are missing!` | `realsense-viewer` ([`common/viewer.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/viewer.cpp)) | Neither `/etc/udev/rules.d/99-realsense-libusb.rules` nor `/lib/udev/rules.d/60-librealsense2-udev-rules.rules` exists. Run `./scripts/setup_udev_rules.sh`. The viewer also writes its built-in copy to `~/.99-realsense-libusb.rules`, which you can copy to `/etc/udev/rules.d/`. |
| `Multiple realsense udev-rules were found!` | `realsense-viewer` | Both the source-install rules and the Debian package rules exist. Remove one of them. |
| `v4l2-ctl not found, install with: sudo apt install v4l-utils` | `scripts/setup_udev_rules.sh` | Install `v4l-utils` and run the script again. |
| `Remove all RealSense cameras attached. Hit any key when ready` | `scripts/setup_udev_rules.sh` | Printed whenever any `/dev/video*` node exists. Unplug the camera and press Enter. |
| `Permission Denied! This is often an indication of outdated or missing udev-rules.` | Error thrown by librealsense when USB access to a camera in firmware-update (DFU) mode is denied, either while the device is opened or during the update ([`src/fw-update/fw-update-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/fw-update/fw-update-device.cpp)). The application gets it as a librealsense error. | Install or update the udev rules. |
| `No internet connection. Cloning Python bindings may fail` | `wrappers/CMakeLists.txt` | Connect the Pi to the network and configure again. |
| `import pyrealsense2` fails | Python cannot find the module | Check that you ran `sudo make install` with the same `python3` you import from. Check the install folder printed by `make install`, or set `PYTHON_INSTALL_DIR` and configure again. |
| The camera is listed, but high resolutions or frame rates are missing | USB 2 port or bandwidth | See [Camera performance on the Pi](#camera-performance-on-the-pi). Use a USB 3 port where the board has one. |

For more questions, see [[Troubleshooting Q&A]]. To update camera firmware, see [[Firmware Update]].

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`de7fa8c`](https://github.com/realsenseai/librealsense/commit/de7fa8cf3e05bac6f3ce6b5eeec2eb766c57259e)._
<!-- /wikigen:footer -->
