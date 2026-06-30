# rs-enumerate-devices Tool

## Goal
`rs-enumerate-devices` tool is a console application providing information about Realsense devices.

## Usage
After installing `librealsense` run ` rs-enumerate-devices` to launch the tool.
An example output for a D415 camera (using `-S` option) is:

```
Device Name                   Serial Number       Firmware Version
Intel RealSense D415          725112060411        05.12.02.100
Device info:
    Name                          :     Intel RealSense D415
    Serial Number                 :     725112060411
    Firmware Version              :     05.12.02.100
    Physical Port                 :     \\?\usb#vid_8086&pid_0ad3&mi_00#6&2a371216&0&0000#{e5323777-f976-4f5b-9b55-b94699c46e44}\global
    Debug Op Code                 :     15
    Advanced Mode                 :     YES
    Product Id                    :     0AD3
    Camera Locked                 :     NO
    Usb Type Descriptor           :     3.2
    Product Line                  :     D400
    Asic Serial Number            :     012345678901
    Firmware Update Id            :     012345678901

```

## Command Line Parameters

|Flag   |Description   |
|---|---|
|`-s`,`--short`|Generate a one-line info for each connected device|
|`-S`,`--compact`|Extends `-s` by providing a short summary per device|
|`-o`,`--option`|List supported device controls, options and streaming modes|
|`-c`,`--calib_data`|Provide calibration (Intrinsic/Extrinsic) and streaming modes information|
|`-p <path>`,`--playback_device <path>`|Enumerate streams contained in ROSBag record file|
|`-d`,`--defaults`|Show the default streams configuration|
|`--dds-domain <0-232>`|Set the DDS domain ID (default to 0)|
|`-v`,`--verbose`|Show extra information|
|`--debug`|Turn on LibRS debug logs|
|`--`,`--ignore_rest`|Ignores the rest of the labeled arguments following this flag|
|`--format <raw/basic/FULL>`|Choose which 'format-conversion' to use|
|`--sw-only`|Show only software devices: playback, DDS, etc. -- but not USB/HID/etc.|
| None| The default mode. Equivalent to `-S` plus the list of all the supported streaming profiles|

The options `-o`, `-c` and `-p` are additive and can be combined, e.g. call
`rs-enumerate-devices -o -c -p rosbag.rec` to print the camera info, streaming modes,  supported options and the calibration data both for the live cameras and the prerecorded `rosbag.rec` file.

The options `-S`, `-s` are restrictive and therefore incompatible with `-o` and `-c`,
but still can be used with `-p <file_name>`.
