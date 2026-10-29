The Python wrapper exposes the librealsense C++ API to Python as the `pyrealsense2` module. It is built with [pybind11](https://github.com/pybind/pybind11) from the sources in [`wrappers/python`](https://github.com/realsenseai/librealsense/tree/development/wrappers/python), and is the quickest way to stream from a RealSense camera into NumPy, OpenCV or other Python tools. This page covers installing the package, building it from source, a first program, the bundled examples and the module layout.

## Installing from PyPI

The package name is `pyrealsense2` (set in [`wrappers/python/pyproject.toml`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pyproject.toml)):

```
pip install pyrealsense2
```

Check the installed version:

```python
import pyrealsense2 as rs
print(rs.__version__)       # librealsense API version, e.g. "2.x.y"
print(rs.__full_version__)  # full version string
```

`__version__` and `__full_version__` are set from `RS2_API_VERSION_STR` and `RS2_API_FULL_VERSION_STR` in [`pyrealsense2.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pyrealsense2.cpp), and re-exported by the package's `__init__.py`.

### Supported Python versions

`pyproject.toml` declares `requires-python = ">=3.9"` and lists the classifiers Python 3.10, 3.11, 3.12, 3.13 and 3.14.

## Building from source

Build the wrapper as part of the main librealsense CMake build by turning on `BUILD_PYTHON_BINDINGS` (default `OFF`, defined in [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake)). For the full list of build options see [[Build Configuration]].

### CMake options

| Option / variable | Default | Effect |
|---|---|---|
| `BUILD_PYTHON_BINDINGS` | `OFF` | Builds the `pyrealsense2` module (`wrappers/CMakeLists.txt` adds `wrappers/python` when this or `BUILD_PYTHON_DOCS` is on). |
| `BUILD_PYTHON_DOCS` | `OFF` | Builds the Sphinx documentation for the bindings (`wrappers/python/docs`, target `pyrealsense2_docs`). |
| `BUILD_LEGACY_PYBACKEND` | `OFF` | Also builds the deprecated `pybackend2` module (direct backend access). |
| `PYTHON_EXECUTABLE` | not set | Passed to the pybind11 sub-build (`-DPYTHON_EXECUTABLE=...` in [`CMake/external_pybind11.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/external_pybind11.cmake)). Use it to pick the Python interpreter when several are installed. |
| `PYTHON_INSTALL_DIR` | `${Python_SITEARCH}/pyrealsense2` (CMake 3.12 and later); `${CMAKE_INSTALL_LIBDIR}/pythonX.Y/pyrealsense2` with older CMake | Cache path where `make install` places the module and `__init__.py`. |
| `BUILD_SHARED_LIBS` | `ON` | With `OFF`, librealsense is built as a static library and linked into the module. |

