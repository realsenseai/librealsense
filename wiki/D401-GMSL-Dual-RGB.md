The D401 GMSL is a D400-family stereo camera with no separate RGB sensor: its two stereo imagers produce both the infrared and the color images. librealsense can give you color from it in two ways: a single firmware-processed (ISP) color stream, or a raw dual-RGB mode where the SDK debayers raw Bayer frames from both imagers on the host and exposes two color streams. Read this page if you stream color from a D401 GMSL and need to know which mode you get and which streams can run together.

## When you need it

- You want color and infrared from a D401 GMSL at the same time.
- You want two color streams (left and right imager) from a D401 GMSL.
- A stream combination with `Color 1` fails to resolve, or the Viewer greys out streams, and you want to know why.

## Key facts

- The SDK identifies the camera by product ID `0xABCC` (`RS401_GMSL_PID` in [`src/ds/d400/d400-private.h`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d400/d400-private.h)) with the name `RealSense D401`. The ID is assigned by the Linux MIPI backend in [`src/linux/camera-identifier-v4l.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/linux/camera-identifier-v4l.cpp).
- Depth, infrared and color are all served by the depth sensor; [`src/ds/d400/d400-color.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d400/d400-color.cpp) does not register a separate RGB sensor for this PID.
- Raw dual-RGB is set up only on firmware `5.17.4.13` or newer (checked in `d400-color.cpp` and [`src/ds/d400/d400-device.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d400/d400-device.cpp)). With older firmware only ISP color is available.
- The second color stream selects the mode: requesting `RS2_STREAM_COLOR` index 1 (`Color 1`) uses the raw `RGB8` debayer path for both color streams. The raw blocks are registered after the ISP blocks, so a single `Color` stream, in any format, stays ISP color.
- Raw frames arrive as `BA81` (`RS2_FORMAT_RAW8`) or `pBAA` (`RS2_FORMAT_RAW10`); the raw path outputs `RGB8` at 1280x720, 848x480, 640x480, 640x360, 480x270 and 424x240.
- For this PID the depth sensor's recommended processing blocks include the `RGB Rectification` filter (`dual_rgb_rectify_filter`, [`src/proc/dual-rgb-rectify-filter.h`](https://github.com/realsenseai/librealsense/blob/master/src/proc/dual-rgb-rectify-filter.h)), which rectifies the two color streams.
- The RealSense Viewer applies the color/infrared rules only for product ID `ABCC` when a second color stream is exposed (`is_dual_color_subdevice()` in [`common/subdevice-model.cpp`](https://github.com/realsenseai/librealsense/blob/master/common/subdevice-model.cpp)).
- When advanced mode is not available, the default depth unit for this PID is `0.0001` m, as for the D405 (`d400-device.cpp`).

## Full guide

- Modes, valid stream combinations, code examples, Viewer usage and limitations: https://github.com/realsenseai/librealsense/blob/master/doc/rs400/d401_dual_rgb_gmsl.md

## Related pages

- [[Supported Devices]]
- [[Installation Jetson]]
- [[Realsense Viewer Manual]]
- [[Post Processing Filters]]
- [[API How To]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
