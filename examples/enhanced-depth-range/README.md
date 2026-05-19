# librealsense2-enhanced-depth (Close-Range)

These examples demonstrate usage of the **Enhanced depth** library. The library
is a closed-source depth enhancement module designed to operate on top of the
Intel RealSense SDK 2.0 pipeline. It improves minimum sensing distance (min-Z)
to ~12 cm on NVIDIA Jetson platforms. It integrates transparently into
existing RealSense-based applications and requires no modification to camera
firmware and/or SDK.

<p align="center">
  <a href="https://realsenseai.com/case-studies/?capability_application=autonomous-mobile-robots"><img src="https://librealsense.realsenseai.com/readme-media/minz/minz_600.gif" width="720"/></a>
</p>

---

## Table of Contents

- [What's Included](#whats-included)
- [Installation](#installation)
- [Running the Bundled Examples](#running-the-bundled-examples)
  - [Python (headless stats): `range_depth.py`](#python-headless-stats-range_depthpy)
  - [Python (live windowed compare): `live_minz_compare.py`](#python-live-windowed-compare-live_minz_comparepy)
  - [C++: `range_depth.cpp`](#c-range_depthcpp)
- [Quick Start](#quick-start)
  - [Python](#quick-start-python)
  - [C++](#quick-start-c)
- [Python API](#python-api)
  - [Calibration](#calibration)
  - [DepthRangeImprover](#depthrangeimprover)
  - [Utilities](#utilities)
- [C++ API](#c-api)
  - [Calibration (C++)](#calibration-c)
  - [DepthRangeImprover (C++)](#depthrangeimprover-c)
  - [Utilities (C++)](#utilities-c)
- [FrameMetadata](#framemetadata)
- [Troubleshooting](#troubleshooting)

---

## What's Included

Latency measured at 640x480 on Jetson AGX Orin.

| Component | Class | What it does |
|-----------|-------|-------------|
| Close-range improvement | `DepthRangeImprover` | Extends min distance from 520 mm to 120 mm |

---

## Installation

```bash
sudo dpkg -i librealsense2-enhanced-depth-*.deb
```

Everything installs to `/opt/librealsense2-enhanced-depth/`. No venv needed — uses system Python 3.

The deb depends on `librealsense2` (≥ matching version) — install both from the
same Artifactory / apt source so the SONAMEs line up.

---

## Running the Bundled Examples

This folder ships three minimal end-to-end demos that pull live IR + depth
from a connected RealSense D4xx, push the frames through `DepthRangeImprover`,
and either print stats or display the result.

| Example | What it does | Extra deps |
|---|---|---|
| [`range_depth.py`](#python-headless-stats-range_depthpy) | Headless Python stats: prints valid-pixel counts + recovered close-range pixels per frame | none beyond the SDK |
| [`live_minz_compare.py`](#python-live-windowed-compare-live_minz_comparepy) | Two live windows side-by-side: raw HW depth vs MinZ-improved depth | OpenCV (`python3-opencv`) |
| [`range_depth.cpp`](#c-range_depthcpp) | C++ equivalent of `range_depth.py` (headless stats) | librealsense2-dev |

### Python (headless stats): `range_depth.py`

Once the deb is installed, run directly with system Python:

```bash
python3 range_depth.py
```

(Adjust resolution / frame count by editing the `enable_stream` calls in the
file — the example is intentionally short and self-contained.)

The Python module path `/opt/librealsense2-enhanced-depth/python/` is added to
`sys.path` automatically by the package's `postinst`. If you're running from
a non-standard install or a build tree, set:

```bash
export PYTHONPATH=/opt/librealsense2-enhanced-depth/python:$PYTHONPATH
python3 range_depth.py
```

### Python (live windowed compare): `live_minz_compare.py`

Visual side-by-side: opens two OpenCV windows showing **raw HW depth** (left,
black-out below ~520 mm) and **MinZ-improved depth** (right, close-range
filled in via the Enhanced Depth library). Useful for eyeballing what MinZ recovers.

```bash
sudo apt install python3-opencv      # one-time, if not already installed
python3 live_minz_compare.py         # press 'q' or ESC in either window to stop
```

The script doesn't pop up empty windows during camera warm-up — it prints
`Waiting for first frame…` to the terminal and only opens the windows once
real frames arrive. Status line below the live windows shows per-frame valid
pixel counts and the gain.

If you're running from a non-standard install or build tree, the same
`PYTHONPATH` override applies:

```bash
export PYTHONPATH=/opt/librealsense2-enhanced-depth/python:$PYTHONPATH
python3 live_minz_compare.py
```

### C++: `range_depth.cpp`

The example links against two libraries:

| Library | From package | Purpose |
|---|---|---|
| `librealsense2` | `librealsense2-dev` | camera capture (the same SDK this folder lives in) |
| `librs_depth_range` | `librealsense2-enhanced-depth` | the close-range processor |

Both expose `pkg-config` files, so the simplest one-line build is:

```bash
g++ -std=c++17 range_depth.cpp \
    -I/opt/librealsense2-enhanced-depth/include \
    -L/opt/librealsense2-enhanced-depth/lib \
    -lrs_depth_range \
    -Wl,-rpath,/opt/librealsense2-enhanced-depth/lib \
    $(pkg-config --cflags --libs realsense2) \
    -o range_depth

./range_depth
```

If you'd rather build it as part of a CMake project, drop this snippet into a
`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.10)
project(range_depth)

find_package(realsense2 REQUIRED)

add_executable(range_depth range_depth.cpp)
set_property(TARGET range_depth PROPERTY CXX_STANDARD 17)

target_include_directories(range_depth PRIVATE
    /opt/librealsense2-enhanced-depth/include
)
target_link_directories(range_depth PRIVATE
    /opt/librealsense2-enhanced-depth/lib
)
target_link_libraries(range_depth PRIVATE
    realsense2::realsense2
    rs_depth_range
)
set_target_properties(range_depth PROPERTIES
    INSTALL_RPATH /opt/librealsense2-enhanced-depth/lib
    BUILD_RPATH   /opt/librealsense2-enhanced-depth/lib
)
```

Then standard CMake build:

```bash
mkdir build && cd build
cmake ..
make
./range_depth
```

> **Tip:** the linker hint `-Wl,-rpath,…` (or CMake's `INSTALL_RPATH`) bakes
> the install path into the binary so the dynamic loader finds
> `librs_depth_range.so` without you having to set `LD_LIBRARY_PATH` at run time.

---

## Quick Start

### Quick Start: Python

```python
import pyrealsense2 as rs
import numpy as np
from rs_depth import Calibration, DepthRangeImprover, FrameMetadata

pipeline = rs.pipeline()
cfg = rs.config()
cfg.enable_stream(rs.stream.infrared, 1, 640, 480, rs.format.y8, 30)
cfg.enable_stream(rs.stream.infrared, 2, 640, 480, rs.format.y8, 30)
cfg.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)
profile = pipeline.start(cfg)

ir_stream = profile.get_stream(rs.stream.infrared, 1).as_video_stream_profile()
ir_intrin = ir_stream.get_intrinsics()
ir_extrin = ir_stream.get_extrinsics_to(profile.get_stream(rs.stream.infrared, 2))
cal = Calibration.from_sdk(ir_intrin, ir_extrin)

depth_range = DepthRangeImprover(cal)

frameset = pipeline.wait_for_frames()
ir_left  = np.asanyarray(frameset.get_infrared_frame(1).get_data())
ir_right = np.asanyarray(frameset.get_infrared_frame(2).get_data())
depth_mm = np.asanyarray(frameset.get_depth_frame().get_data())
meta     = FrameMetadata.from_rs2_frameset(frameset)

# Close-range improvement → uint16 mm, shape (H, W)
improved = depth_range.process(ir_left, ir_right, depth_mm, metadata=meta)

pipeline.stop()
```

---

### Quick Start: C++

```cpp
#include <rs_depth_calibration.hpp>
#include <rs_depth_range.hpp>
#include <librealsense2/rs.hpp>

rs2::pipeline pipeline;
rs2::config cfg;
cfg.enable_stream(RS2_STREAM_INFRARED, 1, 640, 480, RS2_FORMAT_Y8, 30);
cfg.enable_stream(RS2_STREAM_INFRARED, 2, 640, 480, RS2_FORMAT_Y8, 30);
cfg.enable_stream(RS2_STREAM_DEPTH,       640, 480, RS2_FORMAT_Z16, 30);
auto profile = pipeline.start(cfg);

auto ir_stream = profile.get_stream(RS2_STREAM_INFRARED, 1).as<rs2::video_stream_profile>();
auto intrin = ir_stream.get_intrinsics();
auto extrin = ir_stream.get_extrinsics_to(profile.get_stream(RS2_STREAM_INFRARED, 2));
auto cal = rs_depth::Calibration::from_params(intrin.fx, std::abs(extrin.translation[0]));

rs_depth::DepthRangeImprover depth_range(cal);

auto frames = pipeline.wait_for_frames();
auto ir_l = frames.get_infrared_frame(1);
auto ir_r = frames.get_infrared_frame(2);
auto depth = frames.get_depth_frame();

// Mirrors Python's FrameMetadata.from_rs2_frameset.
auto meta = FrameMetadata::from_rs2_frameset(frames);

std::vector<uint16_t> depth_out(640 * 480);
depth_range.process((const uint8_t*)ir_l.get_data(),
                    (const uint8_t*)ir_r.get_data(),
                    (const uint16_t*)depth.get_data(),
                    depth_out.data(), meta);

pipeline.stop();
```

---

## Python API

### Calibration

Full D4xx camera calibration. Construct once, pass everywhere.

```python
from rs_depth import Calibration
```

| Constructor | When to use |
|-------------|-------------|
| `Calibration.from_sdk(ir_intrinsics, ir_extrinsics, rgb_intrinsics=..., depth_to_rgb=...)` | `rs2_intrinsics` / `rs2_extrinsics` objects |
| `Calibration.from_params(focal_length_px, baseline_m)` | Plain scalars (minimal fallback) |

**IR / Depth fields** (GETINTCAL ID 25 / RECPARAMSGET):

| Field | Type | Description |
|-------|------|-------------|
| `focal_length_px` | `float` | fx at the active streaming resolution |
| `baseline_m` | `float` | Stereo baseline in metres |
| `fy`, `ppx`, `ppy` | `float` | Depth stream intrinsics |
| `width`, `height` | `int` | Resolution these params apply to |
| `ir_distortion_model` | `int` | `rs2_distortion` enum value |
| `ir_distortion_coeffs` | `tuple[float, ...]` | Brown model (k1, k2, p1, p2, k3) |

**RGB fields** (GETINTCAL ID 32):

| Field | Type | Description |
|-------|------|-------------|
| `rgb_fx`, `rgb_fy` | `float` | RGB focal lengths in pixels |
| `rgb_ppx`, `rgb_ppy` | `float` | RGB principal point |
| `rgb_width`, `rgb_height` | `int` | RGB stream resolution |
| `rgb_distortion_model` | `int` | `rs2_distortion` enum value |
| `rgb_distortion_coeffs` | `tuple[float, ...]` | Brown model (k1, k2, p1, p2, k3) |
| `depth_to_rgb_rotation` | `tuple[float, ...]` | 9-float column-major 3×3 rotation |
| `depth_to_rgb_translation` | `tuple[float, ...]` | 3-float translation in metres |

---

### DepthRangeImprover

Extends RealSense minimum working distance from ~520 mm to ~120 mm.
Recovers close-range depth that the RealSense hardware cannot measure and blends it with the hardware depth for a seamless result.

```python
from rs_depth import DepthRangeImprover

DepthRangeImprover(
    calibration,                      # Calibration — required
    min_z_threshold_mm=None,          # Auto from calibration (F*B/105); pass int to override
    scale_factor=0.35,                # Downscale factor for processing (speed vs quality)
    crop_region=None,                 # (x1, y1, x2, y2) — process only this ROI;
                                      # output is always full-size (hw depth fills the background)
)
```

#### `DepthRangeImprover.process(ir_left, ir_right, depth_mm)`

```python
depth_mm = improver.process(ir_left, ir_right, depth_mm, metadata=meta)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ir_left` | `np.ndarray (H, W) uint8` | Left IR image. |
| `ir_right` | `np.ndarray (H, W) uint8` | Right IR image. |
| `depth_mm` | `np.ndarray (H, W) uint16` | RealSense hardware depth in mm. |
| `metadata` | `FrameMetadata` | **Required.** Per-frame metadata. |
| **Returns** | `np.ndarray (H, W) uint16` | Merged depth in **millimetres**. |

---

### Utilities

```python
from rs_depth import convert_depth, split_y8i, discover_backends

# Unit conversion
depth_m  = convert_depth(depth_mm, "mm", "meters")
depth_mm = convert_depth(depth_m, "meters", "mm")

# Y8I interleaved frame → separate left/right (zero-copy views)
left, right = split_y8i(y8i_frame)  # (H, W, 2) → two (H, W) uint8
depth_mm = depth_range.process(left, right, hw_depth_mm)

# Check what's installed at runtime
status = discover_backends()
print(status.has_depth_range)  # True
```

---

## C++ API

Headers at `/opt/librealsense2-enhanced-depth/include/`, library at `/opt/librealsense2-enhanced-depth/lib/`.

---

### Calibration (C++)

```cpp
#include <rs_depth_calibration.hpp>
```

| Constructor | When to use |
|-------------|-------------|
| `Calibration::from_sdk(ir_intrin, ir_extrin, rgb_intrin*, depth_to_rgb*)` | `rs2_intrinsics` / `rs2_extrinsics` from librealsense2. Available when `<librealsense2/rs.hpp>` is included before this header. |
| `Calibration::from_raw_tables(data25, size25, data32, size32, rec, rec_sz, w, h)` | Raw firmware blobs |
| `Calibration::from_params(focal_length_px, baseline_m)` | Plain scalars (minimal fallback) |

**IR / Depth fields** (GETINTCAL ID 25 / RECPARAMSGET):

| Field | Type | Description |
|-------|------|-------------|
| `focal_length_px` | `float` | fx at the active streaming resolution |
| `baseline_m` | `float` | Stereo baseline in metres |
| `fy`, `ppx`, `ppy` | `float` | Depth stream intrinsics (0 if not set) |
| `width`, `height` | `int` | Resolution these params apply to |
| `ir_distortion_model` | `int` | `rs2_distortion` enum value |
| `ir_distortion_coeffs[5]` | `float[]` | Brown model (k1, k2, p1, p2, k3) |

**RGB fields** (GETINTCAL ID 32):

| Field | Type | Description |
|-------|------|-------------|
| `rgb_fx`, `rgb_fy` | `float` | RGB focal lengths in pixels |
| `rgb_ppx`, `rgb_ppy` | `float` | RGB principal point |
| `rgb_width`, `rgb_height` | `int` | RGB stream resolution |
| `rgb_distortion_model` | `int` | `rs2_distortion` enum value |
| `rgb_distortion_coeffs[5]` | `float[]` | Brown model (k1, k2, p1, p2, k3) |
| `depth_to_rgb_rotation[9]` | `float[]` | Column-major 3×3 rotation |
| `depth_to_rgb_translation[3]` | `float[]` | Translation in metres |

---

### DepthRangeImprover (C++)

Extends RealSense minimum working distance from ~520 mm to ~120 mm.
Pure C++ — no Python runtime required.

```cpp
#include <rs_depth_calibration.hpp>
#include <rs_depth_range.hpp>
```

```cpp
rs_depth::DepthRangeImprover(
    calibration,                    // Calibration — required
    crop_region     = {-1,-1,-1,-1},// {x1, y1, x2, y2} — process only this ROI;
                                    // output is always full-size (hw depth fills the background)
    min_z_threshold = 0,            // 0 = auto from calibration (F*B/105); pass mm to override
    scale_factor    = 0.35f         // Downscale factor for processing (speed vs quality)
);
```

#### `DepthRangeImprover::process(ir_left, ir_right, depth_mm, depth_out, metadata)`

```cpp
depth_range.process(ir_left, ir_right, depth_mm, depth_out, metadata);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ir_left` | `const uint8_t* (H×W)` | Left IR image — grayscale, row-major |
| `ir_right` | `const uint8_t* (H×W)` | Right IR image |
| `depth_mm` | `const uint16_t* (H×W)` | RealSense hardware depth in mm |
| `depth_out` | `uint16_t* (H×W)` | Caller-allocated output — improved depth in **millimetres** |
| `metadata` | `const FrameMetadata&` | **Required.** Per-frame metadata (carries `width`, `height`, exposure, gain, etc.). |

```sh
g++ -std=c++17 your_app.cpp \
    -I/opt/librealsense2-enhanced-depth/include \
    -L/opt/librealsense2-enhanced-depth/lib -lrs_depth_range \
    -Wl,-rpath,/opt/librealsense2-enhanced-depth/lib \
    -o your_app
```

---

### Utilities (C++)

```cpp
#include <rs_depth_utils.hpp>
```

```cpp
// Unit conversion
std::vector<float>    depth_m(n);
std::vector<uint16_t> depth_mm(n);

rs_depth::convert_depth(depth_mm.data(), depth_m.data(),  n);  // mm  → metres
rs_depth::convert_depth(depth_m.data(),  depth_mm.data(), n);  // metres → mm

// Y8I split — mirrors Python: split_y8i()
std::vector<uint8_t> left(w * h), right(w * h);
rs_depth::split_y8i(y8i_data, left.data(), right.data(), w, h);
depth_range.process(left.data(), right.data(), hw_depth, depth_out, metadata);

// Backend discovery
auto status = rs_depth::discover_backends();
status.has_depth_range;  // true
```

---

## FrameMetadata

Per-frame metadata from the RealSense sensor. Contains per-stream metadata
(IR, depth, color) plus frame-level format info. Constructed from native
`pyrealsense2` frames or manually.

```python
from rs_depth import FrameMetadata, StreamMetadata

# From native pyrealsense2
frameset = pipeline.wait_for_frames()
meta = FrameMetadata.from_rs2_frameset(frameset)

# Manual construction
meta = FrameMetadata(width=640, height=480, ir=StreamMetadata(exposure_us=8000))
```

#### StreamMetadata fields

| Field | Type | Description |
|-------|------|-------------|
| `frame_number` | `int` | Frame counter |
| `timestamp_us` | `float` | Device clock (microseconds) |
| `arrival_time_us` | `float` | System clock (microseconds) |
| `sensor_timestamp_us` | `float` | Mid-exposure timestamp |
| `backend_timestamp_us` | `float` | Backend receive timestamp |
| `exposure_us` | `int` | Actual exposure (microseconds) |
| `gain` | `int` | Sensor gain |
| `auto_exposure` | `bool` | Auto-exposure on/off |
| `actual_fps` | `float` | Measured FPS × 1000 |
| `laser_power` | `int` | Laser power (0–360) |
| `emitter_mode` | `int` | 0=off, 1=laser, 2=auto |
| `temperature` | `float` | Sensor temperature (Celsius) |
| `white_balance` | `int` | White balance (Kelvin) |
| `brightness` | `int` | Brightness level |
| `contrast` | `int` | Contrast level |
| `saturation` | `int` | Saturation level |

#### FrameMetadata fields

| Field | Type | Description |
|-------|------|-------------|
| `ir` | `StreamMetadata` | Metadata from the IR frame |
| `depth` | `StreamMetadata` | Metadata from the depth frame |
| `color` | `StreamMetadata` | Metadata from the color frame |
| `ir_format` | `str` | IR format: `"y8"`, `"y16"`, or `"y12i"` |
| `width` | `int` | Frame width |
| `height` | `int` | Frame height |

Convenience properties on `FrameMetadata` (read IR first, fall back to depth):
`frame_number`, `timestamp_us`, `exposure_us`, `gain`, `laser_power`,
`temperature`, `actual_fps`.

> **Note:** Not all fields are available on all devices. Unsupported fields
> default to 0. Full per-frame metadata requires the librealsense UVC kernel
> patch.

---

## Troubleshooting

### Camera not detected

```
ERROR: Failed to start RealSense camera
```

Check that the camera is connected and not in use by another process:
```bash
rs-enumerate-devices    # from librealsense2-utils
```

### No display (headless / SSH)

Use `range_depth.py` or `range_depth.cpp` — both are headless and print per-frame
stats to stdout. `live_minz_compare.py` requires a display.
