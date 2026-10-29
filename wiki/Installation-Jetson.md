This page explains how to install the RealSense SDK 2.0 (librealsense) on NVIDIA Jetson boards running L4T (Linux for Tegra), the Ubuntu-based image that NVIDIA ships with JetPack. It is for anyone who wants to use a RealSense camera on a Jetson, either with the prebuilt packages or by building from source with CUDA. Most of the generic Linux steps are the same as on a PC and are described on [[Installation Linux]]; this page covers what is different on Jetson.

## Overview

A Jetson runs Linux on ARM64, so librealsense uses the same two Linux backends as on a PC. CMake selects one in [`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/unix_config.cmake):

| Backend | Selected when | Kernel patches |
|---|---|---|
| V4L2 (`RS2_USE_V4L2_BACKEND`) | default (`FORCE_RSUSB_BACKEND=OFF`) | Uses the L4T kernel's `uvcvideo` and HID drivers. For full support (depth formats, per-frame metadata, HID motion sensors), patch the kernel modules with [`scripts/patch-realsense-ubuntu-L4T.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/patch-realsense-ubuntu-L4T.sh). |
| RSUSB (`RS2_USE_LIBUVC_BACKEND`) | `-DFORCE_RSUSB_BACKEND=ON` | Not needed. UVC and HID are implemented in user space over libusb. |

The backend is fixed when you configure the build; one build cannot use both.

