Post-processing filters are librealsense processing blocks that take frames from the camera and produce new, filtered frames. Most of them improve depth data: they reduce resolution, drop values outside a distance range, smooth noise in space and time, and fill holes. This page is for developers who use depth from a RealSense stereo camera and want cleaner depth or point clouds in their own application.

## When you need it

- Your depth image or point cloud is noisy, has holes, or has more pixels than you need.
- You want to apply in your own code the same filters the RealSense Viewer offers in its Post-Processing panel (see [[Realsense Viewer Manual]]).
- You need to rotate, threshold, colorize, align or convert frames on the host.
- Your camera runs filters in firmware (embedded filters) and you want to read or change their settings.

## Key facts

- Host filters are C++ classes derived from `rs2::filter` in [`include/librealsense2/hpp/rs_processing.hpp`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/hpp/rs_processing.hpp), for example `rs2::decimation_filter`, `rs2::threshold_filter`, `rs2::disparity_transform`, `rs2::spatial_filter`, `rs2::temporal_filter`, `rs2::hole_filling_filter`, `rs2::rotation_filter`, `rs2::hdr_merge` and `rs2::sequence_id_filter`. The C creators (such as `rs2_create_decimation_filter_block`) are in [`rs_processing.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/h/rs_processing.h).
- Call `process()` on a filter, or `apply_filter()` on a frame, and pass the returned frame to the next filter to build a chain. Filters are also `rs2::options`, so `set_option()` and `get_option_range()` work on them.
- Defaults set in [`src/proc`](https://github.com/realsenseai/librealsense/blob/master/src/proc): decimation `RS2_OPTION_FILTER_MAGNITUDE` 2 (range 1 to 8); spatial smooth alpha 0.5, smooth delta 20, magnitude 2, hole filling disabled; temporal smooth alpha 0.4, smooth delta 20, persistency 3; hole filling filter mode "Farest from around".
- The C++ constructor `threshold_filter(float min_dist = 0.15f, float max_dist = 4.f)` sets the minimum and maximum distance in meters when it is created.
- D400 and D500 depth sensors return an ordered list from `rs2::sensor::get_recommended_filters()`: decimation, rotation, HDR merge, sequence ID filter, threshold, depth to disparity, spatial, temporal, hole filling, disparity to depth ([`src/ds/ds-device-common.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/ds-device-common.cpp)). On the D401 GMSL camera the D400 depth sensor adds the dual-RGB rectify filter at the end ([`src/ds/d400/d400-device.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/ds/d400/d400-device.cpp)). Spatial and temporal filtering therefore work on disparity data.
- Embedded filters run on the camera (D500 cameras and DDS cameras that expose them). A depth sensor lists them with `query_embedded_filters()`, and `get_embedded_filter<T>()` returns one by class (`rs2::embedded_decimation_filter`, `rs2::embedded_temporal_filter`, `rs2::embedded_close_range_filter`). It throws `Could not find requested embedded filter type!` ([`rs_sensor.hpp`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/hpp/rs_sensor.hpp)) if the sensor does not have that filter.
- On D500 cameras the embedded filters are configured through composite options declared in [`rs_composite_option.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/h/rs_composite_option.h), with the structs in [`rs_decimation_filter_dpp.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/h/rs_decimation_filter_dpp.h), [`rs_temporal_filter_dpp.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/h/rs_temporal_filter_dpp.h) and [`rs_hdrd_control.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/h/rs_hdrd_control.h) (close range).

## Full guide

- Filter descriptions, controls and API usage: https://github.com/realsenseai/librealsense/blob/master/doc/post-processing-filters.md
- Examples: [examples/post-processing](https://github.com/realsenseai/librealsense/blob/master/examples/post-processing/readme.md) (host filters) and [examples/embedded-filters](https://github.com/realsenseai/librealsense/blob/master/examples/embedded-filters/README.md) (on-camera filters).

## Related pages

- [[Realsense Viewer Manual]]
- [[Depth from Stereo]]
- [[Projection in RealSense SDK 2.0]]
- [[HDR]]
- [[Improved Close Range Depth]]
- [[D500 Perception Features]]
- [[Ethernet and DDS Cameras]]
- [[GPU Acceleration]]
- [[Code Samples]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
