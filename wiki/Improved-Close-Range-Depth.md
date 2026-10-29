Improved Close Range Depth fills in depth for objects that are closer to the camera than the normal stereo minimum range. librealsense reaches it in two ways: an on-camera control on D500-series depth sensors, and a host-side library (`librealsense2-enhanced-depth`, installed separately) used by the bundled examples and an optional RealSense Viewer filter. This page is for developers who want to turn the feature on, tune the on-camera control from code, or try the host-side examples.

## Two ways to get close-range depth

| | On-camera control | Host-side library |
|---|---|---|
| Where it runs | In the camera firmware (an embedded filter on the depth sensor) | On the host, on the IR left, IR right and depth frames |
| Exposed as | Composite option `RS2_COMPOSITE_OPTION_HDRD_CONTROL` with the `rs2_hdrd_control` struct ([`rs_hdrd_control.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_hdrd_control.h)) | Separate package `librealsense2-enhanced-depth`, loaded from `/opt/librealsense2-enhanced-depth` |
| Used by | Your application through the C, C++ or Python API; the RealSense Viewer embedded-filter editor | [`examples/enhanced-depth-range`](https://github.com/realsenseai/librealsense/tree/development/examples/enhanced-depth-range); the RealSense Viewer when built with `BUILD_WITH_CLOSE_RANGE_DEPTH` |
| Devices | D500-series devices that register the control (see [Devices with the on-camera control](#devices-with-the-on-camera-control)) | Depth sensors that stream IR left, IR right and depth at the same resolution (the examples request 1280x720); the Viewer filter is further limited to non-color sensors that support `RS2_OPTION_STEREO_BASELINE`, a CUDA device detected at run time, and VGA (640x480) or higher |

## Enable the on-camera control

The control is not an option on the depth sensor itself. It lives on one of the sensor's **embedded filters**, which has its own options container. Find the embedded filter that supports `RS2_COMPOSITE_OPTION_HDRD_CONTROL`, read the current value, change the fields you need and write the whole struct back. Every read and write moves all fields together in one transaction.

### C++

```cpp
#include <librealsense2/rs.hpp>

rs2::context ctx;
auto dev   = ctx.query_devices().front();
auto depth = dev.first< rs2::depth_sensor >();

for( auto && ef : depth.query_embedded_filters() )
{
    if( ! ef.supports_composite_option( RS2_COMPOSITE_OPTION_HDRD_CONTROL ) )
        continue;

    // Read-modify-write: keep the header exactly as the device returned it.
    auto v = ef.get_composite_option_as< rs2_hdrd_control >( RS2_COMPOSITE_OPTION_HDRD_CONTROL );
    v.enable      = 1;
    v.reserved[0] = 0;  // must be zero on SET
    ef.set_composite_option_from( RS2_COMPOSITE_OPTION_HDRD_CONTROL, v );
}
```

`get_composite_option_as<T>()` and `set_composite_option_from()` live in [`rs_options.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_options.hpp). The get helpers check that the payload size matches `sizeof(T)`. Both get and set check that `header.version` equals `DPP_HEADER_CURRENT_VERSION` (1). If not, they throw. A struct whose header was left zero-initialized is therefore rejected, so always start from a value read from the device.

To read the supported limits, use the range struct:

```cpp
auto r = ef.get_composite_option_range_as< rs2_hdrd_control_range >( RS2_COMPOSITE_OPTION_HDRD_CONTROL );
// r.min, r.max, r.step and r.def are each an rs2_hdrd_control
```

### Python

The Python wrapper binds the struct as `rs.hdrd_control` and adds typed helpers on every options container, embedded filters included:

```python
import pyrealsense2 as rs

ctx = rs.context()
dev = ctx.query_devices()[0]
depth = dev.first_depth_sensor()
opt = rs.composite_option_id.hdrd_control

for ef in depth.query_embedded_filters():
    if opt in ef.get_supported_composite_options():
        cfg = ef.get_hdrd_control(opt)      # rs.hdrd_control
        cfg.enable = 1
        ef.set_hdrd_control(opt, cfg)
        rng = ef.get_hdrd_control_range(opt)  # .min / .max / .step / .default
```

You can also get the filter directly with `depth.get_embedded_filter(rs.embedded_filter_type.improved_close_range_depth)`. The raw-bytes calls `get_composite_option()`, `set_composite_option()` and `get_composite_option_range()` are available as well. `rs.hdrd_control` does not expose the `reserved` field. To control the exact bytes sent, use `set_composite_option()` with a 38-byte buffer. The live test [`pytest-hdrd-control.py`](https://github.com/realsenseai/librealsense/blob/development/unit-tests/live/options/pytest-hdrd-control.py) shows both styles. It also zeroes the last four bytes (`reserved`) before restoring a raw value.

### C

The C API is in [`rs_composite_option.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_composite_option.h). These functions take the embedded filter as an `rs2_options*`:

- `rs2_set_composite_option(options, RS2_COMPOSITE_OPTION_HDRD_CONTROL, &value, sizeof(rs2_hdrd_control), &e)`
- `rs2_get_composite_option(...)` and `rs2_get_composite_option_range(...)`. Each returns an `rs2_raw_data_buffer`. Read it with `rs2_get_raw_data_size` / `rs2_get_raw_data`, cast it to `rs2_hdrd_control` / `rs2_hdrd_control_range`, and free it with `rs2_delete_raw_data`.
- `rs2_supports_composite_option`, `rs2_get_composite_options_list`, `rs2_is_composite_option_read_only` and `rs2_get_composite_option_description`.

### Choosing the field values

- `enable`: 0 = off, 1 = on.
- `filter_type` decides which other fields matter:
  - **Downscale** (0): set `downscale_ratio` (1 = x2, 2 = x4).
  - **Lookup shift** (1): set `shift_mode` (0 = 126 px, 1 = 64 px, 2 = manual). For manual, also set `shift_pixels` (0–256).
- `threshold_mode` works on its own, separate from `filter_type`:
  - 0 = Zero range.
  - 1 = MinZ range. The firmware computes the threshold and does not expose it to the host.
  - 2 = Manual. Set `threshold_mm` (0–65535).

The Viewer's tooltips describe the threshold modes this way. "Zero range" fills only originally-empty depth pixels. "MinZ (computed)" lets the firmware pick the threshold for the active resolution.

## Use the on-camera control in the RealSense Viewer

The [[Realsense Viewer Manual]] has an editor for this control on the depth sensor's embedded filter. The code is in [`embedded-filter-model.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/embedded-filter-model.cpp). It shows **Filter Type** (Downscale / Lookup Shift), **Downscale Ratio** (x2 / x4), **Shift Mode** (Shift 126px / Shift 64px / Manual), **Shift Pixels**, **Threshold Mode** (Zero range / MinZ (computed) / Manual) and **Threshold (mm)**. Fields that do not apply to the current selection are greyed out. The filter's on/off toggle writes the struct's `enable` field. This embedded filter does not register `RS2_OPTION_EMBEDDED_FILTER_ENABLED`.

## Devices with the on-camera control

The control is registered by `hdrd_filter_feature` ([`hdrd-filter-feature.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/features/hdrd-filter-feature.cpp)) from these D500 device classes in [`d500-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-factory.cpp) and [`d500-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-device.cpp):

| Devices (names from `rs500_sku_names`) | Minimum firmware |
|---|---|
| RealSense D555 | 7.58.45911.14188 |
| RealSense D535 Dual RGB, RealSense D585 Dual RGB, RealSense D585 Proto Dual RGB | USB: 7.58.45911.14188; MIPI: 7.58.46315.15221 |
| RealSense D535, RealSense D535F, RealSense D585, RealSense D585F, RealSense D585 Prototype | USB: 7.58.45911.14188; MIPI: 7.58.46315.15221 |
| RealSense D585 (D585S without the safety function) | USB: 7.58.45911.14188; MIPI: 7.58.46315.15221 |

RealSense D585S does not register the control. MIPI (GMSL) registration for the D535/D585 rows above is *(changed in the next release)*. At v2.59.1 the Dual RGB devices and the RealSense D535, D535F, D585, D585F and D585 Prototype registered it over USB only, while the RealSense D585 (D585S without the safety function) registered it on any connection from firmware 7.58.45911.14188.

On MIPI, the firmware cannot report the control's range. The SDK returns a built-in table instead: min `{0, 0, 1, 0, 0, 0, 0}`, max `{1, 1, 2, 2, 256, 2, 65535}`, step 1, default `{0, 0, 1, 0, 126, 1, 0}`, in field order. On USB, the range comes from the firmware.

A control can be registered and still fail on a given device or firmware. [`pytest-hdrd-control.py`](https://github.com/realsenseai/librealsense/blob/development/unit-tests/live/options/pytest-hdrd-control.py) treats a `RuntimeError` from the first read as "not functional". See also [[D500 Perception Features]] and [[Supported Devices]].

## Host-side library: run the examples

[`examples/enhanced-depth-range`](https://github.com/realsenseai/librealsense/tree/development/examples/enhanced-depth-range) holds three demos. Each streams IR left, IR right and depth, runs the host-side close-range improver on every frame, and prints a status line: the two `range_depth` demos report close-range pixels recovered, and `live_close_range_compare.py` reports the gain in valid depth pixels. They need the separately distributed `librealsense2-enhanced-depth` package, installed under `/opt/librealsense2-enhanced-depth`. See the [examples README](https://github.com/realsenseai/librealsense/blob/development/examples/enhanced-depth-range/README.md) for how to obtain it. The improver's API (`rs_depth` in Python, `rs_depth_range.h` / `rs_depth_calibration.hpp` in C++) comes from that package, not from librealsense.

| Example | What it does |
|---|---|
| [`range_depth.py`](https://github.com/realsenseai/librealsense/blob/development/examples/enhanced-depth-range/range_depth.py) | Headless. Streams 1280x720 at 30 fps and prints a live status line: close-range pixels from the camera vs. the improver, and pixels recovered |
| [`live_close_range_compare.py`](https://github.com/realsenseai/librealsense/blob/development/examples/enhanced-depth-range/live_close_range_compare.py) | Opens two OpenCV windows: camera depth and improved depth |
| [`range_depth.cpp`](https://github.com/realsenseai/librealsense/blob/development/examples/enhanced-depth-range/range_depth.cpp) | C++ version of `range_depth.py`. Loads `librs_depth_range.so` at run time with `dlopen()`, so there is no link-time dependency on it |

Build and run the C++ example as its header comment shows:

```bash
g++ -std=c++17 range_depth.cpp \
    -I/opt/librealsense2-enhanced-depth/include \
    $(pkg-config --cflags --libs realsense2) \
    -ldl -o range_depth
./range_depth        # Ctrl-C to stop
```

Both `range_depth` examples convert raw Z16 depth to millimetres with the sensor's depth scale (`get_depth_scale()`) before passing it to the improver.

## Host-side library in the RealSense Viewer

On 64-bit ARM builds, the CMake option `BUILD_WITH_CLOSE_RANGE_DEPTH` (default `OFF`, defined in [`lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake)) adds an **Improved Close Range Depth** post-processing filter to the Viewer. See [[Build Configuration]].

- At configure time, [`tools/realsense-viewer/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/tools/realsense-viewer/CMakeLists.txt) needs `rs_depth_range.hpp` and `rs_depth_calibration.hpp` under `CLOSE_RANGE_DEPTH_ROOT/include` (default `/opt/librealsense2-enhanced-depth`). Without them, configuration fails.
- At run time, the Viewer loads `librs_depth_range.so` with `dlopen()` ([`rs-depth-range-loader.h`](https://github.com/realsenseai/librealsense/blob/development/common/rs-depth-range-loader.h)). It tries the default search path first, then `/opt/librealsense2-enhanced-depth/lib/`.
- The filter is added for every non-color sensor that supports `RS2_OPTION_STEREO_BASELINE` ([`subdevice-model.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/subdevice-model.cpp)). It runs before the recommended filters, such as decimation.
- The toggle is unavailable, with a tooltip, in these cases:
  - The library is not found: "Improved Close Range Depth library not found; install librealsense2-enhanced-depth package".
  - CUDA is not detected: "Improved Close Range Depth requires CUDA (not detected on this system)".
  - The required streams are not set: "Depth, IR Left/Right streams have to be enabled at VGA or higher resolution". Depth, IR 1 and IR 2 must all be enabled at 640x480 or higher.
- If the depth and IR frames have different resolutions, the filter passes the frames through unchanged ([`close-range-depth-improver.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/close-range-depth-improver.cpp)).

## Reference: `rs2_hdrd_control`

From [`rs_hdrd_control.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_hdrd_control.h). The struct is packed and 38 bytes long: a 6-byte `dpp_header` ([`rs_dpp_header.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_dpp_header.h)) followed by eight `int32_t` slots, of which seven are used. Defaults are the values documented in the header. The device reports its own values through the range call.

<!-- wikigen:table-hdrd-control -->
| Item | Type | Values | Default |
|---|---|---|---|
| `rs2_hdrd_control::header` | `dpp_header` | `version` (must be `DPP_HEADER_CURRENT_VERSION`), `flags`, `ctl_id`, `param_count`, `param_type`. Carry it back unchanged from a read | — |
| `rs2_hdrd_control::enable` | `int32_t` | 0 = Off, 1 = On | 0 |
| `rs2_hdrd_control::filter_type` | `int32_t` | 0 = Downscale, 1 = Lookup shift | — |
| `rs2_hdrd_control::downscale_ratio` | `int32_t` | Used when `filter_type` is Downscale: 1 = x2, 2 = x4 (0 is reserved) | 1 |
| `rs2_hdrd_control::shift_mode` | `int32_t` | Used when `filter_type` is Lookup shift: 0 = 126 px, 1 = 64 px, 2 = Manual | 0 |
| `rs2_hdrd_control::shift_pixels` | `int32_t` | Used when `shift_mode` is Manual: 0–256 | 126 |
| `rs2_hdrd_control::threshold_mode` | `int32_t` | 0 = Zero range, 1 = MinZ range (computed by firmware), 2 = Manual | 1 *(changed in the next release)*; the v2.59.1 header documents 0 |
| `rs2_hdrd_control::threshold_mm` | `int32_t` | Used when `threshold_mode` is Manual: 0–65535 | 0 |
| `rs2_hdrd_control::reserved[1]` | `int32_t` | Must be zero on SET | — |
| `rs2_hdrd_control_range` | struct | `min`, `max`, `step`, `def`, each an `rs2_hdrd_control`. Returned by `rs2_get_composite_option_range(RS2_COMPOSITE_OPTION_HDRD_CONTROL)`. Read-only | — |
<!-- /wikigen:table-hdrd-control -->

Python names: `rs.hdrd_control` (fields `header`, `enable`, `filter_type`, `downscale_ratio`, `shift_mode`, `shift_pixels`, `threshold_mode`, `threshold_mm`) and `rs.hdrd_control_range` (`min`, `max`, `step`, `default`) in [`pyrs_options.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pyrs_options.cpp), and `rs.composite_option_id.hdrd_control` in [`c_files.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/c_files.cpp).

## Related pages

- [[Post Processing Filters]]: host-side filters such as decimation, which run after the close-range filter in the Viewer
- [[D500 Perception Features]]
- [[Python Wrapper]]
- [[Code Samples]]

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`46cf1a6`](https://github.com/realsenseai/librealsense/commit/46cf1a67cc441ad9f55ce339a0de30f2a1c6fed0)._
<!-- /wikigen:footer -->