The Jetson guide [`doc/installation_jetson.md`](https://github.com/realsenseai/librealsense/blob/development/doc/installation_jetson.md) recommends the native kernel drivers (V4L2 backend), especially in production. It describes RSUSB as a way to get a working SDK without touching the kernel, with some performance and functional limitations (for example with multiple cameras). The L4T kernel is configured and flashed differently from a desktop Ubuntu kernel, which is why the patch script rebuilds only the affected modules instead of the whole kernel image.

You can install librealsense on a Jetson in these ways:

1. **Prebuilt packages** from the RealSense apt repository.
2. **Build from source with the RSUSB backend**, which needs no kernel patches.
3. **Patch the kernel modules and build from source with the V4L2 backend**.

Each build can enable CUDA with `-DBUILD_WITH_CUDA=ON`. See [Building with CUDA](#building-with-cuda).

Before you start, set up the board with NVIDIA's tools (JetPack, usually installed with NVIDIA SDK Manager). For CUDA builds the CUDA toolkit from JetPack must be installed.

## Install the prebuilt packages

The packages are built and published outside this repository, so the source code cannot confirm which JetPack releases they support. The repository guides describe the current policy:

- [`doc/installation_jetson.md`](https://github.com/realsenseai/librealsense/blob/development/doc/installation_jetson.md) says the prebuilt Debian packages need at least JetPack 5.0.2 (L4T 35.1, CUDA 11.4), and that a lower version may not work because of CUDA version incompatibilities.
- [`doc/distribution_linux.md`](https://github.com/realsenseai/librealsense/blob/development/doc/distribution_linux.md) has the steps to register the server's public key and add the apt repository. They are the same as on a PC; see [[Installation Linux]].

After adding the repository, the Jetson guide installs:

```sh
sudo apt-get install librealsense2-utils
sudo apt-get install librealsense2-dev
```

Then reconnect the camera and run `realsense-viewer`. The guide also notes that you may need to run the kernel patch script described [below](#build-from-source-with-the-v4l2-backend-and-kernel-patches) for the camera to work on your platform.

### Python wheels

The `pyrealsense2` package on PyPI also has `aarch64` wheels. The Python wrapper guide [`wrappers/python/README.rst`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/README.rst) says these wheels are CUDA builds, one per JetPack line, and that the wheel's Python version selects the JetPack it was built for:

| Runs on | Built on | CUDA | Wheel Python version |
|---|---|---|---|
| JetPack 5.x | JetPack 5.0.2 | 11.4 | 3.9 |
| JetPack 6.x | JetPack 6.0 | 12.2 | 3.10 |
| JetPack 7.x | JetPack 7.0 | 13.0 | 3.12 |

Install with the Python version that matches your JetPack, for example on JetPack 6:

```sh
python3.10 -m pip install pyrealsense2
```

The guide says that a wheel for another JetPack's CUDA installs without error, but the GPU-accelerated paths (such as RGB8/BGR8 color conversion, align and pointcloud) then fail. For other Python versions, or to run without CUDA, build the wrapper from source. See [[Python Wrapper]].

## Build from source

### 1. Prerequisites

Install the same build tools and libraries as on a PC. The full list, and what the build does with each package, is on [[Installation Linux]]:

```sh
sudo apt-get install git wget cmake build-essential
sudo apt-get install libssl-dev libusb-1.0-0-dev libudev-dev pkg-config
sudo apt-get install libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev
sudo apt-get install v4l-utils
```

Then get the source and stay in the repository root. The scripts below use paths relative to it (`./scripts/`, `config/`):

```sh
git clone https://github.com/realsenseai/librealsense.git
cd librealsense
```

### 2. Install the udev rules

```sh
./scripts/setup_udev_rules.sh
```

[`scripts/setup_udev_rules.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/setup_udev_rules.sh) works the same way as on a PC (see [[Installation Linux]]). It stops if `v4l2-ctl` is missing, asks you to remove the cameras if any `/dev/video*` node exists, and copies [`config/99-realsense-libusb.rules`](https://github.com/realsenseai/librealsense/blob/development/config/99-realsense-libusb.rules) to `/etc/udev/rules.d/`.

On Jetson one more step can apply: if `v4l2-ctl --list-devices` reports a `tegra` device, the script also installs the MIPI camera files:

- [`config/99-realsense-d4xx-mipi-dfu.rules`](https://github.com/realsenseai/librealsense/blob/development/config/99-realsense-d4xx-mipi-dfu.rules) into `/etc/udev/rules.d/`
- [`scripts/rs-enum.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/rs-enum.sh) and [`scripts/rs_ipu6_d457_bind.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/rs_ipu6_d457_bind.sh) into `/usr/local/bin/`

To remove the rules, run `./scripts/setup_udev_rules.sh --uninstall`.

### 3. Choose the backend

Continue with one of the two sections below: [RSUSB](#build-from-source-with-the-rsusb-backend) (no kernel changes) or [V4L2 with kernel patches](#build-from-source-with-the-v4l2-backend-and-kernel-patches).

## Build from source with the RSUSB backend

After the prerequisites and udev rules, configure and build with the RSUSB backend. Add `-DBUILD_WITH_CUDA=ON` if the CUDA toolkit is installed:

```sh
mkdir build && cd build
cmake .. -DFORCE_RSUSB_BACKEND=ON -DBUILD_WITH_CUDA=ON
make -j$(($(nproc)-1))
sudo make install
```

CMake prints `using RS2_USE_LIBUVC_BACKEND` for this backend. The deprecated `FORCE_LIBUVC` option still works: it prints a deprecation message and turns on `FORCE_RSUSB_BACKEND` ([`CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/CMakeLists.txt)).

The Jetson guide also points to the all-in-one script [`scripts/libuvc_installation.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/libuvc_installation.sh), described on [[Installation Linux]]. Note that it:

- downloads and builds the **`master`** branch archive into `~/librealsense_build`, not your local checkout;
- creates a 2 GB `/swapfile` if no swap is active;
- configures with `cmake ../ -DFORCE_LIBUVC=true -DCMAKE_BUILD_TYPE=release` only, so it does **not** enable CUDA.

To get a CUDA build with RSUSB, use the manual commands above.

## Build from source with the V4L2 backend and kernel patches

### 1. Patch the kernel modules

Before you run the script:

- Connect the board to the internet. The script downloads the kernel sources.
- Check that you have enough free space (`df -h`). The Jetson guide says the process needs about 2.5 GB.
- Unplug the RealSense cameras.

From the repository root (note the `L4T` suffix):

```sh
./scripts/patch-realsense-ubuntu-L4T.sh
```

[`scripts/patch-realsense-ubuntu-L4T.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/patch-realsense-ubuntu-L4T.sh) runs with `set -e`, so it stops at the first failing command. It does the following:

1. If any `/dev/video*` node exists, it prints `Remove all RealSense cameras attached. Hit any key when ready` and waits for Enter.
2. It loads the helper functions from [`scripts/patch-utils-hwe.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/patch-utils-hwe.sh). If `/sys/devices/pwm-fan/target_pwm` exists, it sets the fan to `200` to limit heat during the build.
3. It reads the board name from `/proc/device-tree/model` and the L4T version from `/etc/nv_tegra_release`. If that file is missing, it prints `/etc/nv_tegra_release not present, aborting script` and stops.
4. It picks the patch set for the L4T version (see the table below). Any other version ends with `Unsupported JetPack revision <version> aborting script`.
5. It installs `build-essential git libssl-dev curl`.
6. It creates a `Tegra` folder in the current directory and copies [`scripts/Tegra/sync.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/Tegra/sync.sh) and the matching `.repos` file into it.
7. It downloads NVIDIA's license text for the kernel sources and shows it. You must press a key within 30 seconds to accept; otherwise the `read -t 30` times out and the script stops. If the license cannot be downloaded, it prints `License link not found` and exits.
8. It runs `sync.sh -k jetson_<L4T version>` (for 39.2, `jetson_39.2.0`) to fetch the kernel source for your L4T tag. `sync.sh` first checks which NVIDIA git server is reachable (`gitlab.com/nvidia/nv-tegra` or `nv-tegra.nvidia.com`).
9. It configures the kernel tree, applies the RealSense patches and builds the `uvcvideo`, `v4l2-core` and (for JetPack 6 and 7) the HID and IIO motion-sensor modules.
10. It installs the modules, runs `depmod` and reloads them. On success it prints `Script has completed. Please consult the installation guide for further instruction.`

L4T versions the script accepts, and what happens next:

| L4T (from `/etc/nv_tegra_release`) | JetPack (comments in the script) | Kernel | Patch set | Result |
|---|---|---|---|---|
| 32.2.1, 32.2.3, 32.3.1, 32.4.3 | JetPack 4 | 4.9 | `4.4` | Accepted, but `scripts/Tegra/4.4.repos` is not in the repository, so the script stops when it tries to copy it. |
| 32.4.4, 32.5, 32.5.1, 32.6.1, 32.7.1 | JetPack 4.4.1 to 4.6.1 | 4.9 | `4.4.1` | Accepted, but `scripts/Tegra/4.4.1.repos` is not in the repository, so the script stops the same way. |
| 35.1, 35.4.1, 35.6.4 | 5.0.2, 5.1.2, 5.1.6 | 5.10 | `5.0` | Builds and installs the patched `uvcvideo`. See the note below about the HID modules. |
| 36.3, 36.4, 36.4.3, 36.4.4, 36.4.7, 36.5 | 6.0, 6.1, 6.2, 6.2.1, 6.2.2 | 5.15 (`jammy` source) | `6.0` | Builds and installs the patched video and HID/IIO modules. |
| 38.2, 38.2.1, 38.2.2, 38.4, 39.2 | 7.0, 7.1, 7.2 | 6.8 (`noble` source) | `7.0` | Builds and installs the patched video and HID/IIO modules. |

What the script installs, by patch set:

- **`5.0` (L4T 35.x)**: applies `01-realsense-camera-formats-L4T-5.0.patch`, `02-realsense-metadata-L4T-5.0.patch` and `05-realsense-powerlinefrequency-control-fix.patch` from [`scripts/Tegra/LRS_Patches/`](https://github.com/realsenseai/librealsense/tree/development/scripts/Tegra/LRS_Patches). The built modules are copied to your home folder as `jetson_<L4T>-uvcvideo.ko` (and the `videobuf` modules), then `uvcvideo` is replaced with the helper `try_module_insert`. That helper backs up the original module, and restores it if the patched one fails to load. The script then tries to install `hid_sensor_gyro_3d` and `hid_sensor_accel_3d` from `~/jetson_<L4T>-hid-sensor-*.ko` files that only the `4.4` patch set creates, so on this path it stops at that step after `uvcvideo` has been replaced.
- **`6.0` and `7.0` (L4T 36.x, 38.x, 39.2)**: adds the HID sensor options (`CONFIG_HID_SENSOR_HUB`, `CONFIG_HID_SENSOR_ACCEL_3D`, `CONFIG_HID_SENSOR_GYRO_3D` and others) to the kernel config. It applies the Ubuntu `jammy` or `noble` patches from [`scripts/`](https://github.com/realsenseai/librealsense/tree/development/scripts) (`realsense-camera-formats-<codename>-master.patch`, `realsense-metadata-<codename>-master.patch`, plus `realsense-powerlinefrequency-control-fix-jammy.patch` or `makefile-noble-7.0.patch` when present). It also changes the `uvcvideo` version string from `1.1.1` to `1.1.1-realsense`. The modules (`uvcvideo`, `videodev`, `kfifo_buf`, `industrialio-triggered-buffer`, `hid-sensor-iio-common`, `hid-sensor-hub`, `hid-sensor-accel-3d`, `hid-sensor-gyro-3d`, `hid-sensor-trigger`) are copied to `/lib/modules/$(uname -r)/extra/`. `/etc/depmod.d/ubuntu.conf` is edited so that `extra` is searched before `updates`. Then the old modules are unloaded with `modprobe -r` and the new ones loaded with `modprobe`.

The Jetson guide says the method was verified on Jetson AGX Xavier with JetPack 5.0.2, Jetson AGX Orin with JetPack 6.0, and Jetson AGX Thor with JetPack 7.0 (beta level). For Jetson Nano it points to external instructions.

### 2. Configure, build and install

From the repository root:

```sh
mkdir build && cd build
cmake .. -DFORCE_RSUSB_BACKEND=OFF -DBUILD_WITH_CUDA=ON -DCMAKE_BUILD_TYPE=Release
make -j$(($(nproc)-1))
sudo make install
```

`FORCE_RSUSB_BACKEND` is already `OFF` by default, and an unset `CMAKE_BUILD_TYPE` becomes `Release` on Linux ([`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/unix_config.cmake)), so both flags only make the choice explicit. CMake prints `using RS2_USE_V4L2_BACKEND`. Leave out `-DBUILD_WITH_CUDA=ON` if the CUDA toolkit is not installed (see [Building with CUDA](#building-with-cuda)). Outputs and install locations are the same as on a PC; see [[Installation Linux]].

Then connect the camera and run `rs-enumerate-devices` or `realsense-viewer`.

## Building with CUDA

`BUILD_WITH_CUDA` is `OFF` by default ([`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake)). When you turn it on, [`CMake/global_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/global_config.cmake) defines `RS2_USE_CUDA` and includes [`CMake/cuda_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/cuda_config.cmake), which:

- prints `Building with CUDA..`, calls `enable_language( CUDA )` and `find_package(CUDA REQUIRED)`, so configuration fails if CMake cannot find the CUDA toolkit and the `nvcc` compiler. JetPack installs CUDA under `/usr/local/cuda`; if CMake does not find `nvcc`, put `/usr/local/cuda/bin` on your `PATH`.
- links the CUDA runtime, cuSPARSE and cuBLAS libraries.
- picks the GPU architectures from the CUDA version. The Jetson ones are:

| Jetson GPU | Architecture | Added when |
|---|---|---|
| Xavier | `72` | CUDA 11.0 or newer, and older than 13.0 |
| Orin | `87` | CUDA 11.4 or newer, and older than 13.0 |
| Thor | `110` | CUDA 13.0 or newer |

  A comment in the file notes that without the Jetson entries the kernels build for `sm_86` but fail to launch on Orin with "no kernel image available".

Related options:

| Option | Default | Effect |
|---|---|---|
| `BUILD_WITH_CUDA` | `OFF` | Enable CUDA. |
| `BUILD_WITH_CUDA_ZEROCOPY` | same as `BUILD_WITH_CUDA` | Zero-copy GPU memory path for integrated GPUs such as Jetson. CMake stops with `BUILD_WITH_CUDA_ZEROCOPY requires BUILD_WITH_CUDA=ON` if it is on without CUDA. At run time it is used only when the GPU is integrated ([`src/cuda/cuda-frame-memory.cu`](https://github.com/realsenseai/librealsense/blob/development/src/cuda/cuda-frame-memory.cu)). |
| `BUILD_WITH_NEON` | `ON` (ARM64 only) | ARM64 NEON optimizations. Defined only when `CMAKE_SYSTEM_PROCESSOR` is ARM64/AArch64. |
| `BUILD_WITH_CLOSE_RANGE_DEPTH` | `OFF` (ARM64 only) | Improved Close Range Depth ("Jetson only, requires enhanced-depth library"). See [[Improved Close Range Depth]]. |

`BUILD_WITH_CUDA` and `BUILD_WITH_HIP` cannot both be on. For what CUDA accelerates and how to check it at run time, see [[GPU Acceleration]]. All other options are on [[Build Configuration]].

## Troubleshooting

Messages you may see, and where they come from:

| Message | Printed by | What to do |
|---|---|---|
| `Running on NVIDIA Jetson without the CUDA runtime installed.` | `realsense-viewer` and `rs-depth-quality` ([`common/viewer.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/viewer.cpp)), when `/etc/nv_tegra_release` exists, the build has no CUDA, and `/usr/local/cuda` does not exist | Install CUDA through NVIDIA JetPack, then rebuild with `-DBUILD_WITH_CUDA=ON`. |
| `Running on NVIDIA Jetson with the CUDA runtime installed,` ... `rebuild librealsense with -DBUILD_WITH_CUDA=ON.` | `realsense-viewer` and `rs-depth-quality`, when `/etc/nv_tegra_release` exists, the build has no CUDA, and `/usr/local/cuda` exists | Rebuild with `-DBUILD_WITH_CUDA=ON`. |
| `Running on NVIDIA Jetson and realsense-viewer was built with CUDA,` ... `the CUDA runtime failed to initialize` | `realsense-viewer` and `rs-depth-quality`, when `/etc/nv_tegra_release` exists, the build has CUDA, but no GPU device is reported | Check the CUDA driver and library stack on the board. The SDK log has the `cudaGetDeviceCount` error. |
| `/etc/nv_tegra_release not present, aborting script` | `scripts/patch-realsense-ubuntu-L4T.sh` | The script runs only on an L4T image. |
| `Unsupported JetPack revision ... aborting script` | `scripts/patch-realsense-ubuntu-L4T.sh` | Your L4T version is not in the table above. Build with `-DFORCE_RSUSB_BACKEND=ON` instead. |
| `License link not found` | `scripts/patch-realsense-ubuntu-L4T.sh` | The NVIDIA license page for your L4T release could not be downloaded. Check the network connection. |
| `Failed to unload module <name>. Error type <n>. Try rebooting` | `scripts/patch-utils-hwe.sh` (used by the L4T script) | A module is still in use. Unplug the cameras, stop programs that use them, reboot and run the script again. |
| `Failed to reload module <name>. Error type <n>. Operation is aborted` | `scripts/patch-utils-hwe.sh` | A module did not load: a patched module (JetPack 6 and 7 path) or a dependent module being reloaded (JetPack 5 path). Check that `uname -r` matches the kernel the modules were built for. |
| `Failed to insert the patched module. Operation is aborted, the original module is restored` | `scripts/patch-utils-hwe.sh` | The patched module (JetPack 5 path) did not load and the original was restored. Check that the running kernel matches your L4T release. |
| `RealSense UDEV-Rules are missing!` | `realsense-viewer` and `rs-depth-quality` | Run `./scripts/setup_udev_rules.sh`. See [[Installation Linux]] for the other udev messages. |

If the patch script stopped halfway, the `Tegra` folder it created in the repository root is reused on the next run. `sync.sh` prints `...but the directory is not a git repository -- clean it up first` and stops if a source folder there is not a git checkout; delete that folder and run the script again.

For more questions, see [[Troubleshooting Q&A]]. For supported cameras, see [[Supported Devices]]. To update camera firmware, see [[Firmware Update]].

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`de7fa8c`](https://github.com/realsenseai/librealsense/commit/de7fa8cf3e05bac6f3ce6b5eeec2eb766c57259e)._
<!-- /wikigen:footer -->
