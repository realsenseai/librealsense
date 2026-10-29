Frame metadata is the set of read-only, per-frame attributes that librealsense attaches to each frame: counters, device and host timestamps, exposure, gain, laser power, and (on devices that report them) safety and diagnostic values. You need it when you have to know the exact capture conditions of a frame, for example to synchronize streams, detect dropped frames, log exposure settings, or read device-computed values.

The attributes are defined by the `rs2_frame_metadata_value` enum in [`include/librealsense2/h/rs_frame.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_frame.h). Whether a given frame carries a given attribute depends on the device, its firmware, the stream, and whether the host platform delivers the metadata payload (see [How metadata reaches the host](#how-metadata-reaches-the-host)).

## Reading metadata

The query model is check-then-query: first ask whether the frame supports an attribute, then read it. Values are returned as `rs2_metadata_type`, a 64-bit signed integer (`long long`, defined in [`rs_types.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_types.h)).

### C++

`rs2::frame` in [`include/librealsense2/hpp/rs_frame.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_frame.hpp) provides:

- `bool supports_frame_metadata(rs2_frame_metadata_value frame_metadata) const`
- `rs2_metadata_type get_frame_metadata(rs2_frame_metadata_value frame_metadata) const`

```cpp
#include <librealsense2/rs.hpp>
#include <iostream>

int main()
{
    rs2::pipeline pipe;
    pipe.start();
    rs2::frameset frames = pipe.wait_for_frames();
    rs2::depth_frame depth = frames.get_depth_frame();

    if (depth.supports_frame_metadata(RS2_FRAME_METADATA_FRAME_COUNTER))
        std::cout << "Frame counter: "
                  << depth.get_frame_metadata(RS2_FRAME_METADATA_FRAME_COUNTER) << std::endl;
}
```

Calling `get_frame_metadata` for an attribute the frame does not carry throws an error. The library builds the message as `<stream> frame does not support metadata "<attribute>"` (in `rs2_get_frame_metadata`, [`src/rs.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/rs.cpp)), so always check with `supports_frame_metadata` first.

To dump every attribute a frame carries, loop over the enum up to `RS2_FRAME_METADATA_COUNT` and use `rs2_frame_metadata_to_string` for the name. The [save-to-disk example](https://github.com/realsenseai/librealsense/blob/development/examples/save-to-disk/rs-save-to-disk.cpp) does exactly this and writes the result to a CSV file per stream.

### C

The C API in [`rs_frame.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_frame.h) has the same pair:

```c
int rs2_supports_frame_metadata(const rs2_frame* frame, rs2_frame_metadata_value frame_metadata, rs2_error** error);
rs2_metadata_type rs2_get_frame_metadata(const rs2_frame* frame, rs2_frame_metadata_value frame_metadata, rs2_error** error);
```

`rs2_frame_metadata_to_string` and `rs2_timestamp_domain_to_string` convert enum values to readable names.

### Python

The [[Python Wrapper]] binds the same methods on `pyrealsense2.frame` (see [`wrappers/python/pyrs_frame.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pyrs_frame.cpp)): `supports_frame_metadata(frame_metadata)` and `get_frame_metadata(frame_metadata)`. The enum is exposed as `rs.frame_metadata_value`. Python value names are the library's string names in lower case with spaces replaced by underscores (for example `RS2_FRAME_METADATA_FRAME_COUNTER` becomes `rs.frame_metadata_value.frame_counter`).

```python
import pyrealsense2 as rs

pipe = rs.pipeline()
pipe.start()
frames = pipe.wait_for_frames()
depth = frames.get_depth_frame()

md = rs.frame_metadata_value.frame_counter
if depth.supports_frame_metadata(md):
    print("Frame counter:", depth.get_frame_metadata(md))
```

## Frame timestamps and timestamp domains

Every frame also has a timestamp, read with `rs2::frame::get_timestamp()` (milliseconds), and a timestamp domain that tells which clock the timestamp refers to, read with `get_frame_timestamp_domain()`. In Python these are `get_timestamp()` / the `timestamp` property and `get_frame_timestamp_domain()` / the `frame_timestamp_domain` property.

