The D401 GMSL is a D400-family stereo camera that connects over GMSL/MIPI and has no separate RGB sensor: its two stereo imagers produce both the infrared and the color images. librealsense offers two ways to get color from it: a single firmware-processed (ISP) color stream, or a "raw dual-RGB" mode where the SDK debayers raw Bayer frames from both imagers on the host and exposes two color streams. Read this page if you stream color from a D401 GMSL and need to know which stream combinations work, how to select a mode, and what the SDK does in each.

For connecting and installing GMSL/MIPI cameras, see [[Installation Jetson]] and [[Supported Devices]].

## How the SDK identifies the camera

| Item | Value in code |
|------|---------------|
| Product ID | `0xABCC` (`RS401_GMSL_PID` in [`d400-private.h`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-private.h)) |
| Name string | `RealSense D401` |
| Device class | `rs401_gmsl_device` (created in [`d400-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-factory.cpp)) |
| Minimum firmware listed for the PID | `5.17.2.2` |
| Device groups it belongs to | the supported D400 SKU, multi-sensor and MIPI device PID sets in `d400-private.h` |

The PID is assigned by the Linux MIPI backend: [`camera-identifier-v4l.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/linux/camera-identifier-v4l.cpp) reads the device's GVD and maps the D400 GMSL product byte `0x13` to PID `0xABCC` under vendor ID `0x8086`. The MIPI/GMSL path is part of the Linux backend (`src/linux/`).

`rs401_gmsl_device` derives from `d400_color`, `d400_mipi_device` and `firmware_logger_device`. Depth, infrared and color are all served by the depth sensor, named `Stereo Module`: the code in [`d400-color.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-color.cpp) treats the color endpoint as part of the depth unit for this PID instead of registering a separate RGB sensor.

## Choosing a mode

| Mode | What the SDK requests from the camera | Color streams | Infrared at the same time? |
|------|----------------------------------------|---------------|----------------------------|
| ISP color | `YUYV` from the firmware ISP, converted on the host | `Color` (index 0) | Yes |
| Raw dual-RGB | Raw Bayer (`RAW8` or `RAW10`) from both imagers, debayered on the host | `Color` (index 0) and `Color 1` (index 1), both `RGB8` | No |

**The second color stream selects the mode.** Enabling `Color 1` (stream `RS2_STREAM_COLOR`, index 1) selects raw dual-RGB. A single `Color` stream is ISP color in any format, including `RGB8`: the raw debayer blocks are registered after the ISP blocks, so the ISP conversion wins for a lone `Color` request and the raw path is used only when `Color 1` is also requested (see `d400_color::register_processing_blocks()`).

Raw dual-RGB is only set up when the firmware is `5.17.4.13` or newer (`d400_device::is_d401_raw_dual_rgb()` in [`d400-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-device.cpp)). With older firmware the raw-to-RGB8 blocks are not registered, so `Color 1` is not offered and only ISP color is available.

## Streaming raw dual-RGB from code

Request both color streams as `RGB8`. Depth may be added; infrared may not.

```cpp
rs2::config cfg;
cfg.enable_stream(RS2_STREAM_COLOR, 0, 848, 480, RS2_FORMAT_RGB8, 30); // Color   (left imager)
cfg.enable_stream(RS2_STREAM_COLOR, 1, 848, 480, RS2_FORMAT_RGB8, 30); // Color 1 (right imager)
cfg.enable_stream(RS2_STREAM_DEPTH, 848, 480, RS2_FORMAT_Z16, 30);     // optional

rs2::pipeline pipe;
pipe.start(cfg);
```

Requesting `Color 1` together with any infrared stream makes the depth sensor's `open()` throw:

```
Color 1 and Infrared cannot stream together: in raw dual-RGB mode both imagers output Bayer
```

