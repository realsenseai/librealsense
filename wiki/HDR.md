HDR (high dynamic range) on D400 cameras makes the depth sensor alternate between two exposure/gain settings, frame after frame, so that one frame captures dark areas and the next captures bright areas. The SDK's `hdr_merge` processing block then combines each pair of frames into one depth frame with fewer holes. This page is for developers who want to configure HDR from code or from the RealSense Viewer and merge the resulting frames.

## Which devices support it

The HDR options are registered in [`src/ds/d400/d400-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-device.cpp) on the depth sensor of D400 devices when the firmware version is `5.12.8.100` or later. With older firmware the options are not registered, and `exposure`/`gain` stay plain sensor controls. The sequence frame metadata (`RS2_FRAME_METADATA_SEQUENCE_SIZE`, `RS2_FRAME_METADATA_SEQUENCE_ID`, `RS2_FRAME_METADATA_SEQUENCE_NAME`) is registered for D400 depth sensors under the same firmware condition. Check for support at runtime:

```cpp
rs2::depth_sensor depth_sensor = device.first< rs2::depth_sensor >();
if( ! depth_sensor.supports( RS2_OPTION_SEQUENCE_SIZE ) )
    std::cout << "HDR options are not available on this device/firmware\n";
```

The [rs-hdr example](https://github.com/realsenseai/librealsense/blob/development/examples/hdr/rs-hdr.cpp) uses the same test. It looks for a device whose `RS2_CAMERA_INFO_PRODUCT_LINE` is `D400`. If the device does not support `RS2_OPTION_SEQUENCE_SIZE`, it prints `Firmware and/or SDK versions must be updated for the HDR feature to be supported.`

The D500 series has a different feature, HDRD, which is a depth filter configured through an embedded filter. It is not covered on this page. See [[D500 Perception Features]].

## Configure and enable HDR

All HDR options live on the depth sensor. A configuration is a short sequence of frames, and each frame has its own exposure and gain. The steps below follow [`examples/hdr/rs-hdr.cpp`](https://github.com/realsenseai/librealsense/blob/development/examples/hdr/rs-hdr.cpp):

```cpp
rs2::depth_sensor depth_sensor = device.query_sensors().front();

// 1. sequence size - the registered option accepts only 2
depth_sensor.set_option(RS2_OPTION_SEQUENCE_SIZE, 2);

// 2. id of this HDR configuration, 0..3
depth_sensor.set_option(RS2_OPTION_SEQUENCE_NAME, 0);

// 3. select sequence item 1, then set its exposure and gain
depth_sensor.set_option(RS2_OPTION_SEQUENCE_ID, 1);
depth_sensor.set_option(RS2_OPTION_EXPOSURE, 8000);
depth_sensor.set_option(RS2_OPTION_GAIN, 25);

// 4. select sequence item 2, then set its exposure and gain
depth_sensor.set_option(RS2_OPTION_SEQUENCE_ID, 2);
depth_sensor.set_option(RS2_OPTION_EXPOSURE, 18);
depth_sensor.set_option(RS2_OPTION_GAIN, 16);

// 5. turn HDR on
depth_sensor.set_option(RS2_OPTION_HDR_ENABLED, 1);
```

How this works in [`src/hdr-config.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/hdr-config.cpp) and [`src/ds/ds-options.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/ds-options.cpp):

- **Exposure and gain are routed by `RS2_OPTION_SEQUENCE_ID`.** If the sequence ID is `1` or `2`, setting or querying `RS2_OPTION_EXPOSURE` and `RS2_OPTION_GAIN` reads or writes that slot of the HDR configuration. Their range is the same as the depth sensor's normal exposure and gain range. If the sequence ID is `0` (described as `UVC`), the calls go to the normal sensor controls.
- **Default slots.** If the device has no active HDR configuration, slot 1 gets exposure = default exposure minus 1000 and gain = default gain. Slot 2 gets the minimum exposure and the minimum gain. If HDR is already active in the device, the SDK reads the active exposure and gain values back from it.
- **Enabling.** If `RS2_OPTION_ENABLE_AUTO_EXPOSURE` or `RS2_OPTION_EMITTER_ON_OFF` is on when you set `RS2_OPTION_HDR_ENABLED` to `1`, the SDK turns it off. It also resets the sequence ID to `0` and sends the configuration to the device. If HDR is already enabled in the device, the request is skipped and a warning is logged (`HDR is already enabled. Skipping the request.`).
- **Disabling.** Setting `RS2_OPTION_HDR_ENABLED` to `0` sends an empty configuration and resets the sequence ID to `0`. It also turns auto-exposure and emitter on/off back on if the SDK turned them off.
- **Firmware older than `5.17.4.13`.** On these versions the SDK itself saves the manual exposure before enabling HDR and sets it back after disabling. While enabling, it first sets the exposure to `30000`. Newer firmware restores the exposure on its own.

### Auto-exposure and HDR

While HDR is enabled, auto-exposure cannot be turned on. On HDR-capable firmware, `RS2_OPTION_ENABLE_AUTO_EXPOSURE` is registered as a gated option ([`src/option.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/option.cpp)). While HDR is enabled, setting it does not throw. The SDK ignores the request and logs the warning `Auto Exposure cannot be set while HDR is enabled`. The example turns auto-exposure off itself before it sends the HDR configuration.