According to the documentation of `get_timestamp()` in [`rs_frame.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_frame.hpp), the timestamps related to a frame are, in order:

- `RS2_FRAME_METADATA_SENSOR_TIMESTAMP`: device clock; for video sensors, the middle of exposure. Needs metadata support.
- `RS2_FRAME_METADATA_FRAME_TIMESTAMP`: device clock, stamped when frame readout and transfer start. Needs metadata support.
- `RS2_FRAME_METADATA_BACKEND_TIMESTAMP`: host clock, when the frame passes from the USB controller to the driver.
- `RS2_FRAME_METADATA_TIME_OF_ARRIVAL`: host clock, when the frame reaches librealsense.

When frame metadata is not available, `get_timestamp()` returns the time of arrival. When it is available, the function returns the hardware timestamp or, if global time is enabled, a host-corrected version of it. Use the `RS2_OPTION_GLOBAL_TIME_ENABLED` sensor option to switch between the two.

The `rs2_timestamp_domain` values, in code order:

<!-- wikigen:table-timestamp-domain -->
| Value | Description |
|---|---|
| `RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK` | Frame timestamp was measured in relation to the camera clock |
| `RS2_TIMESTAMP_DOMAIN_SYSTEM_TIME` | Frame timestamp was measured in relation to the OS system clock |
| `RS2_TIMESTAMP_DOMAIN_GLOBAL_TIME` | Frame timestamp was measured in relation to the camera clock and converted to OS system clock by constantly measure the difference |
<!-- /wikigen:table-timestamp-domain -->

## How metadata reaches the host

On USB cameras, video frames carry device-generated attributes in the UVC payload header of each frame (Ethernet/DDS devices send them as separate metadata messages that librealsense attaches to the matching frame). The host backend must pass that header to librealsense, which stores it with the frame and parses individual attributes only when you ask for them. If the header does not reach the library, only host-side attributes such as `RS2_FRAME_METADATA_TIME_OF_ARRIVAL` are available, and the frame timestamp falls back to the system clock.

### Linux (V4L2 backend)

The V4L2 backend reads metadata from a separate V4L2 metadata capture node that belongs to the video device (a node that reports `V4L2_CAP_META_CAPTURE`). It configures the node with the `V4L2_META_FMT_D4XX` format and falls back to `V4L2_META_FMT_UVC` (see [`src/linux/v4l-uvc-meta-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/linux/v4l-uvc-meta-device.cpp)). The kernel `uvcvideo` driver creates this node only for devices it recognizes as metadata-capable.

