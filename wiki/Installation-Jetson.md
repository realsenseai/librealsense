This page points you to the guide for installing the RealSense SDK 2.0 (librealsense) on NVIDIA Jetson boards running L4T (Linux for Tegra), the Ubuntu-based image NVIDIA ships with JetPack. It is for anyone who wants to use a RealSense camera on a Jetson, with the prebuilt packages or by building from source. The generic Linux steps are on [[Installation Linux]].

## When you need it

- You are connecting a RealSense camera to a Jetson board for the first time.
- You want to build librealsense on a Jetson with CUDA enabled.
- You need to decide between the native V4L2 backend (with patched kernel modules) and the RSUSB backend (no kernel changes).

## Key facts

- The backend is chosen at configure time: V4L2 by default, or RSUSB with `-DFORCE_RSUSB_BACKEND=ON`; one build uses only one of them ([`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/unix_config.cmake)).
- `BUILD_WITH_CUDA` is `OFF` by default; pass `-DBUILD_WITH_CUDA=ON` to enable CUDA ([`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/lrs_options.cmake)). See [[GPU Acceleration]].
- [`scripts/patch-realsense-ubuntu-L4T.sh`](https://github.com/realsenseai/librealsense/blob/master/scripts/patch-realsense-ubuntu-L4T.sh) reads the L4T version from `/etc/nv_tegra_release` and aborts if that file is missing. It downloads the NVIDIA kernel sources, applies the patches from [`scripts/Tegra/LRS_Patches`](https://github.com/realsenseai/librealsense/tree/master/scripts/Tegra/LRS_Patches) and rebuilds the affected kernel modules instead of replacing the kernel image.
- The L4T script accepts only the L4T releases listed in its `case` statement (32.x, 35.x, 36.x, 38.x and 39.2 lines); for any other version it prints `Unsupported JetPack revision ... aborting script` and exits.
- Before downloading, the L4T script shows the NVIDIA license and waits for a key press within 30 seconds to accept it.
- On a Tegra system, [`scripts/setup_udev_rules.sh`](https://github.com/realsenseai/librealsense/blob/master/scripts/setup_udev_rules.sh) also installs [`config/99-realsense-d4xx-mipi-dfu.rules`](https://github.com/realsenseai/librealsense/blob/master/config/99-realsense-d4xx-mipi-dfu.rules) and copies `scripts/rs-enum.sh` and `scripts/rs_ipu6_d457_bind.sh` to `/usr/local/bin`, in addition to the standard USB rules.
- [`scripts/libuvc_installation.sh`](https://github.com/realsenseai/librealsense/blob/master/scripts/libuvc_installation.sh) builds and installs the SDK with the RSUSB backend, so no kernel patching is needed.

## Full guide

- [doc/installation_jetson.md](https://github.com/realsenseai/librealsense/blob/master/doc/installation_jetson.md): prerequisites, backend choice, Debian packages, and building with either backend.

## Related pages

- [[Installation Linux]]
- [[Build Configuration]]
- [[GPU Acceleration]]
- [[Docker]]
- [[Troubleshooting Q&A]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