This check is in `d400_depth_sensor::open()` in [`d400-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-device.cpp) *(new in the next release)*.

## Streaming ISP color with depth and infrared

A single `Color` stream runs alongside depth and both infrared streams:

```cpp
rs2::config cfg;
cfg.enable_stream(RS2_STREAM_DEPTH, 848, 480, RS2_FORMAT_Z16, 30);
cfg.enable_stream(RS2_STREAM_INFRARED, 1, 848, 480, RS2_FORMAT_Y8, 30);
cfg.enable_stream(RS2_STREAM_INFRARED, 2, 848, 480, RS2_FORMAT_Y8, 30);
cfg.enable_stream(RS2_STREAM_COLOR, 848, 480, RS2_FORMAT_BGR8, 30);   // ISP color

rs2::pipeline pipe;
pipe.start(cfg);
```

The default pipeline configuration of `rs401_gmsl_device` (no `rs2::config`) enables only `Color` index 0, `848x480` at 30 FPS, plus depth. The code comment explains why: `Color 1` shares a hardware endpoint with the right infrared stream, so enabling it by default would make the default configuration unresolvable.

For general pipeline usage see [[API How To]] and [[Code Samples]].

## Using the Viewer

The RealSense Viewer (see [[Realsense Viewer Manual]]) applies the mode rules when it sees a dual-color device. In [`common/subdevice-model.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/subdevice-model.cpp), `is_dual_color_subdevice()` is true for product ID `ABCC` (and `0B5B`, *new in the next release*) when the sensor exposes `Color`, a second color stream and depth or infrared. For such a sensor:

- Enabling `Color 1` turns off the infrared streams and switches `Color` to `RGB8` (raw dual-RGB).
- Enabling an infrared stream turns off `Color 1` (ISP color).
- Selecting a non-`RGB8` format for `Color` turns off `Color 1`.
- Infrared is locked while `Color 1` is enabled, and `Color 1` is locked while infrared is enabled. Depth and `Color` are never locked.

With firmware that does not expose `Color 1`, none of these rules apply.

## Rectification filter

For this PID the depth sensor's recommended processing blocks include `RGB Rectification` (`dual_rgb_rectify_filter`, [`src/proc/dual-rgb-rectify-filter.h`](https://github.com/realsenseai/librealsense/blob/development/src/proc/dual-rgb-rectify-filter.h)). It acts on `RGB8` color frames: once it has seen both color streams it builds stereo-rectification maps from their intrinsics and the left-to-right extrinsics, and outputs rectified images with matching rectified intrinsics. The header describes it as on by default and toggleable in the Viewer's post-processing list. See [[Post Processing Filters]].

## What the SDK does in raw dual-RGB mode

- **Pixel formats.** The depth sensor's fourcc maps (color is on the depth sensor for this PID) accept `BA81` (mapped to `RS2_FORMAT_RAW8`) and `pBAA` (mapped to `RS2_FORMAT_RAW10`) as color. Code comments tie `BA81` to older MIPI drivers and `pBAA` to MIPI driver `1.0.6.10` and newer.
- **Stream routing.** Both imagers arrive with the same raw fourcc. `resolve_d401_color_stream()` assigns the raw pin on the infrared node to `Color` (index 0, left imager) and the other to `Color 1` (index 1, right imager) *(changed in the next release: v2.59.1 ordered the raw pins by pin index only)*.
- **Debayer.** `rggb_converter` turns each raw frame into `RGB8`. The Bayer phase is treated as BGGR (`isp.swap_rb = true`; the comment names the OV9782 imager).
- **Resolutions.** The native color frame is `1288x808`. Each output resolution is produced by demosaicing the native image, center-cropping to the output aspect ratio and scaling. The intrinsics of each color profile are computed with the same crop and scale.
- **Frame numbers.** Software color frame numbers are enabled, because the two imagers share one hardware frame counter (without this, FPS reads double).
- **Extrinsics.** `Color` shares the pose of the left infrared stream and `Color 1` that of the right infrared stream, so the `Color` to `Color 1` extrinsics carry the stereo baseline.
- **Syncer.** `Color 1` is included in the device's frame matcher alongside depth, both infrared streams and `Color`.
- **Depth resolution.** `rs401_gmsl_device::contradicts()` never treats a color profile as conflicting on resolution with other streams, so color and depth can be requested at different resolutions. Depth and infrared are still checked against each other.

Resolutions registered for raw dual-RGB `RGB8` output, in code order:

<!-- wikigen:table-d401-raw-resolutions -->
| Width | Height |
|------:|-------:|
| 1280 | 720 |
| 848 | 480 |
| 640 | 480 |
| 640 | 360 |
| 480 | 270 |
| 424 | 240 |
<!-- /wikigen:table-d401-raw-resolutions -->

## What the SDK does in ISP color mode

- The camera delivers `YUYV`. The host converts it to `RGB8`, `RGBA8`, `BGR8` and `BGRA8` (`map_supported_color_formats()` in [`src/device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/device.cpp)).
- On a Jetson with a MIPI driver older than `1.0.4.9`, the SDK uses a `UYVY`-based workaround converter instead, because the bytes arrive swapped.
- Color frames get MIPI color metadata (frame timestamp, frame counter, sensor timestamp, brightness, contrast, saturation, sharpness and others), registered in `d400_color::register_metadata_mipi()`. See [[Frame Metadata]].

## Controls

All controls are on the `Stereo Module` sensor. The depth-sensor options are registered as for other D400 MIPI devices. Options specific to this PID in the code:

- **Depth units.** When advanced mode is not available, the default depth unit is `0.0001` m (the same as D405), not `0.001` m.
- **Readout shaping.** `RS2_OPTION_READOUT_SHAPING` is registered for this PID on firmware `5.17.3.13` and newer.
- **RGB controls.** For this PID, color controls go to the dedicated color V4L2 node (a node whose path contains `color`), not the depth node. When the firmware is `5.17.3.15` or newer and the MIPI driver is `1.0.4.9` or newer, the SDK registers `RS2_OPTION_SATURATION`, `RS2_OPTION_SHARPNESS`, `RS2_OPTION_WHITE_BALANCE`, `RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE` and `RS2_OPTION_POWER_LINE_FREQUENCY` on that node.
- **Not registered for this PID.** `RS2_OPTION_AUTO_EXPOSURE_PRIORITY` and the color auto-exposure ROI feature are not registered.

The `RS2_OPTION_DUAL_RGB_*` options in [`rs_option.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_option.h) belong to D500 dual-RGB devices. The D400 code does not register them for the D401 GMSL.

## D401 over USB

Over USB, a D401 enumerates with the D405 PID (`0x0B5B`). When that device exposes a `Color 1` stream, the same rules apply *(new in the next release)*: the raw routing, the `Color 1` and infrared exclusion, the crop-and-scale intrinsics, the rectification filter and the Viewer rules. The raw pins arrive with the `pRAA` fourcc (mapped to `RS2_FORMAT_RAW10`), and there is no firmware gate on this path.

## Limitations

- In raw dual-RGB mode, infrared and `Color 1` cannot stream together. Use a single `Color` stream if you need color with infrared.
- Raw dual-RGB needs firmware `5.17.4.13` or newer. Older firmware gives ISP color only.
- The MIPI/GMSL backend is in `src/linux/`, so the GMSL camera is supported through the Linux backend.

## Related source

- [`src/ds/d400/d400-factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-factory.cpp): `rs401_gmsl_device`, its default profiles and matcher
- [`src/ds/d400/d400-color.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-color.cpp): color endpoint, RGB controls, ISP and raw processing blocks
- [`src/ds/d400/d400-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-device.cpp): raw stream routing, firmware gate, color intrinsics, infrared exclusion
- [`src/proc/rggb-converter.h`](https://github.com/realsenseai/librealsense/blob/development/src/proc/rggb-converter.h) and [`src/proc/rggb-debayer.h`](https://github.com/realsenseai/librealsense/blob/development/src/proc/rggb-debayer.h): host debayer
- [`src/linux/camera-identifier-v4l.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/linux/camera-identifier-v4l.cpp): MIPI PID identification
- [`doc/rs400/d401_dual_rgb_gmsl.md`](https://github.com/realsenseai/librealsense/blob/development/doc/rs400/d401_dual_rgb_gmsl.md): repository guide with Viewer screenshots

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`bc496cd`](https://github.com/realsenseai/librealsense/commit/bc496cdb16beca36c41d2b96170e309fceb38d6e)._
<!-- /wikigen:footer -->
