Post-processing filters are processing blocks that run on frames after they arrive from the camera. Most of them clean up depth data: they reduce resolution, cut off distant or close values, smooth noise, and fill holes. If your application uses depth from a RealSense stereo camera and you need cleaner depth or point clouds, apply these filters in your own code. The RealSense Viewer applies the same blocks from its Post-Processing panel (see [[Realsense Viewer Manual]]).

The SDK has two kinds of filters:

- **Host filters** run in librealsense on the host CPU. You create them with classes such as `rs2::decimation_filter` and call `process()` on each frame.
- **Embedded filters** run on the camera. You read and change their settings through the depth sensor; you do not call them on frames.

## Applying filters in C++

Every host filter derives from `rs2::filter`. Call `process()` with a frame and use the frame it returns as the input to the next filter. The [rs-post-processing](https://github.com/realsenseai/librealsense/blob/development/examples/post-processing/rs-post-processing.cpp) example does this on a background thread and shows a slider for each filter option. The code below is a shortened version of that example's processing loop:

```cpp
#include <librealsense2/rs.hpp>

rs2::pipeline pipe;
rs2::config cfg;
cfg.enable_stream(RS2_STREAM_DEPTH, 640, 0, RS2_FORMAT_Z16, 30);
pipe.start(cfg);

rs2::decimation_filter dec_filter;   // reduces depth frame density
rs2::threshold_filter thr_filter;    // removes values outside a distance range
rs2::disparity_transform depth_to_disparity(true);
rs2::spatial_filter spat_filter;     // edge-preserving spatial smoothing
rs2::temporal_filter temp_filter;    // reduces temporal noise
rs2::disparity_transform disparity_to_depth(false);

while (true)
{
    rs2::frameset data = pipe.wait_for_frames();
    rs2::frame filtered = data.get_depth_frame();

    filtered = dec_filter.process(filtered);
    filtered = thr_filter.process(filtered);
    filtered = depth_to_disparity.process(filtered);
    filtered = spat_filter.process(filtered);
    filtered = temp_filter.process(filtered);
    filtered = disparity_to_depth.process(filtered);
    // use filtered ...
}
```

You can also call `apply_filter()` on a frame, for example `data.get_depth_frame().apply_filter(dec_filter)`.

### Recommended order

The example applies its filters in this order. The comment in the example notes that each block is optional and can be applied on its own:

1. Decimation
2. Rotation
3. Threshold
4. Depth-to-disparity transform
5. Spatial
6. Temporal
7. Disparity-to-depth transform (only if step 4 was applied)

D400 and D500 depth sensors also return an ordered list of recommended blocks from `rs2::sensor::get_recommended_filters()`. This list is built in [ds-device-common.cpp](https://github.com/realsenseai/librealsense/blob/development/src/ds/ds-device-common.cpp) and [sensor.cpp](https://github.com/realsenseai/librealsense/blob/development/src/sensor.cpp). Its order is: decimation, rotation (set for depth and infrared), HDR merge, sequence ID filter, threshold, depth to disparity, spatial, temporal, hole filling, disparity to depth. On a D401 GMSL camera, and on a device with the D405 product ID that publishes a second color stream (Color 1), the D400 depth sensor adds the dual-RGB rectify filter to the end of the list (see [[D401 GMSL Dual RGB]]). The D405 case is *(changed in the next release)*: at v2.59.1 only the D401 GMSL camera added this filter.

Spatial and temporal filtering is usually done in the disparity domain. That is why the depth-to-disparity transform comes before these filters and the disparity-to-depth transform comes after them.

### Changing filter options

Filters derive from `rs2::options`, so the usual option calls work on them:

```cpp
rs2::spatial_filter spat_filter;
if (spat_filter.supports(RS2_OPTION_FILTER_SMOOTH_ALPHA))
{
    rs2::option_range r = spat_filter.get_option_range(RS2_OPTION_FILTER_SMOOTH_ALPHA);
    spat_filter.set_option(RS2_OPTION_FILTER_SMOOTH_ALPHA, 0.6f);
}
spat_filter.set_option(RS2_OPTION_HOLES_FILL, 2);  // 4-pixel radius
```

`get_option_value_description()` returns the label for an enumerated value, such as `"4-pixel radius"`. Several classes also have constructors that set options, for example `rs2::decimation_filter(float magnitude)`, `rs2::spatial_filter(float smooth_alpha, float smooth_delta, float magnitude, float hole_fill)`, `rs2::temporal_filter(float smooth_alpha, float smooth_delta, int persistence_control)` and `rs2::hole_filling_filter(int mode)`.

Some blocks inherit common options from their base class:

- Blocks built on the stream-filter base (decimation, threshold, rotation, spatial, temporal, hole filling, colorizer, point cloud, units transform, and the YUY, M420, NV12, MJPEG and Y411 decoders) have `RS2_OPTION_FRAMES_QUEUE_SIZE`, `RS2_OPTION_STREAM_FILTER`, `RS2_OPTION_STREAM_FORMAT_FILTER` and `RS2_OPTION_STREAM_INDEX_FILTER`. The last three select the frames the block acts on.
- The disparity transform removes `RS2_OPTION_FRAMES_QUEUE_SIZE`.

See [synthetic-stream.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/synthetic-stream.cpp).

## Host filter reference

The options, ranges and defaults below come from the constructors in [src/proc](https://github.com/realsenseai/librealsense/blob/development/src/proc). The C++ classes are declared in [rs_processing.hpp](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_processing.hpp), and the C functions that create them are in [rs_processing.h](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_processing.h).

<!-- wikigen:table-filters -->
| C++ class | C creator | Source | Purpose |
|---|---|---|---|
| `rs2::pointcloud` | `rs2_create_pointcloud` | [pointcloud.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/pointcloud.cpp) | Depth to 3D points with texture coordinates |
| `rs2::yuy_decoder` | `rs2_create_yuy_decoder` | [color-formats-converter.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/color-formats-converter.cpp) | YUY2 to RGB conversion |
| `rs2::m420_decoder` | `rs2_create_m420_decoder` | [color-formats-converter.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/color-formats-converter.cpp) | M420 to RGB conversion |
| `rs2::nv12_decoder` | `rs2_create_nv12_decoder` | [color-formats-converter.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/color-formats-converter.cpp) | NV12 to RGB conversion |
| `rs2::mjpeg_decoder` *(new in the next release)* | `rs2_create_mjpeg_decoder` | [color-formats-converter.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/color-formats-converter.cpp) | MJPEG to RGB8 conversion |
| `rs2::y411_decoder` | `rs2_create_y411_decoder` | [y411-converter.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/y411-converter.cpp) | Y411 to RGB conversion |
| `rs2::threshold_filter` | `rs2_create_threshold` | [threshold.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/threshold.cpp) | Drops depth outside a distance range |
| `rs2::units_transform` | `rs2_create_units_transform` | [units-transform.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/units-transform.cpp) | Depth to `RS2_FORMAT_DISTANCE` |
| `rs2::align` | `rs2_create_align` | [align.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/align.cpp) | Aligns frames to a chosen stream |
| `rs2::colorizer` | `rs2_create_colorizer` | [colorizer.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/colorizer.cpp) | Depth to a color image |
| `rs2::decimation_filter` | `rs2_create_decimation_filter_block` | [decimation-filter.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/decimation-filter.cpp) | Reduces resolution |
| `rs2::rotation_filter` | `rs2_create_rotation_filter_block` | [rotation-filter.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/rotation-filter.cpp) | Rotates frames in 90 degree steps |
| `rs2::temporal_filter` | `rs2_create_temporal_filter_block` | [temporal-filter.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/temporal-filter.cpp) | Smooths depth over time |
| `rs2::spatial_filter` | `rs2_create_spatial_filter_block` | [spatial-filter.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/spatial-filter.cpp) | Edge-preserving spatial smoothing |
| `rs2::disparity_transform` | `rs2_create_disparity_transform_block` | [disparity-transform.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/disparity-transform.cpp) | Depth to disparity and back |
| `rs2::depth_huffman_decoder` | `rs2_create_huffman_depth_decompress_block` | [include/librealsense2/hpp/rs_processing.hpp](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_processing.hpp) | Deprecated: the C creator throws a "deprecated" not-implemented error |
| `rs2::hole_filling_filter` | `rs2_create_hole_filling_filter_block` | [hole-filling-filter.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/hole-filling-filter.cpp) | Fills invalid depth pixels |
| `rs2::rates_printer` | `rs2_create_rates_printer_block` | [rates-printer.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/rates-printer.cpp) | Prints the frame rate of each stream |
| `rs2::hdr_merge` | `rs2_create_hdr_merge_processing_block` | [hdr-merge.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/hdr-merge.cpp) | Merges HDR frame sequences |
| `rs2::sequence_id_filter` | `rs2_create_sequence_id_filter` | [sequence-id-filter.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/sequence-id-filter.cpp) | Passes frames of one HDR sequence ID |
<!-- /wikigen:table-filters -->

### Decimation filter

`rs2::decimation_filter` reduces the width and height of a frame by the decimation scale. It is set up for Z16 depth by default, and the code also handles color and infrared formats such as YUYV, UYVY, RGB8, BGR8, RGBA8, BGRA8, Y8 and Y16. For Z16 depth it uses the median at scale 2 and 3, and the mean of the non-zero pixels at larger scales.

| Option | Range | Step | Default | Notes |
|---|---|---|---|---|
| `RS2_OPTION_FILTER_MAGNITUDE` | 1 – 8 | 1 | 2 | Decimation scale |

### Threshold filter

`rs2::threshold_filter` sets depth values outside the min/max distance range to zero.

| Option | Range | Step | Default | Notes |
|---|---|---|---|---|
| `RS2_OPTION_MIN_DISTANCE` | 0 – 16 | 0.1 | 0.1 | Meters. Closer depth is dropped |
| `RS2_OPTION_MAX_DISTANCE` | 0 – 16 | 0.1 | 4 | Meters. Farther depth is dropped |

The C++ constructor `threshold_filter(float min_dist = 0.15f, float max_dist = 4.f)` sets both options when it is created. As a result, an `rs2::threshold_filter` built with no arguments starts with a minimum of 0.15 m.

### Disparity transform

`rs2::disparity_transform(bool transform_to_disparity = true)` converts Z16 depth to disparity. With `false`, it converts disparity (`RS2_FORMAT_DISPARITY16` or `RS2_FORMAT_DISPARITY32`) back to depth. It has no filter options.

### Spatial filter

`rs2::spatial_filter` smooths depth between neighboring pixels and preserves edges. It can also fill holes.

| Option | Range | Step | Default | Notes |
|---|---|---|---|---|
| `RS2_OPTION_FILTER_SMOOTH_ALPHA` | 0.25 – 1 | 0.01 | 0.5 | Exponential moving average factor. 1 = no filter |
| `RS2_OPTION_FILTER_SMOOTH_DELTA` | 1 – 50 | 1 | 20 | Edge threshold, in depth levels |
| `RS2_OPTION_FILTER_MAGNITUDE` | 1 – 5 | 1 | 2 | Number of passes over the frame |
| `RS2_OPTION_HOLES_FILL` | 0 – 5 | 1 | 0 | Hole-filling radius, see below |

Values of `RS2_OPTION_HOLES_FILL`: 0 Disabled, 1 2-pixel radius, 2 4-pixel radius, 3 8-pixel radius, 4 16-pixel radius, 5 Unlimited.

### Temporal filter

`rs2::temporal_filter` smooths each pixel with its values in previous frames. It can also keep the last valid value of a pixel that is missing in the current frame.

| Option | Range | Step | Default | Notes |
|---|---|---|---|---|
| `RS2_OPTION_FILTER_SMOOTH_ALPHA` | 0 – 1 | 0.01 | 0.4 | Exponential moving average factor. 1 = no filter |
| `RS2_OPTION_FILTER_SMOOTH_DELTA` | 1 – 100 | 1 | 20 | Edge threshold, in depth increments |
| `RS2_OPTION_HOLES_FILL` | 0 – 8 | 1 | 3 | Persistency mode, see below |

Values of `RS2_OPTION_HOLES_FILL` (persistency): 0 Disabled, 1 Valid in 8/8, 2 Valid in 2/last 3, 3 Valid in 2/last 4, 4 Valid in 2/8, 5 Valid in 1/last 2, 6 Valid in 1/last 5, 7 Valid in 1/8, 8 Always on.

The filter keeps a separate history for each stream type and index. It clears that history when the frame geometry changes (for example after a rotation) or when the stream appears to restart. A restart means the timestamp goes back, or the gap since the last frame is longer than the 8-frame history *(changed in the next release)*. At v2.59.1 the filter cleared its history only when the frame size changed.

### Hole filling filter

`rs2::hole_filling_filter` fills invalid depth pixels from their neighbors.

| Option | Range | Step | Default | Notes |
|---|---|---|---|---|
| `RS2_OPTION_HOLES_FILL` | 0 – 2 | 1 | 1 | 0 Fill from Left, 1 Farest from around, 2 Nearest from around |

### Rotation filter

`rs2::rotation_filter` rotates frames. The default constructor rotates depth frames. To rotate other streams, pass them as `rotation_filter(std::vector<rs2_stream> streams_to_rotate)`, or use `rotation_filter(std::vector<rs2_stream> streams_to_rotate, float value)` to also set the angle.

| Option | Range | Step | Default | Notes |
|---|---|---|---|---|
| `RS2_OPTION_ROTATION` | -90 – 180 | 90 | 0 | Degrees. Positive = clockwise |

### HDR merge and sequence ID filter

`rs2::hdr_merge` merges the frames of an HDR sequence into one depth frame. It has no filter options. It needs the sequence frame metadata. When 20 frames arrive without that metadata, the library ([hdr-merge.cpp](https://github.com/realsenseai/librealsense/blob/development/src/proc/hdr-merge.cpp)) logs the warning "HDR Merge filter cannot process frames because relevant metadata params are missing". It logs it once, and again after the depth frame number starts over (a stream restart).

`rs2::sequence_id_filter` passes only the frames of one sequence ID. It can be set with the constructor `sequence_id_filter(float sequence_id)`.

| Option | Range | Step | Default | Notes |
|---|---|---|---|---|
| `RS2_OPTION_SEQUENCE_ID` | 0 – 2 | 1 | 1 | 0 = all, 1, 2 |

See [[HDR]] for how to configure HDR on the camera.

### Colorizer

`rs2::colorizer` turns a depth frame into a color image. `colorizer(float color_scheme)` sets the color map, and `colorize(frame depth)` is a shorthand for `process()`.

| Option | Range | Step | Default | Notes |
|---|---|---|---|---|
| `RS2_OPTION_MIN_DISTANCE` | 0 – 16 | 0.1 | 0 | Meters |
| `RS2_OPTION_MAX_DISTANCE` | 0 – 16 | 0.1 | 6 | Meters |
| `RS2_OPTION_COLOR_SCHEME` | 0 – 9 | 1 | 0 | 0 Jet, 1 Classic, 2 White to Black, 3 Black to White, 4 Bio, 5 Cold, 6 Warm, 7 Quantized, 8 Pattern, 9 Hue |
| `RS2_OPTION_VISUAL_PRESET` | 0 – 3 | 1 | 0 | 0 Dynamic, 1 Fixed, 2 Near, 3 Far |
| `RS2_OPTION_HISTOGRAM_EQUALIZATION_ENABLED` | 0 – 1 | 1 | 1 | Histogram equalization |

Setting the min or max distance turns histogram equalization off. Each preset sets several options at once:

- Dynamic: equalization on, Jet.
- Fixed: equalization off, Jet, 0–6 m.
- Near: equalization off, Classic, 0.3–1.5 m.
- Far: equalization off, Jet, 1–16 m.

### Point cloud

`rs2::pointcloud` computes 3D points from depth. It also computes texture coordinates for a stream you pass to `map_to()`. See [[Projection in RealSense SDK 2.0]].

| Option | Range | Step | Default | Notes |
|---|---|---|---|---|
| `RS2_OPTION_FILTER_MAGNITUDE` | 1 – 2 | 1 | 2 | Occlusion removal: 1 Off, 2 On |

### Align and units transform

`rs2::align(rs2_stream align_to)` maps the frames of a frameset to the viewpoint of the `align_to` stream. `rs2::units_transform` converts depth to `RS2_FORMAT_DISTANCE`. Neither block has filter options. See [[Projection in RealSense SDK 2.0]] for alignment.

## Embedded (on-camera) filters

Some cameras run filters in firmware. A depth sensor lists them with `rs2::sensor::query_embedded_filters()`. Each entry is an `rs2::embedded_filter`, and `get_type()` returns its `rs2_embedded_filter_type`:

| Type | C++ class | Extension |
|---|---|---|
| `RS2_EMBEDDED_FILTER_TYPE_DECIMATION` | `rs2::embedded_decimation_filter` | `RS2_EXTENSION_DECIMATION_EMBEDDED_FILTER` |
| `RS2_EMBEDDED_FILTER_TYPE_TEMPORAL` | `rs2::embedded_temporal_filter` | `RS2_EXTENSION_TEMPORAL_EMBEDDED_FILTER` |
| `RS2_EMBEDDED_FILTER_TYPE_CLOSE_RANGE` | `rs2::embedded_close_range_filter` | `RS2_EXTENSION_CLOSE_RANGE_EMBEDDED_FILTER` |

To get one filter by type, use `sensor.get_embedded_filter<T>()`. It throws `"Could not find requested embedded filter type!"` if the sensor does not have that filter. An embedded filter is an `rs2::options` container, so you use the normal option calls on it. It may also support composite options.

The [rs-embedded-filters](https://github.com/realsenseai/librealsense/blob/development/examples/embedded-filters/rs-embedded-filters.cpp) example (see also its [README](https://github.com/realsenseai/librealsense/blob/development/examples/embedded-filters/README.md)) shows both styles:

```cpp
auto depth_sensor = dev.first<rs2::depth_sensor>();
for (auto& filter : depth_sensor.query_embedded_filters())
    std::cout << rs2_embedded_filter_type_to_string(filter.get_type()) << std::endl;

auto dec_filter = depth_sensor.get_embedded_filter<rs2::embedded_decimation_filter>();
auto enabled = dec_filter.get_option(RS2_OPTION_EMBEDDED_FILTER_ENABLED);
dec_filter.set_option(RS2_OPTION_EMBEDDED_FILTER_ENABLED, 1);
```

### Scalar options (DDS cameras)

On DDS cameras ([[Ethernet and DDS Cameras]]), the SDK maps the camera's embedded-filter options to regular `rs2_option` values:

- Decimation: `RS2_OPTION_EMBEDDED_FILTER_ENABLED`, `RS2_OPTION_FILTER_MAGNITUDE`.
- Temporal: `RS2_OPTION_EMBEDDED_FILTER_ENABLED`, `RS2_OPTION_FILTER_SMOOTH_ALPHA`, `RS2_OPTION_FILTER_SMOOTH_DELTA`, `RS2_OPTION_HOLES_FILL`.
- Close range: `RS2_OPTION_EMBEDDED_FILTER_ENABLED`, `RS2_OPTION_DOWNSCALE_RATIO`, `RS2_OPTION_DISPARITY_SHIFT`, `RS2_OPTION_THRESHOLD`.

The camera reports the ranges. In the example, setting the decimation magnitude is expected to fail because the option is read-only.

### Composite options (D500 devices)

D500 depth sensors (except D585S) register the decimation, temporal and close-range embedded filters when the device firmware is recent enough, over USB and GMSL/MIPI *(changed in the next release)*. At v2.59.1, every D500 model except the D555, the D585S and the D585 with product ID 0x0B6A registered them only over USB (see [d500-device.cpp](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-device.cpp) and [d500-factory.cpp](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-factory.cpp)). These filters use composite options. A composite option is a struct that is read or written in one transaction. Composite options are declared in [rs_composite_option.h](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_composite_option.h):

| Composite option | Struct | Header |
|---|---|---|
| `RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP` | `rs2_decimation_filter_dpp_config` | [rs_decimation_filter_dpp.h](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_decimation_filter_dpp.h) |
| `RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP` | `rs2_temporal_filter_dpp_config` | [rs_temporal_filter_dpp.h](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_temporal_filter_dpp.h) |
| `RS2_COMPOSITE_OPTION_HDRD_CONTROL` | `rs2_hdrd_control` | [rs_hdrd_control.h](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_hdrd_control.h) |

Each struct starts with a `dpp_header` (see [rs_dpp_header.h](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_dpp_header.h)). The header has `version`, `flags`, `ctl_id`, `param_count` and `param_type`, followed by 8 `int32_t` parameter slots. The C++ typed calls (`get_composite_option_as()`, `get_composite_option_range_as()` and `set_composite_option_from()`) only accept `DPP_HEADER_CURRENT_VERSION` (1). The decimation, temporal and HDRD control structs are 38 bytes each. Unused `reserved` slots must be zero when you set the option. `rs2_get_composite_option_range()` returns the bounds as `rs2_decimation_filter_dpp_range`, `rs2_temporal_filter_dpp_range` or `rs2_hdrd_control_range`, each with `min`, `max`, `step` and `def` structs.

Decimation fields, as documented in the header:

| Field | Meaning |
|---|---|
| `enabled` | 0 = Off, 1 = On. Default 0. Can only be set while Depth/IR is not streaming |
| `magnitude` | Downscale factor, currently fixed at 2 |

Temporal fields, as documented in the header:

| Field | Meaning |
|---|---|
| `enabled` | 0 = Off, 1 = On. Default 0 |
| `smooth_alpha` | Alpha scaled to 0 – 1000. Default 400 (0.4) |
| `smooth_delta` | 1 – 100, step 1. Default 20 |
| `persistency_index` | 0 – 8, step 1. Default 3 |

In C++, read and write these options with `get_composite_option_as<T>()`, `set_composite_option_from()` and `supports_composite_option()` from [rs_options.hpp](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_options.hpp). The example does this for the close-range filter:

```cpp
const auto id = RS2_COMPOSITE_OPTION_HDRD_CONTROL;
if (filter.supports_composite_option(id))
{
    rs2_hdrd_control cfg = filter.get_composite_option_as<rs2_hdrd_control>(id);
    cfg.enable = 1;
    filter.set_composite_option_from(id, cfg);
}
```

`supports_composite_option()` only shows that the option is registered. The read can still fail on a device or firmware that does not support it, so the example catches `rs2::error` around `get_composite_option_as()`. For the close-range filter, see [[Improved Close Range Depth]]. For other D500 features, see [[D500 Perception Features]].

## See also

- [rs-post-processing example](https://github.com/realsenseai/librealsense/blob/development/examples/post-processing/rs-post-processing.cpp) and [its readme](https://github.com/realsenseai/librealsense/blob/development/examples/post-processing/readme.md)
- [doc/post-processing-filters.md](https://github.com/realsenseai/librealsense/blob/development/doc/post-processing-filters.md)
- [[API How To]], [[Code Samples]], [[GPU Acceleration]], [[Depth from Stereo]]

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`84e42c1`](https://github.com/realsenseai/librealsense/commit/84e42c1d2ce717c57dcf11f155bbe68bb41bbd1c)._
<!-- /wikigen:footer -->
