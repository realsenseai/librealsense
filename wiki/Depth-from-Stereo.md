Depth from stereo computes the distance to each pixel by matching points between a left and a right image and measuring their disparity. This concept page is for anyone new to stereo depth who wants to understand how RealSense stereo cameras produce depth and which SDK values describe the geometry.

## When you need it

- You want to understand block matching, calibration and rectification before working with depth data.
- You want to convert depth pixels to distances or 3D points in your own code.
- You compare passive stereo, active stereo (with a texture projector) and structured light.

## Key facts

- Each video stream profile has intrinsics (`rs2_intrinsics`: `width`, `height`, `ppx`, `ppy`, `fx`, `fy`, distortion `model` and `coeffs`), defined in [`include/librealsense2/h/rs_types.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/h/rs_types.h) and read with `rs2::video_stream_profile::get_intrinsics()`.
- Extrinsics between two streams (`rs2_extrinsics`: column-major 3x3 `rotation` and `translation` in meters, in [`include/librealsense2/h/rs_sensor.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/h/rs_sensor.h)) are read with `rs2::stream_profile::get_extrinsics_to()`.
- D400 and D500 depth sensors register `RS2_OPTION_STEREO_BASELINE`, the distance in mm between the stereo imagers.
- Depth frames hold values in depth units; `RS2_OPTION_DEPTH_UNITS` gives the number of meters per unit and `rs2::depth_sensor::get_depth_scale()` returns it. `rs2::depth_frame::get_distance(x, y)` returns the distance in meters directly.
- On D400 devices without advanced mode, the default depth unit is `0.001` m, and `0.0001` m for D405 and D401 GMSL ([`src/ds/d400/d400-device.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d400/d400-device.cpp)).
- [`include/librealsense2/rsutil.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/rsutil.h) provides `rs2_deproject_pixel_to_point()`, `rs2_project_point_to_pixel()` and `rs2_transform_point_to_point()` for moving between pixels, 3D points and streams.
- The projector is controlled with `RS2_OPTION_EMITTER_ENABLED` and `RS2_OPTION_LASER_POWER` on devices that register them; `rs2::disparity_transform` converts depth to disparity and back.

## Full guide

- Block matching, rectification, OpenCV software stereo, passive vs. active stereo and structured light: https://github.com/realsenseai/librealsense/blob/master/doc/depth-from-stereo.md

## Related pages

- [[Projection in RealSense SDK 2.0]]
- [[Post Processing Filters]]
- [[D400 Series Visual Presets]]
- [[Calibration]]
- [[Code Samples]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
