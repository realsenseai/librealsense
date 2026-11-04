This page is a short orientation on which RealSense cameras librealsense recognizes and how to tell which one is connected. It is for anyone who wants to check whether a camera is handled by the SDK, identify a device from code or the command line, or work with the IMU on a D400 camera such as the D435i.

## When you need it

- You want to know whether the SDK enumerates a given D400 or D500 camera, and under which name and product ID (PID).
- You need to tell apart cameras that report the same name, for example a USB and a GMSL variant.
- You want to read IMU (accel and gyro) data from a D400 camera such as the D435i.
- You want to know on which platforms releases are validated.

## Key facts

- The SDK picks D400 USB and MIPI cameras only if their PID is in `rs400_sku_pid` ([`src/ds/d400/d400-private.h`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d400/d400-private.h)), and D500 cameras only if it is in `rs500_sku_pid` ([`src/ds/d500/d500-private.h`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d500/d500-private.h)). The names the SDK reports come from `rs400_sku_names` and `rs500_sku_names` in the same files; a PID missing from the map is reported as `RS4xx` or `RS5xx`.
- D400 and D500 USB and MIPI devices report `RS2_CAMERA_INFO_PRODUCT_LINE` as `D400` or `D500`, and `RS2_CAMERA_INFO_CONNECTION_TYPE` as `USB`, or `GMSL` for MIPI/GMSL cameras ([`src/ds/d400/d400-device.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d400/d400-device.cpp), [`src/ds/d500/d500-device.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d500/d500-device.cpp)); Ethernet (DDS) cameras report connection type `DDS` (see [[Ethernet and DDS Cameras]]). Some names repeat across PIDs (for example `RealSense D430` for both a USB and a GMSL PID), so use `RS2_CAMERA_INFO_PRODUCT_ID` and the connection type to tell them apart.
- `rs2::context::query_devices()` takes a product-line mask such as `RS2_PRODUCT_LINE_D400` or `RS2_PRODUCT_LINE_D500` ([`include/librealsense2/h/rs_context.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/h/rs_context.h)); [`src/backend-device-factory.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/backend-device-factory.cpp) only looks for D400 or D500 cameras when the mask includes that line.
- Recovery (DFU) PIDs such as `RealSense D4XX Recovery` are in the name maps but not in the supported PID sets; they are picked up only as firmware-update devices (see [[Firmware Update]]).
- If a supported PID has no device class or creating the device fails, the D400 and D500 factories fall back to a device with partial capabilities, as long as the `partial-device-allowed` context setting is true, which is the default ([`src/ds/ds-private.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/ds-private.cpp)). Otherwise the factory logs `Failed to create device for PID 0x…` ([`src/ds/d400/d400-factory.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d400/d400-factory.cpp), [`src/ds/d500/d500-factory.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d500/d500-factory.cpp)).
- D400 cameras with a HID IMU are the PIDs in `d400_hid_sensors_pid` (D435I, D436, D430I, D455). The IMU chip is reported in `RS2_CAMERA_INFO_IMU_TYPE` (for example `BMI055`, or `IMU_Unknown`).
- IMU samples are rotated into the depth sensor's coordinate system using `imu_to_depth_alignment()` ([`src/proc/motion-transform.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/proc/motion-transform.cpp)). If the IMU calibration table cannot be read, [`src/ds/ds-calib-parsers.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/ds-calib-parsers.cpp) logs `IMU Calibration is not available, default intrinsic and extrinsic will be used.`; [`tools/rs-imu-calibration/rs-imu-calibration.py`](https://github.com/realsenseai/librealsense/blob/master/tools/rs-imu-calibration/rs-imu-calibration.py) calibrates the IMU.
- Validation is done on x86 NUC machines running Windows 10 and Ubuntu 16.04 LTS; other platforms are tested and supported on demand (according to doc/support-matrix.md).

## Full guide

- Release process and validation platforms: https://github.com/realsenseai/librealsense/blob/master/doc/support-matrix.md
- D435i IMU guide: https://github.com/realsenseai/librealsense/blob/master/doc/d435i.md
- To list connected devices, run [`rs-enumerate-devices`](https://github.com/realsenseai/librealsense/tree/master/tools/enumerate-devices); for IMU streams see the [`examples/motion`](https://github.com/realsenseai/librealsense/tree/master/examples/motion) example.

## Related pages

- [[Command Line Tools]]
- [[Firmware Update]]
- [[Ethernet and DDS Cameras]]
- [[D500 Perception Features]]
- [[D401 GMSL Dual RGB]]
- [[Calibration]]
- [[Projection in RealSense SDK 2.0]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
