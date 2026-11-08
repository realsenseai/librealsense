RealSense SDK 2.0 can record a live streaming session from a device to a file, then load that file as a playback device that acts like the original camera, with read-only controls. This page is for developers who want to capture data for offline processing, testing or sharing, and for anyone who replays recordings in the SDK, the RealSense Viewer or ROS2 tools.

## When you need it

- You want to capture camera data once and process it again later without the camera connected.
- You need repeatable input for tests, bug reports or algorithm comparisons.
- You want to open recordings in ROS2 tools, or convert older ROS1 `.bag` recordings.
- You want to pause, seek or change playback speed while you inspect recorded frames.

## Key facts

- The pipeline records with `rs2::config::enable_record_to_file()` and plays back with `rs2::config::enable_device_from_file(file, repeat_playback)`. You cannot use both on the same config ([`include/librealsense2/hpp/rs_pipeline.hpp`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/hpp/rs_pipeline.hpp)).
- For low-level access, `rs2::recorder` wraps a live device and has `pause()` and `resume()`. An optional `compression_enabled` constructor argument turns compression on or off. `rs2::playback` adds `pause()`, `resume()`, `seek()`, `get_position()`, `get_duration()`, `set_real_time()`, `set_playback_speed()` and `set_status_changed_callback()` ([`include/librealsense2/hpp/rs_record_playback.hpp`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/hpp/rs_record_playback.hpp)).
- Playback status is reported as `rs2_playback_status`: `RS2_PLAYBACK_STATUS_UNKNOWN`, `PLAYING`, `PAUSED` or `STOPPED`. `STOPPED` is the initial status and is also reported when playback ends ([`include/librealsense2/h/rs_record_playback.h`](https://github.com/realsenseai/librealsense/blob/master/include/librealsense2/h/rs_record_playback.h)).
- The CMake option `BUILD_ROSBAG2` is `ON` by default ([`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/lrs_options.cmake)). With it on, recordings are written as ROS2 rosbag2 files and the path must end in `.db3` ([`src/media/ros2/ros2_writer.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/media/ros2/ros2_writer.cpp)). With it off, recordings are written as ROS1 bag files and `.db3` is rejected.
- On playback, a `.db3` file opens with a ROS2 reader. This covers both SDK-written files and native ROS2 bags. Any other file opens with the ROS1 `.bag` reader ([`src/media/ros_factory.cpp`](https://github.com/realsenseai/librealsense/blob/master/src/media/ros_factory.cpp)).
- To convert a ROS1 `.bag` recording to `.db3`, use `rs2::context::convert_bag_to_db3()`, which the Python wrapper also exposes, or the `rs-convert` tool. All of these require a build with `BUILD_ROSBAG2` on; otherwise the call throws. In such a build, the RealSense Viewer also offers to convert a `.bag` file when you load it ([`src/media/bag_to_db3_converter.h`](https://github.com/realsenseai/librealsense/blob/master/src/media/bag_to_db3_converter.h), [`tools/convert/rs-convert.cpp`](https://github.com/realsenseai/librealsense/blob/master/tools/convert/rs-convert.cpp)).
- [`examples/record-playback/rs-record-playback.cpp`](https://github.com/realsenseai/librealsense/blob/master/examples/record-playback/rs-record-playback.cpp) shows recording and playback from a pipeline.

## Full guide

- Recording, playback, file formats and conversion: https://github.com/realsenseai/librealsense/blob/master/doc/record-and-playback.md. Legacy ROS1 bag format: https://github.com/realsenseai/librealsense/blob/master/doc/record-and-playback-legacy-ros1.md. Sample recordings: https://github.com/realsenseai/librealsense/blob/master/doc/sample-data.md

## Related pages

- [[Realsense Viewer Manual]]
- [[Command Line Tools]]
- [[Code Samples]]
- [[API How To]]
- [[Python Wrapper]]
- [[Frame Metadata]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