### Changing exposure or gain while HDR is running

- **Sequence ID is `0`.** Setting exposure or gain throws. The message is `The control - <description> - is locked while HDR mode is active.`
- **Sequence ID is `1` or `2`.** The SDK disables HDR, updates the slot, and enables HDR again. It keeps the selected sequence ID through this.

### Errors from the HDR options

| Situation | Message (from `src/hdr-config.cpp`) |
|---|---|
| Value outside the option range | `hdr_config::set(...) failed! value: <v> is out of the option range: [<min>, <max>].` |
| Sequence size other than 2 or 3 | `hdr_config::set_sequence_size(...) failed! Only size 2 or 3 are supported.` |
| Sequence ID above the sequence size | `hdr_config::set_sequence_index(...) failed! Index above sequence size.` |

`hdr_config` itself accepts a sequence size of 3, but the D400 device registers `RS2_OPTION_SEQUENCE_SIZE` with range 2..2, so only 2 can be set through the option. `hdr_merge` also processes only frames whose sequence size is 2.

## Merge the frames with hdr_merge

When HDR is on, every depth frame carries its sequence position in its metadata. `rs2::hdr_merge` ([`include/librealsense2/hpp/rs_processing.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_processing.hpp), [`src/proc/hdr-merge.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/hdr-merge.cpp)) works on framesets:

```cpp
rs2::config cfg;
cfg.enable_stream(RS2_STREAM_DEPTH);
cfg.enable_stream(RS2_STREAM_INFRARED, 1);   // optional, improves merging
rs2::pipeline pipe;
pipe.start(cfg);

rs2::hdr_merge merging_filter;
while (true)
{
    rs2::frameset data = pipe.wait_for_frames();
    rs2::frame merged = merging_filter.process(data);
    rs2::depth_frame depth = merged.as<rs2::frameset>().get_depth_frame();
    // ...
}
```

This is how the block behaves:

- **Input.** It accepts only framesets that contain a depth frame with `RS2_FRAME_METADATA_SEQUENCE_SIZE` and `RS2_FRAME_METADATA_SEQUENCE_ID` metadata, and the sequence size must be 2. Other input passes through unchanged. After 20 frames without the needed metadata it logs a warning once (the count starts again when the depth frame counter goes backwards, e.g. after a pipeline restart): `HDR Merge filter cannot process frames because relevant metadata params are missing`.
- **Pairing.** It stores the frameset of sequence ID 0 and then the one of sequence ID 1. The pair is merged only if the second depth frame counter is the first plus one and both frames have the same resolution.
- **Merging with infrared.** Infrared is used when both framesets have infrared frames, and these conditions hold:
  - the infrared frames have the same size as the depth frames
  - they support frame-counter and sequence-ID metadata
  - their frame counter and sequence ID match the depth frames
  - both infrared frames have the same format

  For each pixel, the merged value is the depth from the first frame if its infrared value is neither under- nor over-saturated and its depth is non-zero. Otherwise it is the second frame's depth under the same conditions, and otherwise `0`. For Y8, a usable value is strictly between 5 and 250. For Y16, it is strictly between 20 and 1003. With an infrared format other than Y8 or Y16, the block merges using depth only.
- **Merging with depth only.** Each pixel takes the first frame's depth if it is non-zero, otherwise the second frame's.
- **Output.** The block returns the latest merged frame, or the input frameset if no merged frame is saved. The saved frame is dropped if the input frame counter goes backwards, the resolution changes, or the input is 4 or more frames ahead of it.

The example notes that merging works with depth alone, but the result is better when infrared is streamed too.

## Show one sequence item with sequence_id_filter

