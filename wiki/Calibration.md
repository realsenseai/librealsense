librealsense gives you an API to check and improve the calibration of a depth camera in the field, and to save or reset the calibration table stored in the camera. This page covers on-chip self-calibration, tare calibration, target-based focal-length calibration, depth-to-color (UV-map) calibration, how to manage calibration tables, and the Python tool for IMU calibration. Read it if your depth quality has degraded, if you need absolute distance accuracy against a known target, or if you want to build calibration into your own application.

All functions on this page belong to the `rs2::auto_calibrated_device` class in [include/librealsense2/hpp/rs_device.hpp](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_device.hpp), which wraps the C functions in [include/librealsense2/h/rs_device.h](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_device.h). The [[Realsense Viewer Manual]] runs the same calibrations from its UI ([common/on-chip-calib.cpp](https://github.com/realsenseai/librealsense/blob/development/common/on-chip-calib.cpp), [common/d500-on-chip-calib.cpp](https://github.com/realsenseai/librealsense/blob/development/common/d500-on-chip-calib.cpp)).

## Which devices support it

A device supports this API when it can be extended to `RS2_EXTENSION_AUTO_CALIBRATED_DEVICE`. In the code, that applies to:

- **D400 devices**: `d400_device` inherits `auto_calibrated` ([src/ds/d400/d400-auto-calibration.cpp](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-auto-calibration.cpp)).
- **D500 devices over USB or MIPI**: `d500_device` inherits `d500_auto_calibrated` ([src/ds/d500/d500-auto-calibration.cpp](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-auto-calibration.cpp)).
- **DDS (Ethernet) devices whose product line is `D500`**: the DDS device proxy installs a `d500_auto_calibrated` object through `auto_calibrated_proxy` ([src/dds/rs-dds-device-proxy.cpp](https://github.com/realsenseai/librealsense/blob/development/src/dds/rs-dds-device-proxy.cpp), [src/auto-calibrated-proxy.cpp](https://github.com/realsenseai/librealsense/blob/development/src/auto-calibrated-proxy.cpp)). If it did not install one, every call throws `Auto Calibration capability has not been initiated`. See [[Ethernet and DDS Cameras]].

Not every function is implemented on both families:

| Function | D400 (`auto_calibrated`) | D500 (`d500_auto_calibrated`) |
|---|---|---|
| `run_on_chip_calibration` | Yes | Yes (see [D500 on-chip calibration](#d500-on-chip-calibration)) |
| `run_tare_calibration` | Yes | Yes |
| `process_calibration_frame` | Yes | No: throws `Process Calibration Frame not applicable for this device` |
| `run_focal_length_calibration` | Yes | Yes |
| `run_uv_map_calibration` | Yes | No: throws `UV Map Calibration not applicable for this device` |
| `calculate_target_z` | Yes | Yes |
| `get_calibration_table` / `set_calibration_table` / `write_calibration` / `reset_to_factory_calibration` | Yes | Yes |
| `get_calibration_config` / `set_calibration_config` | No: throws `Calibration Config not applicable for this device` | Yes |

To get the interface from a device:

```cpp
rs2::device dev = pipe.get_active_profile().get_device();
if( dev.is< rs2::auto_calibrated_device >() )
{
    auto cal_dev = dev.as< rs2::auto_calibrated_device >();
    // ...
}
```

## Run on-chip self-calibration

On-chip calibration (OCC) recalibrates the depth camera without a dedicated target. The [rs-on-chip-calib](https://github.com/realsenseai/librealsense/blob/development/examples/on-chip-calib/rs-on-chip-calib.cpp) example shows the full flow:

1. Start depth streaming. The example uses `cfg.enable_stream( RS2_STREAM_DEPTH, 256, 144, RS2_FORMAT_Z16, 90 )`.
2. Build a JSON string with the calibration parameters (see below).
3. Call `run_on_chip_calibration( json, &health, callback, timeout_ms )`. It returns a new `rs2::calibration_table` (a `std::vector<uint8_t>`) and writes the health-check value to `health`.
4. To keep the result, call `set_calibration_table( res )` and then `write_calibration()` to save it to flash.

```cpp
float health = 0;
rs2::calibration_table res = cal_dev.run_on_chip_calibration( json, &health,
    []( const float progress ) { std::cout << "progress = " << progress << "%" << std::endl; },
    9000 );
cal_dev.set_calibration_table( res );
cal_dev.write_calibration();
```

The C++ signatures are:

```cpp
template<class T>
calibration_table run_on_chip_calibration(std::string json_content, float* health, T callback, int timeout_ms = 5000) const;
calibration_table run_on_chip_calibration(std::string json_content, float* health, int timeout_ms = 5000) const;
```

OCC on D400 and D555, and tare calibration on D400 and D500, turn thermal compensation off while they run (`thermal_compensation_guard` in [src/ds/ds-calib-common.cpp](https://github.com/realsenseai/librealsense/blob/development/src/ds/ds-calib-common.cpp)).

### On-chip calibration JSON keys (D400)

The D400 implementation (`auto_calibrated::run_on_chip_calibration`) reads these keys. Keys it does not know are ignored. An empty string uses all defaults.

| Key | Default in code | Notes |
|---|---|---|
| `calib type` | `0` | `0` = regular, `1` = focal length, `2` = regular and focal length |
| `speed` | `3` (slow) | For `calib type` 0: `0` very fast, `1` fast, `2` medium, `3` slow, `4` white wall. Values outside 0–4 throw |
| `host assistance` | `0` | `0` none, `1` start, `2` first feed, `3` second feed. Values outside 0–3 throw |
| `scan parameter` | `0` | `0` = Py scan, `1` = Rx scan |
| `data sampling` | `1` | `0` = polling, `1` = interrupt |
| `apply preset` | `1` | Read, but the OCC code does not use it |
| `fl step count` | `100` | Focal-length scan. Checked to be 8–256 when `calib type` is 1 |
| `fy scan range` | `40` | Focal-length scan. Checked to be 1–60000 when `calib type` is 1 |
| `keep new value after sucessful scan` | `1` | Spelled as in the code |
| `fl data sampling` | `1` | `0` polling, `1` interrupt |
| `adjust both sides` | `0` | `0` adjusts the right side only, `1` adjusts both sides |
| `fl scan location` | `0` | |
| `fy scan direction` | `0` | |
| `white wall mode` | `0` | |
| `scan only` | `0` | |
| `interactive scan` | `0` | |
| `step count v3` | `0` | When > 0, the keys `fill factor 0`, `fill factor 1`, … are read |
| `resize factor` | `5` | A value given in an earlier OCC or tare call is kept for later calls |

Example JSON, as built by rs-on-chip-calib:

```json
{
 "calib type":0,
 "speed":2,
 "scan parameter":0
}
```

### D500 on-chip calibration

`d500_auto_calibrated::run_on_chip_calibration` takes one of three paths, depending on the device:

- **Name contains `D555`**: the OCC path (`run_occ`). It reads only `speed` (default slow), `scan parameter` (default Py scan) and `data sampling` (default interrupt), with the same ranges as on D400.
- **Product IDs in the D5x5 family, and D585S** (`uses_interactive_triggered_calibration` in [src/ds/d500/d500-private.h](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-private.h)): interactive triggered calibration.
- **Other D500 devices**: triggered calibration.

The two triggered paths do not parse JSON keys. They search the string for one of these commands, checked in this order: `calib dry run`, `calib commit`, `calib cancel`, `calib try new`, `calib try old`, `calib run`, `calib abort` (an alias of cancel). Any other content throws `run_on_chip_calibration called with wrong content in json file`. For example, `d500_triggered_calibration.py` calls `run_on_chip_calibration("calib run", progress_cb, 100000)`. For the full flows, see the Python examples [d500_triggered_calibration.py](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/d500_triggered_calibration.py) and [d5x5_interactive_triggered_calibration.py](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/d5x5_interactive_triggered_calibration.py).

## Run tare calibration

Tare calibration corrects the camera's absolute distance using a flat target at a known distance. You pass the measured ground truth in millimetres:

```cpp
template<class T>
calibration_table run_tare_calibration(float ground_truth_mm, std::string json_content, float* health, T callback, int timeout_ms = 5000) const;
calibration_table run_tare_calibration(float ground_truth_mm, std::string json_content, float* health, int timeout_ms = 5000) const;
```

`health` must point to at least two floats: the code writes the health-check values before and after calibration. To measure the ground truth with the camera, call `calculate_target_z` (see [Target-based calibrations](#target-based-calibrations)).

### Tare calibration JSON keys

| Key | Default in code | Range checked in code | Read by |
|---|---|---|---|
| `average step count` | `20` | 1–30 | D400, D500 |
| `step count` | `20` | 5–30 | D400, D500 |
| `accuracy` | `2` (medium) | `0` very high, `1` high, `2` medium, `3` low | D400, D500 |
| `speed` | `3` (slow) | 0–4 | D400, D500 |
| `scan parameter` | `0` (Py scan) | 0–1 | D400, D500 |
| `data sampling` | `0` (polling) | 0–1 | D400, D500 |
| `apply preset` | `1` | | D400, D500 |
| `host assistance` | `0` | 0–3 | D400 |
| `depth` | `0` | | D400 |
| `resize factor` | `5` (a value given in an earlier OCC or tare call is kept) | | D400 |

When `apply preset` is 1, the code switches the visual preset to High Accuracy for the calibration and restores the previous preset afterwards. On D500, if the previous preset was Custom, it saves and restores the advanced-mode controls *(changed in the next release)*. In v2.59.1, tare calibration on D500 with the Custom preset throws `Calibration with custom visual preset is not supported`.

### Host-assisted calibration (D400)

When `host assistance` is not 0, the D400 code collects depth frames on the host. Each depth frame is passed to `process_calibration_frame( frame, health, timeout_ms )`, which returns an empty table until calibration finishes and the new table when it is done. [depth_ucal_example.py](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/depth_ucal_example.py) runs this flow for OCC and tare.

## Target-based calibrations

These calibrations take frame queues filled with images of a calibration target.

### Focal-length calibration

```cpp
std::vector<uint8_t> run_focal_length_calibration(rs2::frame_queue left, rs2::frame_queue right,
    float target_w, float target_h, int adjust_both_sides, float* ratio, float* angle) const;
// plus an overload that adds a progress callback as the last argument
```

- `left`, `right`: left and right IR frames of the target.
- `target_w`, `target_h`: size of the rectangle on the target, in mm.
- `adjust_both_sides`: `1` corrects both left and right focal lengths, `0` corrects the right one only.
- `ratio`, `angle`: receive the correction ratio and the target's tilt angle.

Both D400 and D500 implement it and apply the correction the same way: the correction factor is `ratio / 100 + 1` (`ratio` is a percentage). The code multiplies the right camera's `fx`/`fy` by this factor. When `adjust_both_sides` is set, it instead multiplies the right camera's values and divides the left camera's values by the factor's square root.

### UV-map (depth-to-color) calibration (D400 only)

```cpp
std::vector<uint8_t> run_uv_map_calibration(rs2::frame_queue left, rs2::frame_queue color, rs2::frame_queue depth,
    int py_px_only, float* health, int health_size) const;
// plus an overload that adds a progress callback as the last argument
```

It calibrates the color camera against depth and returns a color calibration table, or an empty table if px, py, fx or fy would change by 16 pixels or more. `health` must hold four floats: when a table is returned, the code writes the percent change of px, py, fx, fy, and it only checks that `health_size` is greater than 0. The Viewer calls it with `health_size` 4.

### Measure distance to the target

```cpp
float calculate_target_z(rs2::frame_queue queue1, rs2::frame_queue queue2, rs2::frame_queue queue3,
    float target_width, float target_height) const;
// plus an overload that adds a progress callback
```

It returns the distance to the target plane in millimetres. On D400 the queues are left IR, right IR and depth, and only the left IR queue is used. On both D400 and D500, `queue1` must hold at least 10 frames, otherwise the call throws.

## Read, write and reset the calibration table

| Method | What it does |
|---|---|
| `calibration_table get_calibration_table()` | Reads the current depth calibration table from the camera |
| `void set_calibration_table(const calibration_table&)` | Makes a table the current one. On D400, a depth table is loaded into RAM so the camera uses it right away, and a color table (from UV-map calibration) is only kept for `write_calibration`. On D500, the table is sent to RAM |
| `void write_calibration() const` | Saves the table from `set_calibration_table` to flash |
| `void reset_to_factory_calibration()` | Restores the factory calibration |
| `std::string get_calibration_config() const` | D500 only: returns the calibration config as a JSON string |
| `void set_calibration_config(const std::string&) const` | D500 only: sets the calibration config from a JSON string |

`write_calibration` and `reset_to_factory_calibration` come from the base class `rs2::calibrated_device`.

On D400, calling `write_calibration()` before `set_calibration_table()` throws `Write calibration can be called only after set calibration table was called`. On D500, `set_calibration_table` checks that the table size matches a depth or RGB table and throws otherwise.

A calibration result is used right away but is lost after a reset unless you save it. To go back without writing to flash, save the output of `get_calibration_table()` before calibrating and set it again, or reset the camera, as the comments in rs-on-chip-calib explain.

```cpp
auto cal_dev = dev.as< rs2::auto_calibrated_device >();
rs2::calibration_table backup = cal_dev.get_calibration_table();
// ... run a calibration, inspect the result ...
cal_dev.set_calibration_table( backup );     // back to the previous table
// or: cal_dev.reset_to_factory_calibration();
```

## Python

pyrealsense2 exposes the same methods on `rs.auto_calibrated_device` ([wrappers/python/pyrs_device.cpp](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pyrs_device.cpp)). `run_on_chip_calibration`, `run_tare_calibration` and `process_calibration_frame` return the new table together with the health values instead of writing to an output argument. Examples:

- [depth_auto_calibration_example.py](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/depth_auto_calibration_example.py)
- [depth_ucal_example.py](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/depth_ucal_example.py): OCC and tare, including host-assisted flows
- [d500_triggered_calibration.py](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/d500_triggered_calibration.py)
- [d5x5_interactive_triggered_calibration.py](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/d5x5_interactive_triggered_calibration.py)

## The rs-on-chip-calib example

Source: [examples/on-chip-calib](https://github.com/realsenseai/librealsense/blob/development/examples/on-chip-calib/rs-on-chip-calib.cpp). It is built with the other examples (see [[Build Configuration]]).

The example:

1. Parses the command line with `rs2::cli( "on-chip-calib example" )` and passes the settings to the pipeline.
2. Streams depth at 256x144, Z16, 90 fps, and waits for the first frames.
3. Runs OCC with `"calib type": 0`, `"speed": 2`, `"scan parameter": 0` and a 9000 ms timeout, printing `progress = …%`.
4. Prints `Completed successfully` and asks `Keep results? Yes/No`. If you answer `y` or `yes`, it calls `set_calibration_table` and `write_calibration` and prints `Results saved to flash`. Otherwise it prints `Results not saved`.

Command-line options (from [common/cli.h](https://github.com/realsenseai/librealsense/blob/development/common/cli.h)):

| Flag | Description |
|---|---|
| `--debug` | Turn on librealsense debug logs (registered only when built with easylogging++) |
| `--eth` | Detect DDS devices, even if disabled in the configuration |
| `--eth-only` | Use only DDS devices |
| `--no-eth` | Do not detect DDS devices, even if enabled in the configuration |
| `--domain-id <0-232>` | Domain ID to use with DDS devices |
| `-h`, `--help` | Show usage |
| `--version` | Show version |

The DDS flags are ignored when the build has no DDS support. `--eth`, `--eth-only` and `--no-eth` are mutually exclusive, and so are `--domain-id` and `--no-eth`.

## IMU calibration tool

[tools/rs-imu-calibration/rs-imu-calibration.py](https://github.com/realsenseai/librealsense/blob/development/tools/rs-imu-calibration/rs-imu-calibration.py) is a Python script that calibrates the accelerometer of a camera with an IMU and computes the gyroscope bias. It needs `pyrealsense2` and `numpy`.

### Usage

```
python rs-imu-calibration.py [-s <serial>] [-i <accel.txt> [<gyro.txt>]] [-g]
```

| Flag | Meaning |
|---|---|
| `-i <accel> [<gyro>]` | Compute the calibration from previously saved files instead of collecting live data. The gyro file is optional |
| `-s <serial>` | Serial number of the device to use. Default: the first device found |
| `-g` | Show a graph of the norm values: original values in blue, corrected values in green |
| `-h`, `--help`, `/?` | Print usage and exit |

The script always waits up to about 5 seconds for a device (`wait_for_rs_device`), even with `-i`, and fails with `No RealSense device` if none is found.

### What it does

1. **Interactive mode** (no `-i`): the script starts the device, finds a sensor with both gyro and accel streams in `MOTION_XYZ32F` format, and, if the sensor supports it, turns off `enable_motion_correction` so it reads raw values. It then asks you to place the camera in six orientations, one after the other:
   - Mounting screw pointing down, device facing out
   - Mounting screw pointing left, device facing out
   - Mounting screw pointing up, device facing out
   - Mounting screw pointing right, device facing out
   - Viewing direction facing down
   - Viewing direction facing up

   For each one, hold the camera still until data has been collected. Press ESC to quit. You can then save the raw data as `accel_<footer>.txt` and `gyro_<footer>.txt`.
2. **File mode** (`-i`): it reads the accelerometer CSV file. If you give a gyro file, it uses the first 4 seconds of it to compute the gyro bias.
3. It fits the accelerometer scale/alignment and bias with least squares, and prints per-axis noise variances for accelerometer and gyroscope.
4. It writes two files, next to the accel file in `-i` mode or in the current directory otherwise:
   - `calibration.json`: `device_type` (always `"D435i"`), and under `imus[0]`, the `accelerometer` and `gyroscope` entries with `scale_and_alignment`, `bias` and, when the variance could be computed, `noise_variances` (the gyroscope has none in `-i` mode without a gyro file). The gyroscope scale is the identity matrix.
   - `calibration.bin`: the binary IMU calibration table.
5. It asks `Would you like to write the results to the camera? (Y/N)`. On `Y`, it builds an EEPROM image and sends it with a raw `rs.debug_protocol` command. It then prints `SUCCESS: saved calibration to camera.` or `FAILED: failed to save calibration to camera.`

### Supported devices

The script does not check the device model. It uses any device that has accel and gyro streams (interactive mode) and accepts a debug-protocol command. The table and command it writes follow the D400 IMU table layout; the comments in the script refer to "all D400 cameras with IMU". The script reads the product line but does not use it to pick a table format.

When librealsense opens the motion sensor, it reads the IMU calibration table from the camera ([src/ds/ds-calib-parsers.cpp](https://github.com/realsenseai/librealsense/blob/development/src/ds/ds-calib-parsers.cpp)). If the table is invalid or does not mark its intrinsics as valid, default intrinsic values are used.

## Source files

- [include/librealsense2/hpp/rs_device.hpp](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_device.hpp): `rs2::calibrated_device`, `rs2::auto_calibrated_device`
- [include/librealsense2/h/rs_device.h](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_device.h): C API (`rs2_run_on_chip_calibration`, `rs2_run_tare_calibration`, `rs2_get_calibration_table`, …)
- [src/auto-calibrated-device.h](https://github.com/realsenseai/librealsense/blob/development/src/auto-calibrated-device.h): internal `auto_calibrated_interface`
- [src/auto-calibrated-proxy.cpp](https://github.com/realsenseai/librealsense/blob/development/src/auto-calibrated-proxy.cpp): forwarding for DDS devices
- [src/ds/ds-calib-common.cpp](https://github.com/realsenseai/librealsense/blob/development/src/ds/ds-calib-common.cpp): parameter checks and shared helpers
- [src/ds/d400/d400-auto-calibration.cpp](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-auto-calibration.cpp), [src/ds/d500/d500-auto-calibration.cpp](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-auto-calibration.cpp): device implementations

See also [[Supported Devices]], [[D400 Series Visual Presets]] and [[Depth Quality Tool]].

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`65ed974`](https://github.com/realsenseai/librealsense/commit/65ed97469471f9d104969d2b7d029506058819bd)._
<!-- /wikigen:footer -->