For kernels whose `uvcvideo` driver does not yet know a camera, the repository ships kernel patches in [`scripts/`](https://github.com/realsenseai/librealsense/blob/development/scripts). The `realsense-metadata-<distribution>-<kernel>.patch` files add RealSense device IDs to `drivers/media/usb/uvc/uvc_driver.c` and mark them with `UVC_INFO_META(V4L2_META_FMT_D4XX)`. The patch scripts, for example [`scripts/patch-realsense-ubuntu-lts-hwe.sh`](https://github.com/realsenseai/librealsense/blob/development/scripts/patch-realsense-ubuntu-lts-hwe.sh), apply the metadata patch for the running kernel along with the other RealSense patches, then build the patched kernel modules and load them in place of the stock ones. See the installation pages for the full procedure.

### Windows (Media Foundation backend)

The Windows backend reads the UVC header from the Media Foundation sample attributes (see [`src/mf/mf-uvc.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/mf/mf-uvc.cpp)). Two conditions apply:

- **Build:** metadata support is compiled in unless the build uses WinSDK 8.1 or older. With such an SDK, the compiler prints a notification that UVC metadata requires WinSDK 10.0.10586.0 and "The library will be compiled without the metadata support!", and the build continues. Set the CMake option `ENFORCE_METADATA` (default `OFF`) to make such a build fail instead (see [[Build Configuration]]).
- **Registry:** Windows passes metadata only for video devices that have the `MetadataBufferSizeInKB<n>` registry values set.

[`scripts/realsense_metadata_win10.ps1`](https://github.com/realsenseai/librealsense/blob/development/scripts/realsense_metadata_win10.ps1) sets those values. When it runs, it:

1. Relaunches itself with administrator rights if needed.
2. Finds RealSense devices (vendor IDs `8086` and `38E5`) under the `usbvideo` service and in two device-class branches under `HKLM:\SYSTEM\CurrentControlSet\Control\DeviceClasses`.
3. Under each device's `#global\Device Parameters` key, sets `MetadataBufferSizeInKB0` to `5` if it is missing. For multi-pin devices (a fixed list of `VID_8086` product IDs in the script, plus every `VID_38E5` device), it also sets `MetadataBufferSizeInKB1`, `MetadataBufferSizeInKB2` and `MetadataBufferSizeInKB3`.

Pass the operation with `-op`:

| `-op` value | Effect |
|---|---|
| `install` (default) | Add the keys for currently connected RealSense devices |
| `install_all` | Add the keys for all RealSense devices registered in the system, connected or not |
| `remove` | Remove the keys for currently connected RealSense devices |
| `remove_all` | Remove the keys for all registered RealSense devices |

```powershell
PS> .\realsense_metadata_win10.ps1 -op install_all
```

### RSUSB backend

When librealsense is built with `FORCE_RSUSB_BACKEND` (see [[Build Configuration]]; CMake turns it on automatically on macOS and Windows 7), it talks to the camera through its own user-space UVC implementation. That implementation passes each frame's payload header to the library directly (see [`src/uvc/uvc-streamer.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/uvc/uvc-streamer.cpp)), so kernel patches and registry keys do not apply.

## Which attributes a frame carries

Each device registers a parser for each attribute it supports. Asking for an attribute without a parser, or one that is not in the current frame's payload, makes `supports_frame_metadata` return false. Firmware can also send different sets of attributes on different frames, so a registered attribute may still be missing from some frames. Always check per frame.

Parser classes are in [`src/metadata-parser.h`](https://github.com/realsenseai/librealsense/blob/development/src/metadata-parser.h), and the device payload layouts are in [`src/metadata.h`](https://github.com/realsenseai/librealsense/blob/development/src/metadata.h). For background on the design, see [`doc/frame_metadata.md`](https://github.com/realsenseai/librealsense/blob/development/doc/frame_metadata.md).

Recordings also store frame metadata (see [[Record and Playback]]).

## Metadata attribute reference

All `rs2_frame_metadata_value` values in code order, with the description from the header comment in [`rs_frame.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_frame.h). `RS2_FRAME_METADATA_COUNT` is the number of values and is not a valid attribute. A dash means the header has no comment for that value.

<!-- wikigen:table-metadata -->
| Value | Description |
|---|---|
| `RS2_FRAME_METADATA_FRAME_COUNTER` | A sequential index managed per-stream. Integer value |
| `RS2_FRAME_METADATA_FRAME_TIMESTAMP` | Timestamp set by device clock when data readout and transmit commence. usec |
| `RS2_FRAME_METADATA_SENSOR_TIMESTAMP` | Timestamp of the middle of sensor's exposure calculated by device. usec |
| `RS2_FRAME_METADATA_ACTUAL_EXPOSURE` | Sensor's exposure width. When Auto Exposure (AE) is on the value is controlled by firmware. usec |
| `RS2_FRAME_METADATA_GAIN_LEVEL` | A relative value increasing which will increase the Sensor's gain factor. When AE is set On, the value is controlled by firmware. Integer value |
| `RS2_FRAME_METADATA_AUTO_EXPOSURE` | Auto Exposure Mode indicator. Zero corresponds to AE switched off. |
| `RS2_FRAME_METADATA_WHITE_BALANCE` | White Balance setting as a color temperature. Kelvin degrees |
| `RS2_FRAME_METADATA_TIME_OF_ARRIVAL` | Time of arrival in system clock |
| `RS2_FRAME_METADATA_TEMPERATURE` | Temperature of the device, measured at the time of the frame capture. Celsius degrees |
| `RS2_FRAME_METADATA_BACKEND_TIMESTAMP` | Timestamp get from uvc driver. usec |
| `RS2_FRAME_METADATA_ACTUAL_FPS` | Actual fps, times 1000 (30.1 fps would be 30100 in the metadata) |
| `RS2_FRAME_METADATA_FRAME_LASER_POWER` | Laser power value 0-360. |
| `RS2_FRAME_METADATA_FRAME_LASER_POWER_MODE` | Laser power mode. Zero corresponds to Laser power switched off and one for switched on. deprecated, replaced by RS2_FRAME_METADATA_FRAME_EMITTER_MODE |
| `RS2_FRAME_METADATA_EXPOSURE_PRIORITY` | Exposure priority. |
| `RS2_FRAME_METADATA_EXPOSURE_ROI_LEFT` | Left region of interest for the auto exposure Algorithm. |
| `RS2_FRAME_METADATA_EXPOSURE_ROI_RIGHT` | Right region of interest for the auto exposure Algorithm. |
| `RS2_FRAME_METADATA_EXPOSURE_ROI_TOP` | Top region of interest for the auto exposure Algorithm. |
| `RS2_FRAME_METADATA_EXPOSURE_ROI_BOTTOM` | Bottom region of interest for the auto exposure Algorithm. |
| `RS2_FRAME_METADATA_BRIGHTNESS` | Color image brightness. |
| `RS2_FRAME_METADATA_CONTRAST` | Color image contrast. |
| `RS2_FRAME_METADATA_SATURATION` | Color image saturation. |
| `RS2_FRAME_METADATA_SHARPNESS` | Color image sharpness. |
| `RS2_FRAME_METADATA_AUTO_WHITE_BALANCE_TEMPERATURE` | Auto white balance temperature Mode indicator. Zero corresponds to automatic mode switched off. |
| `RS2_FRAME_METADATA_BACKLIGHT_COMPENSATION` | Color backlight compensation. Zero corresponds to switched off. |
| `RS2_FRAME_METADATA_HUE` | Color image hue. |
| `RS2_FRAME_METADATA_GAMMA` | Color image gamma. |
| `RS2_FRAME_METADATA_MANUAL_WHITE_BALANCE` | Color image white balance. Manual WB value on D400, current WB value on D585S |
| `RS2_FRAME_METADATA_POWER_LINE_FREQUENCY` | Power Line Frequency for anti-flickering Off/50Hz/60Hz/Auto. |
| `RS2_FRAME_METADATA_LOW_LIGHT_COMPENSATION` | Color lowlight compensation. Zero corresponds to switched off. |
| `RS2_FRAME_METADATA_FRAME_EMITTER_MODE` | Emitter mode: 0 - all emitters disabled. 1 - laser enabled. 2 - auto laser enabled (opt). 3 - LED enabled (opt). |
| `RS2_FRAME_METADATA_FRAME_LED_POWER` | Led power value 0-360. |
| `RS2_FRAME_METADATA_RAW_FRAME_SIZE` | The number of transmitted payload bytes, not including metadata |
| `RS2_FRAME_METADATA_GPIO_INPUT_DATA` | GPIO input data |
| `RS2_FRAME_METADATA_SEQUENCE_NAME` | sub-preset id |
| `RS2_FRAME_METADATA_SEQUENCE_ID` | sub-preset sequence id |
| `RS2_FRAME_METADATA_SEQUENCE_SIZE` | sub-preset sequence size |
| `RS2_FRAME_METADATA_TRIGGER` | Frame trigger type |
| `RS2_FRAME_METADATA_PRESET` | Preset id, used in MIPI SKU Metadata |
| `RS2_FRAME_METADATA_INPUT_WIDTH` | Frame input width in pixels, used as safety attribute |
| `RS2_FRAME_METADATA_INPUT_HEIGHT` | Frame input height in pixels, used as safety attribute |
| `RS2_FRAME_METADATA_SUB_PRESET_INFO` | Sub-preset information |
| `RS2_FRAME_METADATA_CALIB_INFO` | FW-controlled frame counter to be using in Calibration scenarios |
| `RS2_FRAME_METADATA_CRC` | CRC checksum of the Metadata |
| `RS2_FRAME_METADATA_SAFETY_DEPTH_FRAME_COUNTER` | Counter of the depth frame upon which the stream was calculated |
| `RS2_FRAME_METADATA_SAFETY_LEVEL1` | Designates the Yellow zone status: 0x1 - High, 0x0 - Low |
| `RS2_FRAME_METADATA_SAFETY_LEVEL1_ORIGIN` | When l1 is low - equals to frame_counter in safety_header - For l1=0x1 : hold the Frame id on last transition to "High" state |
| `RS2_FRAME_METADATA_SAFETY_LEVEL2` | Designates the "Red" zone status: 0x1 - High, 0x0 - Low |
| `RS2_FRAME_METADATA_SAFETY_LEVEL2_ORIGIN` | When l2 is low - equals to frame_counter in safety_header - For l2=0x1 : hold the Frame id on last transition to "High" state |
| `RS2_FRAME_METADATA_SAFETY_LEVEL1_VERDICT` | Current verdict for l1 Safety Signal - May differ from l1_signal due to additional logics applied |
| `RS2_FRAME_METADATA_SAFETY_LEVEL2_VERDICT` | Current verdict for l2 Safety Signal - May differ from l2_signal due to additional logics applied |
| `RS2_FRAME_METADATA_SAFETY_OPERATIONAL_MODE` | Reflects the SC operational mode (XU control) |
| `RS2_FRAME_METADATA_SAFETY_VISION_VERDICT` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_HARA_EVENTS` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_PRESET_INTEGRITY` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_PRESET_ID_SELECTED` | Safety Preset index set via Adaptive Field selection GPIO |
| `RS2_FRAME_METADATA_SAFETY_PRESET_ID_USED` | Safety Preset index used in the latest Vision Safety algo processing |
| `RS2_FRAME_METADATA_SAFETY_SIP_DEGRADATION_USED` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_SIP_GENERIC_METRICS_ACTIVATE` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_SIP_GENERIC_METRICS_STATE` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_SIP_GENERIC_METRICS_VALUE1` | First value from byte array of up to 8 Generic Safety Algo continuous metrics indications |
| `RS2_FRAME_METADATA_SAFETY_SIP_GENERIC_METRICS_VALUE2` | Second value from byte array of up to 8 Generic Safety Algo continuous metrics indications |
| `RS2_FRAME_METADATA_SAFETY_SIP_GENERIC_METRICS_THRESHOLD1` | First value from byte array of up to 8 Generic Safety Algo continuous metrics thresholds |
| `RS2_FRAME_METADATA_SAFETY_SIP_GENERIC_METRICS_THRESHOLD2` | Second value from byte array of up to 8 Generic Safety Algo continuous metrics thresholds |
| `RS2_FRAME_METADATA_SAFETY_ZERO_MONITORING_ENABLED` | Enum - 0:Regular (default) all safety in nominal mode - 1: "Zero Safety" mode |
| `RS2_FRAME_METADATA_SAFETY_HARA_HISTORY_MODE` | Enum - 0:Regular - 1: No history - 2: Local history |
| `RS2_FRAME_METADATA_SAFETY_SOC_FUSA_EVENTS` | Bitmask, enumerated - SOC critical notification: L2/L3 that requires handling/troubleshooting in S.MCU 32-bit value, as supplied by STL mechanism |
| `RS2_FRAME_METADATA_SAFETY_SOC_FUSA_ACTION` | Bitmask, enumerated - Action taken. 0x1 << 0 - HKR Reset, 0x1 << 1 - HKR ShutDown |
| `RS2_FRAME_METADATA_SAFETY_SOC_L0_COUNTER` | Total number of L0 notifications captured |
| `RS2_FRAME_METADATA_SAFETY_SOC_L0_RATE` | L_0 notifications per FDTI |
| `RS2_FRAME_METADATA_SAFETY_SOC_L1_COUNTER` | Total number of L1 notifications captured |
| `RS2_FRAME_METADATA_SAFETY_SOC_L1_RATE` | L_1 notifications per FDTI |
| `RS2_FRAME_METADATA_SAFETY_SOC_GMT_STATUS` | Result, enumerated: 0 - GMT Clock Ok, 1- GMT Clock is outside Safe threshold, 2 - GMT Clock is not available |
| `RS2_FRAME_METADATA_SAFETY_SOC_HKR_CRITICAL_ERROR_GPIO` | Critical-erro GPIO Status: 0 - off, 1 - on |
| `RS2_FRAME_METADATA_SAFETY_SOC_MONITOR_L2_ERROR_TYPE` | Soc Monitor Escalation of L2 error to Arbiter |
| `RS2_FRAME_METADATA_SAFETY_SOC_MONITOR_L3_ERROR_TYPE` | Soc Monitor Escalation of L3 error to Arbiter |
| `RS2_FRAME_METADATA_SAFETY_SOC_SAFETY_AND_SECURITY` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_DEPTH_FRAME_TIMESTAMP` | — |
| `RS2_FRAME_METADATA_SAFETY_SMCU_PROCESSING_TIMESTAMP` | — |
| `RS2_FRAME_METADATA_SAFETY_PIPELINE_PROPAGATION_DELAY` | — |
| `RS2_FRAME_METADATA_SAFETY_SMCU_DEBUG_STATUS_BITMASK` | Bitmask: The values are state-specific and non-FuSa related. Shall be presented as HEX string |
| `RS2_FRAME_METADATA_SAFETY_SMCU_DEBUG_INFO_INTERNAL_STATE` | Enumerated: according to S.MCU FSM |
| `RS2_FRAME_METADATA_SAFETY_SMCU_DEBUG_INFO_BIST_STATUS` | Bitmask. To be presented as HEX value |
| `RS2_FRAME_METADATA_SAFETY_NON_FUSA_GPIO_OUT` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_SMCU_HW_MONITOR_STATUS` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_SMCU_SW_MONITOR_STATUS` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_NON_FUSA_GPIO_IN` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_MB_FUSA_EVENT` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_MB_FUSA_ACTION` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_MB_STATUS` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_SMCU_LIVELINESS` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_SMCU_STATE` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_PRESET_ID` | Designates the Safety Zone index in [0..63] range used in algo pipe |
| `RS2_FRAME_METADATA_SENSOR_ANGLE_ROLL` | In millidegrees. Relative to X (forward) axis. Positive value is CCW |
| `RS2_FRAME_METADATA_SENSOR_ANGLE_PITCH` | In millidegrees. Relative to Y (left) axis. Positive value is CCW |
| `RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_MEDIAN_HEIGHT` | In millimeters. Relative to the "leveled pointcloud" CS |
| `RS2_FRAME_METADATA_FLOOR_DETECTION` | Percentage |
| `RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_FILL_RATE` | Percentage |
| `RS2_FRAME_METADATA_DEPTH_FILL_RATE` | Unsigned value in range of [0..100]. Use [x = 0xFF] if not applicable |
| `RS2_FRAME_METADATA_DEPTH_STDEV` | Spatial accuracy in millimetric units |
| `RS2_FRAME_METADATA_OCCUPANCY_GRID_ROWS` | Number of rows in the grid. Max value is 250 (corresponding to 5M width with 2cm tile) |
| `RS2_FRAME_METADATA_OCCUPANCY_GRID_COLUMNS` | Number of columns in the grid. Max value is 320 (corresponding to ~6.5M depth with 2cm tile) |
| `RS2_FRAME_METADATA_OCCUPANCY_CELL_SIZE` | Edge size of each tile, measured in cm |
| `RS2_FRAME_METADATA_NUMBER_OF_3D_VERTICES` | The max number of points is 640x360 |
| `RS2_FRAME_METADATA_SAFETY_PRESET_ERROR_TYPE` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_SAFETY_PRESET_ERROR_PARAM_1` | Preset Error Param. Enumerated |
| `RS2_FRAME_METADATA_SAFETY_PRESET_ERROR_PARAM_2` | Preset Error Param. Enumerated |
| `RS2_FRAME_METADATA_DANGER_ZONE_POINT_0_X_CORD` | Danger Zone point #0, X coord in mm |
| `RS2_FRAME_METADATA_DANGER_ZONE_POINT_0_Y_CORD` | Danger Zone point #0, Y coord in mm |
| `RS2_FRAME_METADATA_DANGER_ZONE_POINT_1_X_CORD` | Danger Zone point #1, X coord in mm |
| `RS2_FRAME_METADATA_DANGER_ZONE_POINT_1_Y_CORD` | Danger Zone point #1, Y coord in mm |
| `RS2_FRAME_METADATA_DANGER_ZONE_POINT_2_X_CORD` | Danger Zone point #2, X coord in mm |
| `RS2_FRAME_METADATA_DANGER_ZONE_POINT_2_Y_CORD` | Danger Zone point #2, Y coord in mm |
| `RS2_FRAME_METADATA_DANGER_ZONE_POINT_3_X_CORD` | Danger Zone point #3, X coord in mm |
| `RS2_FRAME_METADATA_DANGER_ZONE_POINT_3_Y_CORD` | Danger Zone point #3, Y coord in mm |
| `RS2_FRAME_METADATA_WARNING_ZONE_POINT_0_X_CORD` | Warning Zone point #0, X coord in mm |
| `RS2_FRAME_METADATA_WARNING_ZONE_POINT_0_Y_CORD` | Warning Zone point #0, Y coord in mm |
| `RS2_FRAME_METADATA_WARNING_ZONE_POINT_1_X_CORD` | Warning Zone point #1, X coord in mm |
| `RS2_FRAME_METADATA_WARNING_ZONE_POINT_1_Y_CORD` | Warning Zone point #1, Y coord in mm |
| `RS2_FRAME_METADATA_WARNING_ZONE_POINT_2_X_CORD` | Warning Zone point #2, X coord in mm |
| `RS2_FRAME_METADATA_WARNING_ZONE_POINT_2_Y_CORD` | Warning Zone point #2, Y coord in mm |
| `RS2_FRAME_METADATA_WARNING_ZONE_POINT_3_X_CORD` | Warning Zone point #3, X coord in mm |
| `RS2_FRAME_METADATA_WARNING_ZONE_POINT_3_Y_CORD` | Warning Zone point #3, Y coord in mm |
| `RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_0_X_CORD` | Diagnostic Zone point #0, X coord in mm |
| `RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_0_Y_CORD` | Diagnostic Zone point #0, Y coord in mm |
| `RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_1_X_CORD` | Diagnostic Zone point #1, X coord in mm |
| `RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_1_Y_CORD` | Diagnostic Zone point #1, Y coord in mm |
| `RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_2_X_CORD` | Diagnostic Zone point #2, X coord in mm |
| `RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_2_Y_CORD` | Diagnostic Zone point #2, Y coord in mm |
| `RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_3_X_CORD` | Diagnostic Zone point #3, X coord in mm |
| `RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_3_Y_CORD` | Diagnostic Zone point #3, Y coord in mm |
| `RS2_FRAME_METADATA_EMBEDDED_FILTERS` | Bitmask, enumerated |
| `RS2_FRAME_METADATA_OCCUPANCY_GRID_ORIGIN_X` *(new in the next release)* | Occupancy-grid origin on the forward axis, in millimeters |
| `RS2_FRAME_METADATA_OCCUPANCY_GRID_ORIGIN_Y` *(new in the next release)* | Occupancy-grid origin on the lateral axis, in millimeters |
<!-- /wikigen:table-metadata -->

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`84e42c1`](https://github.com/realsenseai/librealsense/commit/84e42c1d2ce717c57dcf11f155bbe68bb41bbd1c)._
<!-- /wikigen:footer -->
