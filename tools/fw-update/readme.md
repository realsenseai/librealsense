# rs-fw-update Tool

## Goal
`rs-fw-update` tool is a console application for updating the depth cameras firmware.

## Prerequisites
In order to update a depth camera firmware, a signed image file is required.
The latest firmware for D400 cameras is available [here.](https://dev.realsenseai.com/docs/firmware-releases-d400)
The firmware is packed into zip file and contains a file with "bin" extension with the following naming convension: "Signed_Image_UVC_<firmware_version>.bin"

## Usage
After installing `librealsense` run ` rs-fw-update -l` to launch the tool and print a list of connected devices.
An example of output for D415 and D585S cameras is:

```
Connected devices:
1) [USB] Intel RealSense D435IF s/n 038322070306, update serial number: 039223050231, firmware version: 5.15.1
2) [USB] Intel RealSense D585S s/n 353322320541, update serial number: 353322320541, firmware version: 0.5.8784.3406, SMCU firmware version: 0.0.2.0
```

Then we will provid the serial number to identify the device together with the path to firmware file that we want to update ` rs-fw-update -s 038322070306 -f Signed_Image_UVC_5_15_1_0.bin`.
An example for the expected output is:

```
Search for device with serial number: 038322070306

Updating device FW:
[USB] Intel RealSense D435IF s/n 038322070306, update serial number: 039223050231, firmware version: 5.15.1

Firmware update started. Please don't disconnect device!

Firmware update progress: 100[%]

Firmware update done

Waiting for device to reconnect...

Device 038322070306 successfully updated to FW: 5.15.1
```

In case only one camera is connected you can simply run ` rs-fw-update -f Signed_Image_UVC_5_15_1_0.bin`.

A camera/s might be in a recovery state, in such case listing the devices will output the following:

```
Connected devices:
1) [0ADB] Intel RealSense D4XX Recovery, update serial number: 039223050231, firmware version: 5.16.0.1
```

In such case we can use the recovery flag and run ` rs-fw-update -r -f Signed_Image_UVC_5_15_1_0.bin`
An example for the expected output is:

```
Update to FW: Signed_Image_UVC_5_15_1_0.bin

Recovering device:
[0ADB] Intel RealSense D4XX Recovery, update serial number: 039223050231, firmware version: 5.16.0.1

Firmware update started. Please don't disconnect device!

Firmware update progress: 100[%]

Firmware update done
Waiting for new device...

Recovery done

```

## Command Line Parameters

|Flag   |Description   |
|---|---|
|`--sw-only`|Show only software devices (playback, DDS, etc. -- but not USB/HID/etc.)|
|`-b`|Create a backup to the camera flash and saved it to the given path|
|`-s`|The serial number of the device to be update, this is mandetory if more than one device is connected|
|`-m`|Path of the safety MCU image file |
|`-f`|Path of the firmware image file|
|`-u`|Update unsigned firmware, available only for unlocked cameras|
|`-r`|Recover all connected devices which are in recovery mode|
|`-l`|List all available devices and exits|
|`--debug`|Turn on LibRS debug logs|
|`--`|Ignores the rest of the labeled arguments following this flag.|
|`--version`|Displays version information and exits|
|`-h`|Displays usage information and exits|
| None| List supported streaming modes|

## Limitation
* Do not run additional applications that use the RealSense camera, such as Viewer, together with the rs-fw-update tool
