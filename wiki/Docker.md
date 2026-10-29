The librealsense repository contains a Dockerfile and helper scripts that build a small Ubuntu-based container image with the RealSense SDK 2.0 command-line tools, non-graphical examples and the Python bindings. Use it if you want to run RealSense tools or a librealsense-based application in a container on a Linux host with a camera connected over USB. Everything described here is in [`scripts/Docker/`](https://github.com/realsenseai/librealsense/tree/development/scripts/Docker).

## Overview

The folder has these files:

| File | What it does |
|---|---|
| [`Dockerfile`](https://github.com/realsenseai/librealsense/blob/development/scripts/Docker/Dockerfile) | Two-stage build: a builder stage that downloads and compiles a released librealsense version, and a runtime stage that holds only the installed binaries and runtime packages. |
| [`build_image.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/Docker/build_image.sh) | Builds the runtime image for the host platform and tags it `librealsense`. |
| [`build_image_multi_platform.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/Docker/build_image_multi_platform.sh) | Builds the image for `linux/amd64` and `linux/arm64` with `docker buildx` and saves each one to a tar file. |
| [`run_image.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/Docker/run_image.sh) | Starts the `librealsense` image with access to the host's camera devices. |
| [`readme.md`](https://github.com/realsenseai/librealsense/blob/development/scripts/Docker/readme.md) | The repository guide for the Docker image. |
| [`LRS_Docker_Depth_example.gif`](https://github.com/realsenseai/librealsense/blob/development/scripts/Docker/LRS_Docker_Depth_example.gif) | Animated image that `readme.md` shows after its `rs-depth` example command. |

The image does **not** contain your local source tree. The Dockerfile downloads the source archive of a release tag from GitHub and builds that. To containerize your own changes, you have to change the Dockerfile.

## Build the image

You need Docker on a Linux host, a clone of the librealsense repository with its tags, and network access to GitHub from the build.

### With the helper script

Run the script from the `scripts/Docker` folder, because it passes `.` as the build context and Docker looks for the `Dockerfile` there:

```sh
cd scripts/Docker
./build_image.sh
```

The script:

1. Finds the most recent tag reachable from your checkout with `` LIBRS_GIT_TAG=`git describe --abbrev=0 --tags` `` (for example `v2.59.1`), and strips the leading `v` to get `LIBRS_VERSION`.
2. Prints `Building images for librealsense version ${LIBRS_VERSION}`.
3. Runs:
   ```sh
   docker build \
           --target librealsense \
           --build-arg LIBRS_VERSION=$LIBRS_VERSION \
           --tag librealsense \
           --tag librealsense --tag librealsense:$LIBRS_GIT_TAG \
           .
   ```

The result is an image tagged both `librealsense` (that is, `librealsense:latest`) and `librealsense:<tag>`, for example `librealsense:v2.59.1`. The script has no command-line flags.

### With `docker build` directly

To build a different release, or a different base image, call `docker build` yourself from `scripts/Docker`. `LIBRS_VERSION` is required, written without the leading `v`:

```sh
docker build --target librealsense \
    --build-arg LIBRS_VERSION=2.59.1 \
    --build-arg BASE_IMAGE=ubuntu:20.04 \
    --tag librealsense .
```

If `LIBRS_VERSION` is missing, the build stops at the `RUN test -n "$LIBRS_VERSION"` step. The version must match a tag `v<LIBRS_VERSION>` on GitHub, because the source is downloaded from `https://codeload.github.com/realsenseai/librealsense/tar.gz/refs/tags/v$LIBRS_VERSION`.

### For x86_64 and arm64

[`build_image_multi_platform.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/Docker/build_image_multi_platform.sh) needs Docker with `buildx`, and for the arm64 build on an x86 host, emulation for that platform. It computes `LIBRS_VERSION` the same way as `build_image.sh` and then runs two builds:

```sh
# Build x86_64 image
docker buildx \
    build \
    --platform linux/amd64 \
    --target librealsense \
    --build-arg LIBRS_VERSION=$LIBRS_VERSION \
    --tag librealsense --tag librealsense:$LIBRS_GIT_TAG \
    -o type=docker,dest=- . > librealsense-amd64.tar

# Build arm64 image (slow!)
docker buildx \
    build \
    --platform linux/arm64 \
    --target librealsense \
    --tag librealsense --tag librealsense:$LIBRS_GIT_TAG \
    --build-arg LIBRS_VERSION=$LIBRS_VERSION \
    -o type=docker,dest=- . > librealsense-arm64.tar
```

The images are written to `librealsense-amd64.tar` and `librealsense-arm64.tar` in the current folder. They are not loaded into the local Docker image list or pushed to a registry. Load one with `docker load -i librealsense-arm64.tar` on the machine where you want to run it.

## Run the container with a camera

Connect the camera to the host, then run:

```sh
./run_image.sh
```

[`run_image.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/Docker/run_image.sh) runs:

```sh
docker run -it --rm \
    -v /dev:/dev \
    --device-cgroup-rule "c 81:* rmw" \
    --device-cgroup-rule "c 189:* rmw" \
    librealsense
```

What each option does:

| Option | Purpose |
|---|---|
| `-it` | Interactive terminal. |
| `--rm` | Removes the container when it exits. |
| `-v /dev:/dev` | Mounts the host's whole `/dev` folder into the container, so the camera's device nodes are visible, including ones created after the container starts (for example after a reconnect). |
| `--device-cgroup-rule "c 81:* rmw"` | Allows read, write and mknod on character devices with major number 81 (video devices). |
| `--device-cgroup-rule "c 189:* rmw"` | Allows read, write and mknod on character devices with major number 189 (USB devices). |

The script comment describes the two cgroup rules as granting the container access "to the camera and usb endpoints of the machine". The script does not use `--privileged`, `--device` or `--network` options.

With no command, the container runs its default command, `rs-enumerate-devices --compact`, which prints a short summary of the connected devices and exits.

### Run another command or a shell

`run_image.sh` takes no arguments. To run something else, use the same `docker run` options and add the command after the image name:

```sh
docker run -it --rm \
    -v /dev:/dev \
    --device-cgroup-rule "c 81:* rmw" \
    --device-cgroup-rule "c 189:* rmw" \
    librealsense rs-depth
```

`rs-depth` is the C example from [`examples/C/depth`](https://github.com/realsenseai/librealsense/tree/development/examples/C/depth), which draws the depth stream as text in the terminal. To get a shell inside the container, use `/bin/bash` as the command instead.

## What the Dockerfile does

### Build arguments

| Argument | Default | Used for |
|---|---|---|
| `BASE_IMAGE` | `ubuntu:20.04` | Base image of both stages. |
| `LIBRS_VERSION` | none (required) | Release to download and build, without the `v` prefix. |

### Stage 1: `librealsense-builder`

1. Starts `FROM $BASE_IMAGE` and sets `DEBIAN_FRONTEND=noninteractive`.
2. Installs the build dependencies with `apt-get install -qq -y --no-install-recommends`: `build-essential`, `cmake`, `git`, `libssl-dev`, `libusb-1.0-0-dev`, `pkg-config`, `libgtk-3-dev`, `libglfw3-dev`, `libgl1-mesa-dev`, `libglu1-mesa-dev`, `curl`, `python3`, `python3-dev` and `ca-certificates`.
3. In `/usr/src`, downloads the tag archive with `curl`, unpacks it, and links `/usr/src/librealsense-$LIBRS_VERSION` to `/usr/src/librealsense`.
4. Configures and builds in `/usr/src/librealsense/build`:
   ```sh
   cmake \
   -DCMAKE_C_FLAGS_RELEASE="${CMAKE_C_FLAGS_RELEASE} -s" \
   -DCMAKE_CXX_FLAGS_RELEASE="${CMAKE_CXX_FLAGS_RELEASE} -s" \
   -DCMAKE_INSTALL_PREFIX=/opt/librealsense \
   -DBUILD_GRAPHICAL_EXAMPLES=OFF \
   -DBUILD_PYTHON_BINDINGS:bool=true \
   -DCMAKE_BUILD_TYPE=Release ../ \
   && make -j$(($(nproc)-1)) all \
   && make install
   ```

What these options mean, according to [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake):

- `-s` in the release flags makes the linker strip symbols, to keep the binaries small. `${CMAKE_C_FLAGS_RELEASE}` and `${CMAKE_CXX_FLAGS_RELEASE}` are read from the shell, where they are normally empty.
- `BUILD_GRAPHICAL_EXAMPLES=OFF` (default `ON`) skips the OpenGL examples and the graphical tools, including `realsense-viewer` and the Depth Quality Tool.
- `BUILD_PYTHON_BINDINGS=true` (default `OFF`) builds `pyrealsense2`. With CMake 3.12 or newer it installs into the `Python_SITEARCH` folder of the builder's `python3` ([`CMake/external_pybind11.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/external_pybind11.cmake)). The Dockerfile copies it from `/usr/lib/python3/dist-packages/pyrealsense2`.
- All other options keep their defaults. In particular, `BUILD_EXAMPLES` and `BUILD_TOOLS` are `ON`, and `FORCE_RSUSB_BACKEND` is `OFF`, so the library uses the V4L2 backend ([`CMake/unix_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/unix_config.cmake)). See [[Build Configuration]] for all options.

The build uses one job less than the number of CPUs (`nproc`) of the build machine.

### Stage 2: `librealsense` (the final image)

The scripts build only this stage (`--target librealsense`). It:

1. Starts again `FROM ${BASE_IMAGE}`.
2. Copies from the builder stage:
   - `/opt/librealsense` to `/usr/local/`, so the tools and examples land in `/usr/local/bin` and the libraries in `/usr/local/lib`.
   - `/usr/lib/python3/dist-packages/pyrealsense2` to the same path.
   - `config/99-realsense-libusb.rules` and `config/99-realsense-d4xx-mipi-dfu.rules` to `/etc/udev/rules.d/`.
3. Sets `PYTHONPATH=$PYTHONPATH:/usr/local/lib`.
4. Installs the runtime packages `libusb-1.0-0`, `udev`, `apt-transport-https`, `ca-certificates`, `curl` and `software-properties-common`.
5. Sets `CMD [ "rs-enumerate-devices", "--compact" ]`. There is no `ENTRYPOINT`, so any command you pass to `docker run` replaces the default.

The build tools, sources and intermediate files stay in the builder stage.

## What the container provides

- The librealsense shared library, the `realsense2-gl` library (`BUILD_GLSL_EXTENSIONS` stays `ON`) and the headers under `/usr/local`.
- The command-line tools built with `BUILD_TOOLS`, such as `rs-enumerate-devices`, `rs-fw-update`, `rs-convert`, `rs-record`, `rs-terminal`, `rs-fw-logger` and `rs-embed`. See [[Command Line Tools]].
- The examples that are built without `BUILD_GRAPHICAL_EXAMPLES`, such as `rs-hello-realsense`, `rs-depth`, `rs-color`, `rs-distance`, `rs-callback` and `rs-save-to-disk`. Which examples these are depends on the `examples/*/CMakeLists.txt` files of the release you build. See [[Code Samples]].
- The `pyrealsense2` Python module, but `python3` is not listed in the final stage's packages (see below).

## Limitations

- **No graphical tools.** `realsense-viewer`, the Depth Quality Tool and the OpenGL examples are not built. The run script also does not pass a display into the container.
- **Python not listed.** The final stage does not list `python3` among its packages (only `libusb-1.0-0`, `udev`, `apt-transport-https`, `ca-certificates`, `curl` and `software-properties-common`). Whether an interpreter ends up in the image depends on what those packages pull in from the base image. If `python3` is missing, install the `python3` package of the same base image in the container, or add it to the Dockerfile. See [[Python Wrapper]].
- **Released source only.** The image is always built from a GitHub release tag, not from the `development` branch or your working tree.
- **Host kernel drivers.** The image uses the V4L2 backend, so the camera is reached through the host kernel's video and USB drivers. Set up the host as described in [[Installation Linux]].
- **Device access is limited to two device types.** The cgroup rules in `run_image.sh` only allow character devices with major numbers 81 and 189. Devices with other major numbers, such as the `iio*` devices that also appear in `99-realsense-libusb.rules`, are not covered. The repository guide lists IMU devices as not supported by the container.
- **udev inside the container.** The image installs `udev` and copies the rules files, but nothing in the image starts the udev daemon.
- **Platforms.** The scripts build for the host platform, or for `linux/amd64` and `linux/arm64` with `build_image_multi_platform.sh`.

### Prebuilt image

The repository guide mentions a prebuilt image, `docker pull librealsense/librealsense`, on Docker Hub, available only for x86. No script or workflow in the repository builds or publishes it, so its tags and contents cannot be checked against the code. To be sure what you run, build the image yourself as described above.

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`de7fa8c`](https://github.com/realsenseai/librealsense/commit/de7fa8cf3e05bac6f3ce6b5eeec2eb766c57259e)._
<!-- /wikigen:footer -->
