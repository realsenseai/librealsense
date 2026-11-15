This page points you to the guide for installing the RealSense SDK 2.0 (librealsense) on Ubuntu Linux, either from prebuilt packages or by building from source with CMake. It is for anyone who wants to run the RealSense tools or develop applications against `librealsense2` on a Linux PC. For NVIDIA Jetson boards see [[Installation Jetson]].

## When you need it

- You are setting up a RealSense camera on an Ubuntu PC for the first time.
- You want to build librealsense from source, for example to change build options or use the RSUSB backend.
- A newly released camera streams but shows no per-frame metadata, and you need to patch the kernel modules.
- You need to install or remove the udev rules that let a normal user open the camera.

## Key facts

- On Linux the default backend is V4L2 (the kernel `uvcvideo` and HID drivers). Configuring with `-DFORCE_RSUSB_BACKEND=ON` selects the user-space RSUSB backend instead, which needs no kernel patches (according to doc/libuvc_installation.md) ([`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/unix_config.cmake)).
- If `CMAKE_BUILD_TYPE` is not set, the build uses `Release`, and outputs are placed under `<build dir>/<build type>` ([`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/unix_config.cmake)).
- `BUILD_EXAMPLES` and `BUILD_GRAPHICAL_EXAMPLES` are `ON` by default; `BUILD_WITH_DDS` and `BUILD_WITH_CUDA` are `OFF` ([`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/lrs_options.cmake)). See [[Build Configuration]] for all options.
- With the default V4L2 backend, if libudev is found at configure time the library watches for devices through udev; otherwise CMake prints `UDEV not found; using polling device-watcher!` and the library polls ([`src/linux/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/master/src/linux/CMakeLists.txt)).
- [`scripts/setup_udev_rules.sh`](https://github.com/realsenseai/librealsense/blob/master/scripts/setup_udev_rules.sh) copies [`config/99-realsense-libusb.rules`](https://github.com/realsenseai/librealsense/blob/master/config/99-realsense-libusb.rules) to `/etc/udev/rules.d/` (on Tegra or IPU6 systems also `config/99-realsense-d4xx-mipi-dfu.rules` and the `rs-enum.sh`/`rs_ipu6_d457_bind.sh` helpers) and reloads udev; `--uninstall` removes them. It needs `v4l2-ctl` (package `v4l-utils`) and stops if it is missing. Run it from the repository root, since it uses relative paths.
- [`scripts/patch-realsense-ubuntu-lts-hwe.sh`](https://github.com/realsenseai/librealsense/blob/master/scripts/patch-realsense-ubuntu-lts-hwe.sh) downloads the Ubuntu kernel source for the running kernel, applies the RealSense patches and rebuilds the affected modules; it picks the kernel branch for Ubuntu `focal`, `jammy` and `noble` ([`scripts/patch-utils-hwe.sh`](https://github.com/realsenseai/librealsense/blob/master/scripts/patch-utils-hwe.sh)).
- [`scripts/libuvc_installation.sh`](https://github.com/realsenseai/librealsense/blob/master/scripts/libuvc_installation.sh) downloads `master`, installs the `99-realsense-libusb.rules` and `99-realsense-d4xx-mipi-dfu.rules` udev rules, and builds and installs the SDK with the RSUSB backend (via the deprecated `-DFORCE_LIBUVC=true`) in Release mode under `~/librealsense_build`.

## Full guide

- [doc/installation.md](https://github.com/realsenseai/librealsense/blob/master/doc/installation.md): dependencies, udev rules, kernel patching, building from source and troubleshooting.
- [doc/distribution_linux.md](https://github.com/realsenseai/librealsense/blob/master/doc/distribution_linux.md): installing the prebuilt Debian packages.
- [doc/libuvc_installation.md](https://github.com/realsenseai/librealsense/blob/master/doc/libuvc_installation.md): building with the RSUSB backend.

## Related pages

- [[Build Configuration]]
- [[Installation Jetson]]
- [[Installation Raspberry Pi]]
- [[Docker]]
- [[Troubleshooting Q&A]]
- [[Supported Devices]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