To see the raw frames of a single exposure setting, use `rs2::sequence_id_filter` ([`src/proc/sequence-id-filter.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/sequence-id-filter.cpp)). It works on single frames, not framesets, and only on frames that have sequence metadata with a non-zero sequence size. It has one option, `RS2_OPTION_SEQUENCE_ID`: `0` means all frames, `1` or `2` selects that sequence item, and the default is `1`.

```cpp
rs2::sequence_id_filter seq_filter(1);          // keep only sequence item 1
rs2::frame filtered = seq_filter.process(depth_frame);
```

If a frame of the other sequence item arrives, the filter returns the last stored frame of the selected item from the same stream. If there is none yet, it returns the input frame. Note that the frame metadata `RS2_FRAME_METADATA_SEQUENCE_ID` starts at 0, while this filter's option counts items from 1.

`hdr_merge` and `sequence_id_filter` are both in the recommended processing blocks of D400 and D500 depth sensors (`get_ds_depth_recommended_proccesing_blocks()` in [`src/ds/ds-device-common.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/ds-device-common.cpp)). The Python wrapper exposes them as `rs.hdr_merge` and `rs.sequence_id_filter` ([`wrappers/python/pyrs_processing.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/pyrs_processing.cpp)). See [[Post Processing Filters]] for the other blocks.

## HDR in the RealSense Viewer

The Viewer ([[Realsense Viewer Manual]]) builds its post-processing list from the sensor's recommended filters ([`common/subdevice-model.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/subdevice-model.cpp)):

- **HDR Merge.** The block is added only if the sensor supports `RS2_OPTION_SEQUENCE_ID`.
- **Filter By Sequence id.** The block is added but starts disabled.

The HDR options themselves appear among the depth sensor's controls, like other options.

### HDR Config window

Some devices also get an **HDR Config** button next to the HDR enable control ([`common/option-model.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/option-model.cpp)). The button is drawn only when all of these hold:

- the device name contains `D45`
- the device supports advanced mode, and advanced mode is enabled ([[D400 Advanced Mode]])

The button opens the **HDR Configuration** window ([`common/hdr-model.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/hdr-model.cpp)). The window edits an `hdr-preset` section of the device JSON configuration, which it reads with `serialize_json()` and writes with `load_json()`. It offers:

- **Preset ID** and a list of **Preset Items**, added and removed with `+` and `-`. Each item has an **Iterations** count and its gain and exposure.
- **Auto HDR**. With it on, each item's gain and exposure are deltas applied to the auto-exposure values. With it off, they are absolute values.
- **Load defaults**, **Load from File** and **Save to File** (JSON files), plus **OK**, **Apply** and **Cancel**.

The default preset has two items. Manual gain/exposure values are 16/1 and 16/32000. Auto deltas are +30/+3000 and -30/-3000. When you apply a changed preset while HDR is enabled, the Viewer first sets `RS2_OPTION_HDR_ENABLED` to `0`.

## Reference

### Options

The HDR options in `rs2_option` order ([`include/librealsense2/h/rs_option.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_option.h)). Ranges and defaults are those registered by the D400 depth sensor.

<!-- wikigen:table-options -->
| Option | Meaning | Range | Default |
|---|---|---|---|
| `RS2_OPTION_HDR_ENABLED` | Enable / disable HDR | 0..1 | 0 |
| `RS2_OPTION_SEQUENCE_NAME` | HDR configuration id | 0..3 | 1 (range default) |
| `RS2_OPTION_SEQUENCE_SIZE` | Number of frames in the sequence | 2..2 | 2 |
| `RS2_OPTION_SEQUENCE_ID` | Sequence item that exposure/gain apply to; `0` = UVC (not HDR), items start at 1 | 0..2 | 0 |
<!-- /wikigen:table-options -->

The sequence name option reports range default 1, but the configuration itself starts with id 0.

### Frame metadata

These are the sequence metadata fields ([`include/librealsense2/h/rs_frame.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_frame.h)). For more on metadata, see [[Frame Metadata]].

<!-- wikigen:table-metadata -->
| Metadata | Meaning |
|---|---|
| `RS2_FRAME_METADATA_SEQUENCE_NAME` | sub-preset id |
| `RS2_FRAME_METADATA_SEQUENCE_ID` | sub-preset sequence id |
| `RS2_FRAME_METADATA_SEQUENCE_SIZE` | sub-preset sequence size |
<!-- /wikigen:table-metadata -->

### Source and examples

- [`examples/hdr`](https://github.com/realsenseai/librealsense/tree/development/examples/hdr): the `rs-hdr` example. It shows the input frames and the merged frame side by side and has sliders for exposure and gain. See also [[Code Samples]].
- [`src/hdr-config.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/hdr-config.cpp), [`src/hdr-config.h`](https://github.com/realsenseai/librealsense/blob/development/src/hdr-config.h): the HDR configuration.
- [`src/proc/hdr-merge.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/hdr-merge.cpp), [`src/proc/sequence-id-filter.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/sequence-id-filter.cpp): the processing blocks.

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`bc496cd`](https://github.com/realsenseai/librealsense/commit/bc496cdb16beca36c41d2b96170e309fceb38d6e)._
<!-- /wikigen:footer -->