When `BUILD_PYTHON_BINDINGS` is on, the configure step downloads pybind11 (`v2.13.6`, see [`CMake/pybind11-download.cmake.in`](https://github.com/realsenseai/librealsense/blob/development/CMake/pybind11-download.cmake.in)) and pybind11_json, so it needs network access. On CMake 3.12 and later it also runs `find_package(Python REQUIRED COMPONENTS Interpreter Development)`, so the Python development headers must be installed (on Ubuntu, `python3-dev`).

### Linux / macOS

```
git clone https://github.com/realsenseai/librealsense.git
cd librealsense
mkdir build && cd build
cmake .. -DBUILD_PYTHON_BINDINGS=ON -DPYTHON_EXECUTABLE=$(which python3)
make -j4
sudo make install
```

`make install` copies the `pyrealsense2` binary module and `pyrealsense2/__init__.py` into `PYTHON_INSTALL_DIR`, and installs a CMake package config (`pyrealsense2Config.cmake`) under `${CMAKE_INSTALL_LIBDIR}/cmake/pyrealsense2`. If that directory is not on Python's search path, set `PYTHONPATH` to it or set `PYTHON_INSTALL_DIR` explicitly at configure time.

### Windows

Configure with `BUILD_PYTHON_BINDINGS` enabled (in `cmake-gui` or on the command line, adding `-DPYTHON_EXECUTABLE=<path to python.exe>` if you have several Python installations), open the generated `realsense2.sln` and build the `pyrealsense2` project. The output is `pyrealsense2*.pyd`, which needs `realsense2.dll` next to it at runtime.

### Building a wheel

[`wrappers/python/build_wheel.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/build_wheel.py) packages the module from an existing CMake build directory:

```
cd wrappers/python
python build_wheel.py --build-dir ../../build            # Linux/macOS
python build_wheel.py --build-dir ../../build/Release    # Windows multi-config build
```

The script:

1. generates `pyrealsense2/_version.py` from `include/librealsense2/rs.h` (via [`find_librs_version.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/find_librs_version.py));
2. copies the extension (`pyrealsense2*.pyd` on Windows, `pyrealsense2*.so` elsewhere) and the librealsense shared library (`realsense2.dll`, `librealsense2*.dylib` or `librealsense2.so*`) from the build directory into `pyrealsense2/`;
3. installs `build` and `hatchling` if `python -m build --version` fails, and runs `python -m build --wheel`, producing `dist/pyrealsense2-*.whl`.

`--extra-lib GLOB` (repeatable) bundles further runtime libraries, for example `--extra-lib "fastdds*.dll" --extra-lib "fastcdr*.dll" --extra-lib "foonathan_memory*.dll"` for a Windows build with DDS. Install the result with `pip install dist/pyrealsense2-*.whl`.

The wheel is built by the `hatchling` backend; the version is read from `pyrealsense2/_version.py`, and the platform and Python tag are inferred from the build environment ([`hatch_build.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/hatch_build.py)).

## First program

This program streams depth and prints the distance at the image centre. It uses only calls bound in [`pyrs_pipeline.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pyrs_pipeline.cpp) and [`pyrs_frame.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pyrs_frame.cpp):

```python
import pyrealsense2 as rs

pipeline = rs.pipeline()
config = rs.config()
config.enable_stream(rs.stream.depth)   # other parameters are resolved internally
pipeline.start(config)

try:
    for _ in range(30):
        frames = pipeline.wait_for_frames()
        depth = frames.get_depth_frame()
        if not depth:
            continue
        x, y = depth.get_width() // 2, depth.get_height() // 2
        print("Distance at centre: {:.3f} m".format(depth.get_distance(x, y)))
finally:
    pipeline.stop()
```

- `pipeline.start()` has overloads with no arguments, with a `config`, with a callback, with a `frame_queue`, with a `config` and a callback, and with a `config` and a `frame_queue`.
- `frameset.get_depth_frame()` returns an empty frame if the set has no depth frame; frames support truth testing (`if not depth`).
- `depth_frame.get_distance(x, y)` returns the depth in meters at the given pixel.

The full version of this tutorial is [`python-tutorial-1-depth.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/python-tutorial-1-depth.py). For more task-oriented snippets see [[API How To]]; for the C++ samples see [[Code Samples]].

## Examples

All examples are in [`wrappers/python/examples`](https://github.com/realsenseai/librealsense/tree/development/wrappers/python/examples). Some need extra packages such as NumPy, OpenCV (`cv2`) or pyglet.

| Example | What it shows |
|---|---|
| [`python-tutorial-1-depth.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/python-tutorial-1-depth.py) | Stream depth and print it to the console as ASCII art. |
| [`opencv_viewer_example.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/opencv_viewer_example.py) | Render depth and color with NumPy and OpenCV. |
| [`align-depth2color.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/align-depth2color.py) | Align depth to color and remove the background. |
| [`align-with-software-device.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/align-with-software-device.py) | Capture depth and color, then align them using a software device. |
| [`numpy_to_frame.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/numpy_to_frame.py) | Turn a NumPy array into a frame using a software device. |
| [`export_ply_example.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/export_ply_example.py) | Compute a point cloud and export it to PLY. |
| [`opencv_pointcloud_viewer.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/opencv_pointcloud_viewer.py) | Point cloud software renderer with OpenCV and NumPy. |
| [`pyglet_pointcloud_viewer.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/pyglet_pointcloud_viewer.py) | OpenGL point cloud viewer with pyglet. |
| [`read_bag_example.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/read_bag_example.py) | Read a recorded `.bag` file and colorize the depth stream. |
| [`frame_queue_example.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/frame_queue_example.py) | Use frame queues to decouple slow processing from streaming. |
| [`embedded_filters.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/embedded_filters.py) | List the depth sensor's embedded filters and their options. |
| [`object_detection_example.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/object_detection_example.py) | Stream color and object detection and draw labelled boxes, on devices that support it. |
| [`python-rs400-advanced-mode-example.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/python-rs400-advanced-mode-example.py) | Use the D400 advanced mode interface. |
| [`depth_auto_calibration_example.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/depth_auto_calibration_example.py) | D400 self-calibration flow (on-chip, focal-length and tare calibration). |
| [`depth_ucal_example.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/depth_ucal_example.py) | Auto-calibration APIs for the OCC and Tare flows. |
| [`d500_triggered_calibration.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/d500_triggered_calibration.py) | Triggered calibration API, comparing depth before and after. |
| [`d5x5_interactive_triggered_calibration.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/d5x5_interactive_triggered_calibration.py) | Interactive triggered calibration (run, optional try new/old, then commit or cancel) for cameras whose name contains D535 or D585, excluding D585S. |
| [`d5x5_get_depth_mapping_params.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/d5x5_get_depth_mapping_params.py) *(new in the next release)* | Read the occupancy grid and labeled point cloud parameters of a D5x5 camera. |
| [`d5x5_set_depth_mapping_params.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/d5x5_set_depth_mapping_params.py) *(new in the next release)* | Write those parameters from a JSON file (default [`d5x5_depth_mapping_params.json`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/d5x5_depth_mapping_params.json)). |
| [`box_dimensioner_multicam/box_dimensioner_multicam_demo.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/box_dimensioner_multicam/box_dimensioner_multicam_demo.py) | Measure a box's length, width and height with multiple cameras, calibrated with a chessboard. |
| [`pybackend_example_1_general.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/pybackend_example_1_general.py) | Control a device through the legacy `pybackend2` module (needs `BUILD_LEGACY_PYBACKEND`). |

## Module layout

- **Package `pyrealsense2`.** [`pyrealsense2/__init__.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pyrealsense2/__init__.py) runs `from .pyrealsense2 import *`, so the compiled extension (`pyrealsense2*.so` / `.pyd`) sits inside the package folder and all its names are available as `pyrealsense2.<name>`. The usual import is `import pyrealsense2 as rs`.
- **Binding sources.** The extension is built from [`pyrealsense2.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pyrealsense2.cpp), which calls one `init_*` function per area:

  | File | Area |
  |---|---|
  | `c_files.cpp` | Enums from the C API (`rs.stream`, `rs.format`, `rs.option`, ...) |
  | `pyrs_types.cpp` | Basic types |
  | `pyrs_frame.cpp` | Frames: `frame`, `video_frame`, `depth_frame`, `frameset`, ... |
  | `pyrs_options.cpp` | Options interface |
  | `pyrs_processing.cpp` | Processing blocks and filters, `frame_queue` |
  | `pyrs_sensor.cpp` | Sensors |
  | `pyrs_device.cpp` | Devices |
  | `pyrs_record_playback.cpp` | Recorder and playback |
  | `pyrs_context.cpp` | `context` and device enumeration |
  | `pyrs_pipeline.cpp` | `config`, `pipeline`, `pipeline_profile` |
  | `pyrs_internal.cpp` | Software device and other internal APIs |
  | `pyrs_export.cpp` | Export (`save_single_frameset`) |
  | `pyrs_advanced_mode.cpp` | D400 advanced mode, serializable device |
  | `pyrsutil.cpp` | Utility functions (projection) |
  | `pyrs_eth_config.cpp` | Ethernet device configuration (`eth_config_device`) |
  | `pyrs_rum.cpp` | Usage statistics (`rs.rum` submodule, see [[Usage Statistics RUM]]) |

  `pyrealsense2.cpp` itself also binds `save_to_ply` and the logging functions (`log_to_console`, `log_to_file`, `reset_logger`, `enable_rolling_log_file`, ...).
- **Naming.** Class and method names follow the C++ API in `include/librealsense2/hpp` (`rs2::pipeline` becomes `rs.pipeline`, `rs2::frameset::get_depth_frame` becomes `get_depth_frame`). Enum values are exposed under the enum type, for example `rs.stream.depth` and `rs.format.z16`.
- **`pybackend2`.** A separate, deprecated module built only with `BUILD_LEGACY_PYBACKEND`, from [`pybackend.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pybackend.cpp). It gives direct access to the platform backend; `build_wheel.py` does not package it.

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`46cf1a6`](https://github.com/realsenseai/librealsense/commit/46cf1a67cc441ad9f55ce339a0de30f2a1c6fed0)._
<!-- /wikigen:footer -->
