Depth from stereo is the classic computer vision technique of computing distance by matching points between two images taken from parallel, horizontally offset viewpoints. This page explains the basic ideas (disparity, block matching, calibration and rectification, passive vs. active stereo) and shows how the corresponding quantities (depth units, stereo baseline, intrinsics, disparity) are exposed by the RealSense SDK. It is aimed at developers who want to understand where depth values come from and how to convert between pixels, disparity and metric distance.

## Why depth?

Regular web cameras produce RGB images in the visible spectrum. These can be used for object recognition, tracking and basic scene understanding, but recovering the exact physical dimensions of objects from colour images alone is hard.

A depth camera adds a new channel of information: the distance to the scene at every pixel. This channel can be used like any other image channel (for training and image processing), and also for measurement and 3D scene reconstruction.

## Stereoscopic vision

Depth from stereo is inspired by human [binocular vision](https://en.wikipedia.org/wiki/Binocular_vision). Two cameras look at the scene from parallel viewpoints separated by a known distance, the **baseline**. A point in the scene appears in both images, shifted horizontally by an amount called the **disparity**. The closer the point, the larger the disparity.

For a rectified pair of cameras with focal length `fx` (in pixels) and baseline `B`, depth and disparity are related by:

```
depth = fx * B / disparity
```

The same relation is stated in the SDK for the disparity pixel formats `RS2_FORMAT_DISPARITY16` and `RS2_FORMAT_DISPARITY32` in [`rs_sensor.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_sensor.h): *Disparity = Baseline\*FocalLength/Depth*.

### Block matching

The simplest way to estimate disparity is **SSD (Sum of Squared Differences) block matching**: for each pixel in the left image, take a small block around it and compare it with blocks in the right image shifted by each candidate disparity along the same row. The shift with the smallest difference is the disparity for that pixel.

The following NumPy sketch illustrates the idea. `left` and `right` are rectified grayscale images (2D arrays); `fx`, `baseline` and `units` are example values for illustration only.

```python
import numpy as np

fx = 640.0         # focal length in pixels (example value)
baseline = 50.0    # distance between the two cameras in mm (example value)
disparities = 64   # number of disparities to consider
block = 15         # half-size of the block to match
units = 0.001      # depth units: meters per depth unit

left = left.astype(np.float32)
right = right.astype(np.float32)
disparity = np.zeros(left.shape, dtype=np.float32)

for i in range(block, left.shape[0] - block - 1):
    for j in range(block + disparities, left.shape[1] - block - 1):
        ssd = np.empty(disparities)
        l = left[(i - block):(i + block), (j - block):(j + block)]
        # SSD at every candidate disparity
        for d in range(disparities):
            r = right[(i - block):(i + block), (j - d - block):(j - d + block)]
            ssd[d] = np.sum((l - r) ** 2)
        # select the best match
        disparity[i, j] = np.argmin(ssd)

# Convert disparity (pixels) to depth: baseline in mm -> meters, then to depth units
depth = np.zeros(left.shape, dtype=np.float32)
valid = disparity > 0
depth[valid] = (fx * baseline * 0.001) / (units * disparity[valid])
```

Any practical stereo system has to deal with a few challenges that this naive version ignores:

1. Making sure the two images really come from two parallel views (calibration and rectification).
2. Filtering out pixels where matching failed, for example because of occlusion.
3. Going beyond whole-pixel disparities to obtain sub-pixel accuracy.

## Calibration and rectification

In practice two cameras are never exactly parallel. The matching can be generalised to any two calibrated cameras by searching along [epipolar lines](https://en.wikipedia.org/wiki/Epipolar_geometry), but the more common approach is [image rectification](https://en.wikipedia.org/wiki/Image_rectification): both images are reprojected onto a common virtual image plane so that matching points lie on the same row.

Rectification needs the intrinsic parameters of each camera (focal length, principal point, distortion) and the extrinsic transformation between them. In the RealSense SDK these are available for every video stream through `rs2::video_stream_profile::get_intrinsics()` and `rs2::stream_profile::get_extrinsics_to()`. See [[Projection in RealSense SDK 2.0]] for details on intrinsics, extrinsics and distortion models.

## Software stereo with OpenCV

The [OpenCV](https://opencv.org/) library provides the building blocks for a software stereo pipeline:

1. `calibrateCamera` / `stereoCalibrate` to estimate intrinsic and extrinsic calibration.
2. `stereoRectify` to compute the rectification transforms.
3. `StereoBM` and `StereoSGBM` to compute disparity.
4. `reprojectImageTo3D` to project a disparity image to 3D space.

Refer to the [OpenCV calib3d documentation](https://docs.opencv.org/4.x/d9/d0c/group__calib3d.html) for the exact signatures.

## Passive vs. active stereo

The quality of stereo matching depends mostly on the density of visually distinguishable features in the scene. Flat, untextured surfaces (a white wall, for example) give the matcher nothing to lock on to. Any source of texture, natural or artificial, improves results.

This is why an optional **texture projector** is useful: it projects a pattern onto the scene, adding texture where there is none. Stereo that relies only on ambient texture is called *passive*; stereo assisted by a projector is called *active*.

On devices that provide a projector, the SDK exposes it through sensor options such as:

| Option | Description (from `rs_option.h`) |
|---|---|
| `RS2_OPTION_EMITTER_ENABLED` | Emitter select: 0 - disable all emitters. 1 - enable laser. 2 - enable auto laser. 3 - enable LED. |
| `RS2_OPTION_LASER_POWER` | Power of the laser emitter (mW), with 0 meaning projector turned off |

Whether an option is available, and its range, depends on the device; query it with `rs2::options::supports()` and `rs2::options::get_option_range()`.

## Structured light

[Structured light](https://en.wikipedia.org/wiki/Structured_light) is an alternative to depth from stereo. It recovers depth by recognising a known projected pattern in a single camera image, so it depends on that pattern being visible. Stereo matching, by contrast, works with any texture in the scene and can be used with or without a projector.

## Depth from stereo in the RealSense SDK

### Depth units

Depth frames (`RS2_FORMAT_Z16`) store 16-bit integer values. The pixel value multiplied by the **depth units** (depth scale) gives the distance in meters. The depth units are exposed as:

- the sensor option `RS2_OPTION_DEPTH_UNITS` ("Number of meters represented by a single depth unit");
- `rs2::depth_sensor::get_depth_scale()`;
- `rs2::depth_frame::get_units()` on each depth frame.

`rs2::depth_frame::get_distance(x, y)` returns the distance in meters at a pixel, already scaled.

On D400 devices the default is 0.001 (1 mm), except for the RealSense D405 and the GMSL RealSense D401, which use 0.0001 (0.1 mm). With advanced mode enabled (firmware 5.6.3.0 or later), the D400 depth units become writable (see [[D400 Advanced Mode]]). See [`d400-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-device.cpp).

### Stereo baseline

The distance between the two stereo imagers is exposed as:

- the sensor option `RS2_OPTION_STEREO_BASELINE` ("The distance in mm between the first and the second imagers in stereo-based depth cameras"), read-only;
- `rs2::depth_stereo_sensor::get_stereo_baseline()` (C: `rs2_get_stereo_baseline`), in millimeters;
- `rs2::disparity_frame::get_baseline()` on disparity frames.

The option is registered by the D400 and D500 depth sensors ([`d400-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d400/d400-device.cpp), [`d500-device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/ds/d500/d500-device.cpp)). For DDS cameras the SDK uses the option if the camera provides it, and otherwise falls back to the extrinsics between the two infrared streams ([`rs-dds-depth-sensor-proxy.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/dds/rs-dds-depth-sensor-proxy.cpp)). See [[Ethernet and DDS Cameras]].

### Depth to disparity and back

The `rs2::disparity_transform` processing block ([`rs_processing.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_processing.hpp)) converts between the two representations:

- `rs2::disparity_transform(true)` converts a `RS2_FORMAT_Z16` depth frame into a `RS2_FORMAT_DISPARITY32` disparity frame.
- `rs2::disparity_transform(false)` converts a disparity frame back to `RS2_FORMAT_Z16` depth.

The transform only converts frames from a stereo depth sensor (one that provides a stereo baseline). It uses the frame's focal length (`fx` from the stream intrinsics), the stereo baseline and the depth units. In the implementation ([`disparity-transform.h`](https://github.com/realsenseai/librealsense/blob/development/src/proc/disparity-transform.h)) the conversion factor also includes 5 fractional bits, so the output disparity values are scaled by 32:

```
factor    = baseline_m * fx * 32 / depth_units
disparity = factor / depth_raw
depth_raw = factor / disparity
```

The [post-processing example](https://github.com/realsenseai/librealsense/blob/development/examples/post-processing/rs-post-processing.cpp) optionally converts to disparity before the spatial and temporal filters and converts back afterwards. See [[Post Processing Filters]] for the full filter chain.

### From pixel and depth to a 3D point

Once you know the depth at a pixel, `rs2_deproject_pixel_to_point()` in [`rsutil.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/rsutil.h) computes the 3D point in the camera coordinate system using the stream intrinsics:

```cpp
void rs2_deproject_pixel_to_point(float point[3], const rs2_intrinsics* intrin, const float pixel[2], float depth);
```

The [measure example](https://github.com/realsenseai/librealsense/blob/development/examples/measure/rs-measure.cpp) uses it to measure the real-world distance between two pixels. See [[Projection in RealSense SDK 2.0]] for the projection model.

### Python example

The following snippet reads the depth units and stereo baseline, gets the distance at the center pixel and deprojects it to a 3D point. It uses only APIs bound in the [Python wrapper](https://github.com/realsenseai/librealsense/tree/development/wrappers/python) (see [[Python Wrapper]]).

```python
import pyrealsense2 as rs

pipe = rs.pipeline()
profile = pipe.start()
try:
    depth_sensor = profile.get_device().first_depth_sensor()
    print("Depth units (m):", depth_sensor.get_depth_scale())
    if depth_sensor.supports(rs.option.stereo_baseline):
        print("Stereo baseline (mm):", depth_sensor.get_option(rs.option.stereo_baseline))

    frames = pipe.wait_for_frames()
    depth = frames.get_depth_frame()
    x, y = depth.get_width() // 2, depth.get_height() // 2
    dist = depth.get_distance(x, y)

    intrin = depth.get_profile().as_video_stream_profile().get_intrinsics()
    point = rs.rs2_deproject_pixel_to_point(intrin, [x, y], dist)
    print("Distance (m):", dist, "3D point (m):", point)

    # Convert the depth frame to disparity and back
    to_disparity = rs.disparity_transform(True)
    to_depth = rs.disparity_transform(False)
    disparity = to_disparity.process(depth)
    depth_again = to_depth.process(disparity)
finally:
    pipe.stop()
```

## Related pages and examples

- [[Projection in RealSense SDK 2.0]]: intrinsics, extrinsics, projection and deprojection.
- [[Post Processing Filters]]: filters applied to depth, including the disparity transform.
- [[D400 Advanced Mode]]: depth units and other depth controls on D400 devices.
- [`examples/measure`](https://github.com/realsenseai/librealsense/tree/development/examples/measure): measuring distances using deprojection.
- [`examples/post-processing`](https://github.com/realsenseai/librealsense/tree/development/examples/post-processing): filter chain with depth/disparity conversion.
- [`wrappers/python/examples/opencv_pointcloud_viewer.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/opencv_pointcloud_viewer.py): Python point cloud viewer.

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`84e42c1`](https://github.com/realsenseai/librealsense/commit/84e42c1d2ce717c57dcf11f155bbe68bb41bbd1c)._
<!-- /wikigen:footer -->
