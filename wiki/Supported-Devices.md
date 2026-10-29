This page lists the RealSense cameras that librealsense recognizes, as the SDK code defines them: each product ID (PID), the name the SDK reports for it, its product line, and whether the SDK sets up a motion (IMU) sensor for it. It is for anyone who wants to check whether a camera is handled by the SDK, find out which device is connected, or work with the IMU on a D400 camera such as the D435i.

## See which device is connected

### With rs-enumerate-devices

[`rs-enumerate-devices`](https://github.com/realsenseai/librealsense/tree/development/tools/enumerate-devices) prints every connected device with its camera info fields (name, serial number, firmware version, product ID, product line and so on), followed by the sensors and stream profiles:

```
rs-enumerate-devices
```

Its flags (from [`rs-enumerate-devices.cpp`](https://github.com/realsenseai/librealsense/blob/development/tools/enumerate-devices/rs-enumerate-devices.cpp) and the shared [`common/cli.h`](https://github.com/realsenseai/librealsense/blob/development/common/cli.h)):

| Flag | What it does |
|---|---|
| `-s`, `--short` | One-line summary of the devices |
| `-S`, `--compact` | Short summary of the devices |
| `-o`, `--option` | Show all the supported options per subdevice |
| `-c`, `--calib_data` | Show extrinsic and intrinsic of all subdevices |
| `-d`, `--defaults` | Show the default streams configuration |
| `-v`, `--verbose` | Show extra information |
| `-p`, `--playback_device <path>` | Inspect a recorded file instead of a live device |
| `--sw-only` | Show only software devices (playback, DDS and so on), not USB/HID devices |
| `--format <raw/basic/FULL>` | Choose which format conversion to use (default `full`) |
| `--debug` | Turn on librealsense debug logs |
| `--eth` | Detect DDS devices, even if disabled in the configuration (ignored when not built with DDS support) |
| `--eth-only` | Use only DDS devices; do not look for USB/MIPI devices (ignored when not built with DDS support) |
| `--no-eth` | Do not detect DDS devices, even if enabled in the configuration |
| `--domain-id <0-232>` | Domain ID to use with DDS devices (ignored when not built with DDS support) |
| `-h`, `--help` | Show usage |
| `--version` | Show the version |

See [[Command Line Tools]] for the other tools.

### From code

Every device exposes its identity through `rs2::device::get_info()` with an `rs2_camera_info` value (declared in [`include/librealsense2/h/rs_sensor.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_sensor.h)):

```cpp
#include <librealsense2/rs.hpp>
#include <iostream>

int main()
{
    rs2::context ctx;
    for( auto && dev : ctx.query_devices() )
    {
        std::cout << dev.get_info( RS2_CAMERA_INFO_NAME ) << "  PID "
                  << dev.get_info( RS2_CAMERA_INFO_PRODUCT_ID );
        if( dev.supports( RS2_CAMERA_INFO_PRODUCT_LINE ) )
            std::cout << "  line " << dev.get_info( RS2_CAMERA_INFO_PRODUCT_LINE );
        std::cout << std::endl;
    }
}
```

What the D400 and D500 device code registers ([`src/ds/d400/d400-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-device.cpp), [`src/ds/d500/d500-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-device.cpp)):

| Info field | Value |
|---|---|
| `RS2_CAMERA_INFO_NAME` | The name from the PID-to-name map (the **Name** column below). A PID missing from the map is reported as `RS4xx` (D400) or `RS5xx` (D500). |
| `RS2_CAMERA_INFO_PRODUCT_ID` | The PID as upper-case hex, for example `0B3A` |
| `RS2_CAMERA_INFO_PRODUCT_LINE` | `D400` or `D500` |
| `RS2_CAMERA_INFO_CONNECTION_TYPE` | `USB`, or `GMSL` for MIPI/GMSL cameras |
| `RS2_CAMERA_INFO_IMU_TYPE` | IMU chip name (see [IMU notes](#imu-notes-for-the-d435i-and-other-d400-cameras)) |

A camera in recovery (DFU) mode shows up as a firmware-update device. Its name also comes from the same maps, or `unknown` if the PID is not in them ([`src/ds/d400/d400-fw-update-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-fw-update-device.cpp), [`src/ds/d500/d500-fw-update-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-fw-update-device.cpp)). See [[Firmware Update]].

### Limit enumeration to a product line

`rs2::context::query_devices()` takes a product-line mask from [`include/librealsense2/h/rs_context.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_context.h), for example `RS2_PRODUCT_LINE_D400`, `RS2_PRODUCT_LINE_D500` or `RS2_PRODUCT_LINE_ANY`. [`src/backend-device-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/backend-device-factory.cpp) only picks D400 devices when the mask includes `RS2_PRODUCT_LINE_D400`, and D500 devices when it includes `RS2_PRODUCT_LINE_D500`. USB recovery devices are matched to the D400 or D500 line by their PID. MIPI recovery devices are not filtered by the product line.

## How the SDK decides which cameras it supports

For USB and MIPI cameras, enumeration happens in [`src/backend-device-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/backend-device-factory.cpp):

1. **D400:** `d400_info::pick_d400_devices()` in [`src/ds/d400/d400-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-factory.cpp) keeps only video devices whose PID is in `rs400_sku_pid` ([`src/ds/d400/d400-private.h`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-private.h)). `d400_info::create_device()` then builds a device class for the PID.
2. **D500:** `d500_info::pick_d500_devices()` in [`src/ds/d500/d500-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-factory.cpp) does the same with `rs500_sku_pid` ([`src/ds/d500/d500-private.h`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-private.h)).
3. **Recovery (DFU):** `fw_update_info::pick_recovery_devices()` in [`src/fw-update/fw-update-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/fw-update/fw-update-factory.cpp) picks USB devices with a recovery PID, and MIPI devices in recovery.

If a PID is in the supported set but `create_device()` has no class for it, or creating the device fails, the factory falls back to a generic device with partial capabilities. It does this only when the `partial-device-allowed` context setting is true, which is the default ([`src/ds/ds-private.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/ds-private.cpp)). Otherwise it logs `Failed to create device for PID 0x…` and the error is thrown.

Network cameras are enumerated separately; see [[Ethernet and DDS Cameras]].

## Device reference

The table below has one row per entry of the PID-to-name maps, in code order:

- D400 rows: `rs400_sku_names` in [`src/ds/d400/d400-private.h`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-private.h). The PID constants are defined at the top of the same file.
- D500 rows: `rs500_sku_names` in [`src/ds/d500/d500-private.h`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-private.h).

How the columns are filled:

- **IMU**
  - **HID** means the PID is in `d400_hid_sensors_pid` and the device class includes the HID motion module (`d400_motion`).
  - **UVC** means the device class reads motion data through `d400_motion_uvc` (MIPI/GMSL cameras).
  - **Tracking module** means the PID is in `d400_fisheye_pid` and the class includes `d400_motion`.
  - **Yes** for D500 means the device class includes `d500_motion`.
  - Recovery PIDs have no streams.
- **Notes** names the class that `create_device()` builds ([`d400-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-factory.cpp), [`d500-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-factory.cpp)) or, for recovery PIDs, the update device ([`fw-update-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/fw-update/fw-update-factory.cpp)). MIPI/GMSL PIDs are the ones in `d400_mipi_device_pid`.

All PIDs, names and factory cases below are the same at v2.59.1.

<!-- wikigen:table-devices -->
| Name (as the code reports it) | PID | Product line | IMU | Notes |
|---|---|---|---|---|
| RealSense D400 | 0x0AD1 | D400 | No | `rs400_device` |
| RealSense D410 | 0x0AD2 | D400 | No | `rs410_device` |
| RealSense D415 | 0x0AD3 | D400 | No | `rs415_device` |
| RealSense D430 | 0x0AD4 | D400 | No | `rs430_device` |
| RealSense D430 with Tracking Module | 0x0AD5 | D400 | Tracking module | `rs430_mm_device` |
| RealSense USB2 | 0x0AD6 | D400 | No | `rs410_device` |
| RealSense D4XX Recovery | 0x0ADB | D400 | n/a | Recovery (DFU) PID; D400 update device |
| RealSense D4XX USB2 Recovery | 0x0ADC | D400 | n/a | Recovery (DFU) PID on USB2; D400 update device |
| RealSense IMU | 0x0AF2 | D400 | Motion module class | `rs400_imu_device` (`d400_motion`) |
| RealSense D420 | 0x0AF6 | D400 | No | `rs420_device` |
| RealSense D421 | 0x1155 | D400 | No | `rs421_device` |
| RealSense D420 with Tracking Module | 0x0AFE | D400 | Tracking module | `rs420_mm_device` |
| RealSense D410 with Tracking Module | 0x0AFF | D400 | No | In `rs400_sku_pid` and `d400_fisheye_pid`, but no case in `create_device()`; partial device fallback (`rs400_device`, no motion module) |
| RealSense D400 with Tracking Module | 0x0B00 | D400 | No | In `rs400_sku_pid` and `d400_fisheye_pid`, but no case in `create_device()`; partial device fallback (`rs400_device`, no motion module) |
| RealSense D430 with Tracking and RGB Modules | 0x0B01 | D400 | Tracking module | `rs430_rgb_mm_device` |
| RealSense D460 | 0x0B03 | D400 | No | `rs410_device` |
| RealSense D435 | 0x0B07 | D400 | No | `rs435_device` |
| RealSense D436 | 0x1156 | D400 | HID | `rs436_device` |
| RealSense DS5U | 0x0B0C | D400 | No | `rs405u_device` |
| RealSense D435I | 0x0B3A | D400 | HID | `rs435i_device` |
| RealSense F416 | 0x0B49 | D400 | No | `rs416_device` |
| RealSense D430I | 0x0B4B | D400 | HID | `rs430i_device` |
| RealSense F416 with RGB Module | 0x0B52 | D400 | No | `rs416_rgb_device` |
| RealSense D405 | 0x0B5B | D400 | No | `rs405_device` |
| RealSense D455 | 0x0B5C | D400 | HID | `rs455_device` |
| RealSense D457 | 0xABCD | D400 | UVC | `rs457_device`; MIPI/GMSL |
| RealSense D4XX MIPI Recovery | 0xBBCD | D400 | n/a | MIPI recovery (DFU); D400 update device |
| RealSense D430 | 0xABCE | D400 | UVC | `rs430_gmsl_device`; MIPI/GMSL |
| RealSense D415 | 0xABCF | D400 | No | `rs415_gmsl_device`; MIPI/GMSL |
| RealSense D401 | 0xABCC | D400 | No | `rs401_gmsl_device`; MIPI/GMSL, see [[D401 GMSL Dual RGB]] |
| RealSense D555 | 0x0B56 | D500 | Yes | `rs555_device`; also has Ethernet configuration (`eth_config_device`) |
| RealSense D555 Recovery | 0x0ADE | D500 | n/a | Recovery (DFU) PID; D500 update device |
| RealSense D585 | 0x0B6A | D500 | Yes | `rs585_legacy_device` (D585S without the safety features) |
| RealSense D585S | 0x0B6B | D500 | Yes | `rs585s_device`; adds the safety sensor |
| RealSense D585S Recovery | 0x0ADD | D500 | n/a | Recovery (DFU) PID; D500 update device |
| RealSense D500 Recovery | 0x0CFD | D500 | n/a | Recovery (DFU) PID shared across the D500 family; D500 update device |
| RealSense D500 Recovery | 0x0CFE | D500 | n/a | Recovery (DFU) PID for USB2 hosts; D500 update device |
| RealSense D5xx MIPI Recovery | 0xBBDD | D500 | n/a | MIPI recovery (DFU); D500 update device |
| RealSense D535 Dual RGB | 0x0C01 | D500 | Yes | `rs5x5_device` (dual RGB) |
| RealSense D535 | 0x0C02 | D500 | Yes | `rs5x5_dedicated_color_device` |
| RealSense D535F | 0x0C03 | D500 | Yes | `rs5x5_dedicated_color_device` |
| RealSense D585 Dual RGB | 0x0C04 | D500 | Yes | `rs5x5_device` (dual RGB) |
| RealSense D585 | 0x0C05 | D500 | Yes | `rs5x5_dedicated_color_device` |
| RealSense D585F | 0x0C06 | D500 | Yes | `rs5x5_dedicated_color_device` |
| RealSense D585 Proto Dual RGB | 0x0C07 | D500 | Yes | `rs5x5_device` (dual RGB) |
| RealSense D585 Prototype | 0x0C08 | D500 | Yes | `rs5x5_dedicated_color_device` |
<!-- /wikigen:table-devices -->

Some things to know when you read this table:

- Several names repeat for different PIDs. For example, `RealSense D430` is both 0x0AD4 (USB) and 0xABCE (GMSL), and `RealSense D585` is both 0x0B6A and 0x0C05. Use `RS2_CAMERA_INFO_PRODUCT_ID` and `RS2_CAMERA_INFO_CONNECTION_TYPE` to tell them apart.
- The D400 recovery PIDs and 0xBBCD are not in `rs400_sku_pid`. The D500 recovery PIDs are not in `rs500_sku_pid`. They are only picked up by the firmware-update path.
- The MIPI recovery PIDs are not read from a USB descriptor. On Linux, [`src/linux/v4l-enumerator-mipi.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/linux/v4l-enumerator-mipi.cpp) assigns 0xBBDD (D5xx) or 0xBBCD (D4xx) to a MIPI DFU device that reports `recovery`.
- The D400 code also keeps a minimum firmware version per PID (`d400_device_to_fw_min_version` in `d400-private.h`). The D500 equivalent is still commented out.

### D535/D585 dual RGB and dedicated color modes

The D535 and D585 PIDs come in pairs: a dual-RGB PID (built as `rs5x5_device`) and a dedicated-color-sensor PID (built as `rs5x5_dedicated_color_device`). `RS2_OPTION_SENSORS_CONFIG_MODE` on the depth sensor switches between them: `0` = Dedicated Color Sensor, `1` = Dual RGB. The option cannot be set while streaming. After you change it, call a hardware reset, and the device then enumerates under the other PID ([`include/librealsense2/h/rs_option.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_option.h)).

The option is registered for firmware `7.58.40897.13078` or newer, and only for the PIDs in `d5x5_dual_rgb_mode_pids` (0x0C01, 0x0C02, 0x0C04, 0x0C05, 0x0C07, 0x0C08) *(changed in the next release)*. At v2.59.1 it was registered for the whole D5x5 family, including D535F (0x0C03) and D585F (0x0C06).

See [[D500 Perception Features]] for the D500 features.

## IMU notes for the D435i and other D400 cameras

These notes are based on [`doc/d435i.md`](https://github.com/realsenseai/librealsense/blob/development/doc/d435i.md), limited to what the code confirms.

### IMU chip

The IMU chip is read from the device and reported in `RS2_CAMERA_INFO_IMU_TYPE` as `BMI055`, `BMI085`, `BMI088` or `IMU_Unknown` ([`src/ds/d400/d400-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-device.cpp)). If the device data does not identify the chip, the code falls back to the PID: D435I, D436, D430I and D455 (`d400_hid_bmi_055_pid`) are taken as BMI055.

### Coordinate system

On USB and MIPI connections, IMU samples are rotated into the depth sensor's coordinate system before the SDK delivers them. Each accel and gyro frame goes through `motion_transform`, which multiplies it by `imu_to_depth_alignment()` from the calibration handler ([`src/proc/motion-transform.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/motion-transform.cpp)). Over Ethernet (DDS), the combined motion stream is passed on unchanged. The depth-to-IMU extrinsics are registered for the accel and gyro streams, and both share the same extrinsics ([`src/ds/ds-motion-common.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/ds-motion-common.cpp)). See [[Projection in RealSense SDK 2.0]] for the axis convention.

![D400 depth sensor coordinate system](https://raw.githubusercontent.com/realsenseai/librealsense/development/doc/img/LRS_CS_axis_base.png)
### IMU calibration

When the motion module starts, the SDK reads the IMU calibration table from the device ([`src/ds/ds-calib-parsers.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/ds-calib-parsers.cpp)):

- If the table cannot be read, the library logs `IMU Calibration is not available, default intrinsic and extrinsic will be used.` and uses default values.
- If the table has no valid intrinsics, default intrinsics are used.
- If it has no valid extrinsics, the extrinsics come from mechanical CAD values (logged as `IMU extrinsic table not found; using CAD values`).

On USB and MIPI connections, the motion sensor exposes `RS2_OPTION_ENABLE_MOTION_CORRECTION` (on by default), which applies the intrinsics (sensitivity and bias) to the samples: the stored ones, or the defaults (scale 1, bias 0) when the table has none.

To calibrate the IMU and write the result to the camera, use [`tools/rs-imu-calibration/rs-imu-calibration.py`](https://github.com/realsenseai/librealsense/blob/development/tools/rs-imu-calibration/rs-imu-calibration.py). See [[Calibration]].

### Reading IMU data

Accel and gyro are ordinary streams (`RS2_STREAM_ACCEL`, `RS2_STREAM_GYRO`); Ethernet (DDS) devices instead give one `RS2_STREAM_MOTION` stream in `RS2_FORMAT_COMBINED_MOTION`. Enable them on an `rs2::config` and read `rs2::motion_frame::get_motion_data()` for accel and gyro, or `rs2::motion_frame::get_combined_motion_data()` for the combined motion stream. These programs work with IMU streams:

- [`examples/motion`](https://github.com/realsenseai/librealsense/tree/development/examples/motion) (`rs-motion`) is a complete example.
- [`rs-enumerate-devices`](https://github.com/realsenseai/librealsense/tree/development/tools/enumerate-devices) lists the motion stream profiles.
- [`rs-data-collect`](https://github.com/realsenseai/librealsense/tree/development/tools/data-collect) saves samples to a file.
- [[Realsense Viewer Manual]] covers viewing them in the RealSense Viewer.
- [[Record and Playback]] covers recording them.

## Platform and validation status

Which operating systems and hosts a camera works on is mostly a matter of validation, not code. The repository guide ([`doc/support-matrix.md`](https://github.com/realsenseai/librealsense/blob/development/doc/support-matrix.md)) says:

- Each firmware and librealsense release goes through unit tests and basic functional testing before it is published on the [releases page](https://github.com/realsenseai/librealsense/releases) as a pre-release.
- A release is promoted to latest after the full validation report is reviewed.
- Validation is done on x86 NUC machines running Windows 10 and Ubuntu 16.04 LTS.
- Other platforms are tested and supported on demand, driven by community requests.

One limit does come from the code: MIPI recovery-device detection lives in the Linux V4L2 backend ([`src/linux/`](https://github.com/realsenseai/librealsense/tree/development/src/linux)).

For per-platform setup, see [[Installation Linux]], [[Installation Windows]], [[Installation macOS]], [[Installation Jetson]], [[Installation Raspberry Pi]] and [[Android]].

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`e12faf3`](https://github.com/realsenseai/librealsense/commit/e12faf3cf8e19688f1ad2c832037c8a06f319429)._
<!-- /wikigen:footer -->
