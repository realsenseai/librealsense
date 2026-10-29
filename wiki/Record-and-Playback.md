RealSense SDK 2.0 can record a live streaming session from a device to a file and later load that file as a "playback" device that behaves like the original camera, with read-only controls. This page is for developers who want to capture data for offline processing, testing or sharing, and for anyone who needs to replay recordings in the SDK, in the RealSense Viewer ([[Realsense Viewer Manual]]), or in ROS2 tools.

## Overview

- **Recording** wraps a live device in a record device. Everything the device does (its sensors, streams, extension snapshots such as options and info, and every frame) is written to one file.
- **Playback** creates a device from a file. It has the same sensors and extensions the recorded device had, with the values captured at record time, and it adds playback controls: pause, resume, seek, real-time mode and playback speed.
- Both work with the low-level device/sensor API (`rs2::recorder`, `rs2::playback`) and with the pipeline (`rs2::config::enable_record_to_file`, `rs2::config::enable_device_from_file`).

The recording format depends on the `BUILD_ROSBAG2` CMake option, which is `ON` by default (see [File formats](#file-formats)). With the default build, recordings are ROS2 rosbag2 `.db3` files; legacy ROS1 `.bag` files can still be played back and converted.

## Recording

### With the pipeline

Call `rs2::config::enable_record_to_file()` before starting the pipeline. The file is opened when the pipeline starts and closed when it stops.

```cpp
rs2::config cfg;
cfg.enable_record_to_file("recording.db3");
rs2::pipeline pipe;
pipe.start(cfg);            // file opened for writing here
for (int i = 0; i < 30; i++)
{
    auto frames = pipe.wait_for_frames();
    // use frames here
}
pipe.stop();                // file closed here
```

`enable_record_to_file()` and `enable_device_from_file()` cannot be used on the same `rs2::config`.

### With `rs2::recorder`

`rs2::recorder` is a device: create it from a live device and use it like any other device (query sensors, open and start streams). Frames and changes to the device's extensions are written while it streams.

```cpp
rs2::context ctx;
auto devices = ctx.query_devices();
if (devices.size() > 0)
{
    rs2::recorder rec("recording.db3", devices[0]);
    // rec "is a" device: use its sensors as usual
    rec.pause();    // stop writing without stopping the device from streaming
    rec.resume();   // continue writing
}
```

A third constructor argument, `recorder(file, dev, compression_enabled)`, turns compression on or off explicitly. Without it, the SDK uses the device's own default (`compress_while_record()` in the device implementation; the base device class returns `true`, some device classes override it to `false`). The pipeline also uses the device's default.

`rs2::recorder` also has `filename()`, and a pipeline-created recorder can be reached through `pipe.get_active_profile().get_device().as<rs2::recorder>()`, as the [record-playback example](https://github.com/realsenseai/librealsense/blob/development/examples/record-playback/rs-record-playback.cpp) does.

### File name rules

With the default build (`BUILD_ROSBAG2=ON`) every recording is written by the ROS2 writer, and the output path must end in `.db3`; otherwise the writer throws `Output file must have .db3 extension: '<file>'` ([`ros2_writer.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/media/ros2/ros2_writer.cpp)). An existing file with the same name is removed first.

With `BUILD_ROSBAG2=OFF`, recordings are written as ROS1 bag files, and a `.db3` path is rejected with `Cannot record to .db3 without BUILD_ROSBAG2` ([`ros_factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/media/ros_factory.cpp)).

## Playback

### With the pipeline

Call `rs2::config::enable_device_from_file()`:

```cpp
rs2::config cfg;
cfg.enable_device_from_file("recording.db3");
rs2::pipeline pipe;
pipe.start(cfg);            // file opened for reading here
for (int i = 0; i < 30; i++)
{
    rs2::frameset frames;
    if (pipe.poll_for_frames(&frames))
    {
        // use frames here
    }
}
pipe.stop();
```

The full signature is `enable_device_from_file(const std::string& file_name, bool repeat_playback = true)`. With `repeat_playback` set to `true` (the default) the pipeline starts the file again when it reaches the end. A file holds a finite number of frames, so with `repeat_playback = false` use `poll_for_frames()` or `try_wait_for_frames()` rather than `wait_for_frames()`, which throws when no frame arrives in time.

Any of the SDK [examples](https://github.com/realsenseai/librealsense/tree/development/examples) that use `rs2::pipeline` can run from a file this way.

### With `rs2::playback`

Load a file into a context to get a playback device:

```cpp
rs2::context ctx;
rs2::playback playback = ctx.load_device("recording.db3");
// playback "is a" device: query its sensors and stream them like a live device
```

`ctx.unload_device(file)` removes it again. A playback device created by the pipeline is available as `pipe.get_active_profile().get_device().as<rs2::playback>()`.

### Controlling playback

`rs2::playback` adds these controls (declared in [`rs_record_playback.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_record_playback.hpp)):

```cpp
rs2::playback playback = pipe.get_active_profile().get_device().as<rs2::playback>();

playback.pause();                                   // stop reading and raising frames
playback.resume();                                  // continue
auto total = playback.get_duration();               // std::chrono::nanoseconds
uint64_t pos = playback.get_position();             // nanoseconds from the start
playback.seek(std::chrono::nanoseconds(total / 2)); // jump to the middle (0 = start)
playback.set_real_time(false);                      // do not drop frames
playback.set_playback_speed(0.5f);                  // half speed
```

- **Real-time mode** (`set_real_time(true)`): frames are delivered at the pace they were recorded. If the application takes too long to handle a frame, frames may be dropped.
- **Non-real-time mode** (`set_real_time(false)`): playback waits for each callback to finish before reading the next frame. No frames are dropped and the application's processing time sets the pace.
- **Speed** (`set_playback_speed(speed)`): a multiplier, for example `1` is normal speed and `0.5` is half speed.
- `pause()` while paused does nothing; `resume()` while playing or stopped does nothing.
- `stop()` stops all streaming playback sensors and resets the playback.

### Playback status

`current_status()` returns an `rs2_playback_status`, and `set_status_changed_callback()` registers a callback that receives it on every change:

```cpp
playback.set_status_changed_callback([](rs2_playback_status status)
{
    if (status == RS2_PLAYBACK_STATUS_STOPPED)
        std::cout << "Playback stopped" << std::endl;
});
```

The callback runs on the playback reading thread, so heavy work in it can cause frame drops or latency.

| Value | Meaning (from `rs_record_playback.h`) |
|---|---|
| `RS2_PLAYBACK_STATUS_UNKNOWN` | Unknown state |
| `RS2_PLAYBACK_STATUS_PLAYING` | One or more sensors were started; playback is reading and raising data |
| `RS2_PLAYBACK_STATUS_PAUSED` | One or more sensors were started, but playback paused reading and raising data |
| `RS2_PLAYBACK_STATUS_STOPPED` | All sensors were stopped, or playback reached the end of the file. This is the initial status |

`rs2_playback_status_to_string()` converts a value to text.

### What a playback device can and cannot do

A playback device exposes the sensors, streams, info and options of the recorded device, with the values they had during recording. Changing them is not possible, and only the recorded streams can be played. Depth mapping parameters are not stored in recordings: calling get or set for them on a playback device throws `Depth mapping params are not available on a playback device` *(new in the next release)*.

## Record and playback in the RealSense Viewer

The RealSense Viewer ([[Realsense Viewer Manual]]) can record and play files:

- **Record**: while the device is streaming, use the **Record** button in the device panel. The Viewer uses a timestamped file name with the `.db3` extension (`.bag` when built with `BUILD_ROSBAG2=OFF`), either saved straight to the default folder or offered in a save dialog, depending on the Viewer's recording settings.
- **Play**: choose **Add Source** > **Load Recorded Sequence**, or drag and drop a file onto the Viewer. Playback devices offer pause, speed selection and a seek bar.
- **Legacy `.bag` files**: in a `BUILD_ROSBAG2` build, loading a `.bag` file opens a prompt saying ROS1 `.bag` recordings are deprecated, with **Convert**, **Play as-is** and **Cancel** buttons. **Convert** writes a `.db3` file next to the original and loads it.

See [`common/bag-conversion-helper.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/bag-conversion-helper.cpp) and [`common/device-model.cpp`](https://github.com/realsenseai/librealsense/blob/development/common/device-model.cpp).

## File formats

The reader and writer are chosen in [`src/media/ros_factory.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/media/ros_factory.cpp) by file extension and by the `BUILD_ROSBAG2` CMake option (defined in [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake), default `ON`).

| File | Read (playback) | Write (record) |
|---|---|---|
| `.db3` written by librealsense | Yes, with `BUILD_ROSBAG2=ON` (`ros2_reader`) | Yes, with `BUILD_ROSBAG2=ON` (`ros2_writer`) |
| `.db3` written by other ROS2 tools (for example `ros2 bag record`) | Yes, with `BUILD_ROSBAG2=ON` (`ros2_native_reader`) | No |
| ROS1 `.bag` (legacy) | Yes, in every build (`ros_reader`) | Only with `BUILD_ROSBAG2=OFF` (`ros_writer`) |

- Any file whose name does not end in `.db3` is opened with the ROS1 bag reader.
- Opening a `.db3` file without `BUILD_ROSBAG2` fails with `Cannot open .db3 files without BUILD_ROSBAG2`.
- A `.db3` file is treated as a librealsense recording when its first message is on the `/file_version` topic, which the librealsense writer always writes first. Any other `.db3` file goes to the native reader.
- The storage is the rosbag2 SQLite storage plugin. Other rosbag2 storage formats, such as MCAP, are not supported by the code.

### ROS2 `.db3` recordings

The `.db3` format is a SQLite database in the [ROS2 rosbag2](https://github.com/ros2/rosbag2) layout, with messages serialized in CDR. The rosbag2 components it needs are part of the source tree under `third-party/realsense-file/rosbag2/`; no ROS2 installation is required.

Per-stream topics use the prefix `/device_<N>/sensor_<N>/<Stream>_<index>` and include:

| Topic | Message type |
|---|---|
| `<prefix>/image/data` | `sensor_msgs/msg/Image` (uncompressed video frames) |
| `<prefix>/image/data/compressed` | `sensor_msgs/msg/CompressedImage` (compressed non-depth frames) |
| `<prefix>/image/data/compressedDepth` | `sensor_msgs/msg/CompressedImage` (compressed depth frames) |
| `<prefix>/image/camera_info` | `sensor_msgs/msg/CameraInfo` |
| `<prefix>/imu/data` | `sensor_msgs/msg/Imu` (motion streams) |
| `/tf_static` | `tf2_msgs/msg/TFMessage` (extrinsics) |

With compression on, frames are PNG-encoded where possible; other formats are compressed with zstd, and the `format` field of the message records the layout (`<format>; zstd; ...`). Recordings can be replayed with stock ROS2 tools such as `ros2 bag play` (according to doc/record-and-playback.md); to inspect them, the repository guide suggests third-party viewers that read `.db3`, such as [Foxglove](https://foxglove.dev/). Full details are in [`doc/record-and-playback.md`](https://github.com/realsenseai/librealsense/blob/development/doc/record-and-playback.md).

### Native ROS2 bags

The native reader plays `.db3` bags from other ROS2 tools, such as the RealSense ROS2 wrapper. It infers the stream type from the topic name (`color`/`rgb`, `depth`, `infra`, `accel`, `gyro`, `imu`/`motion`) and the pixel format from the image encoding. Topics it cannot map are skipped with a warning. See [`ros2_native_reader.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/media/ros2/ros2_native_reader.cpp).

### Legacy ROS1 `.bag` recordings

Older SDK releases recorded ROS1 [rosbag](http://wiki.ros.org/rosbag) files. They still play back, but the Viewer marks them as deprecated. The format is described in [`doc/record-and-playback-legacy-ros1.md`](https://github.com/realsenseai/librealsense/blob/development/doc/record-and-playback-legacy-ros1.md).

### Converting `.bag` to `.db3`

```cpp
rs2::context ctx;
ctx.convert_bag_to_db3("recording.bag", "recording.db3");
// optional progress callback: ctx.convert_bag_to_db3(in, out, [](float progress) { ... });
```

```python
import pyrealsense2 as rs
ctx = rs.context()
ctx.convert_bag_to_db3("recording.bag", "recording.db3")
```

From the command line: `rs-convert -i recording.bag -D recording.db3`. The C function is `rs2_convert_bag_to_db3()` in [`rs_context.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_context.h).

### Recorded stream types

Recordings store video, motion and the other stream types the device provides. The device-generated point cloud stream `RS2_STREAM_POINT_CLOUD` *(new in the next release)* is recorded and played back as a raw buffer on a `points` topic, in both the ROS2 and ROS1 writers and readers.

## Sample data

Sample recordings (outdoor scene, staircase, objects under water, a D435i sample with IMU) are listed in [`doc/sample-data.md`](https://github.com/realsenseai/librealsense/blob/development/doc/sample-data.md). They are legacy ROS1 `.bag` files and play directly; to get `.db3` files, convert them with `rs-convert -D` or the Viewer prompt. The repository guide notes that the files were captured with pre-production hardware and are not official marketing material.

## Tools and examples

| Program | What it does |
|---|---|
| [`rs-record-playback`](https://github.com/realsenseai/librealsense/tree/development/examples/record-playback) | Example: records the default streams to `a.db3` with the pipeline and shows depth, pauses and resumes recording, plays the file with a seek bar and pause. |
| [`rs-record`](https://github.com/realsenseai/librealsense/tree/development/tools/recorder) | Records the default pipeline streams to a file for a given time: `rs-record -f <path> -t <seconds>` (default 10 seconds). |
| [`rs-convert`](https://github.com/realsenseai/librealsense/tree/development/tools/convert) | Converts a recording to PNG, CSV, 3D CSV, RAW, PLY or BIN files, or converts a `.bag` file to `.db3` with `-D`. |
| [`rs-rosbag-inspector`](https://github.com/realsenseai/librealsense/tree/development/tools/rosbag-inspector) | GUI that shows the topics and messages in a legacy ROS1 `.bag` file. It does not open `.db3` files. |

Flags for these tools are listed in [[Command Line Tools]].

## How it works

- A **record device** ([`record_device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/media/record/record_device.cpp)) wraps a live device and creates a **record sensor** ([`record_sensor.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/media/record/record_sensor.cpp)) for each real sensor. Calls go to the real device; frames and extension changes go to a writer. The writer stores the device topology and extension snapshots first, so that playback can rebuild the device.
- A **playback device** ([`playback_device.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/media/playback/playback_device.cpp)) reads the initial device snapshot from the file and creates a **playback sensor** ([`playback_sensor.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/media/playback/playback_sensor.cpp)) per recorded sensor. One reading thread dispatches frames to the sensors, and each sensor raises them to the user from its own threads.
- Readers and writers are in [`src/media/ros2`](https://github.com/realsenseai/librealsense/tree/development/src/media/ros2) (`.db3`) and [`src/media/ros`](https://github.com/realsenseai/librealsense/tree/development/src/media/ros) (`.bag`). The `.bag`-to-`.db3` converter is [`bag_to_db3_converter.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/media/bag_to_db3_converter.cpp).

## API reference

### C++ (`rs_record_playback.hpp`, `rs_pipeline.hpp`, `rs_context.hpp`)

| API | Description |
|---|---|
| `rs2::recorder(const std::string& file, rs2::device dev)` | Record `dev` to `file` with the device's default compression |
| `rs2::recorder(const std::string& file, rs2::device dev, bool compression_enabled)` | Record with compression set explicitly |
| `recorder::pause()` / `recorder::resume()` | Pause or resume writing without stopping the device from streaming |
| `recorder::filename()` | Output file name |
| `rs2::context::load_device(const std::string& file)` | Create an `rs2::playback` from a file |
| `rs2::context::unload_device(const std::string& file)` | Remove a loaded playback device |
| `rs2::context::convert_bag_to_db3(input, output[, callback])` | Convert a legacy `.bag` file to `.db3`; optional progress callback |
| `rs2::config::enable_record_to_file(const std::string& file_name)` | Pipeline records to a file |
| `rs2::config::enable_device_from_file(const std::string& file_name, bool repeat_playback = true)` | Pipeline plays from a file |
| `playback::pause()` / `playback::resume()` | Pause or resume playback |
| `playback::stop()` | Stop all playback sensors and reset the playback |
| `playback::file_name()` | Name of the played file |
| `playback::get_position()` | Current position, in nanoseconds (`uint64_t`) |
| `playback::get_duration()` | Total duration (`std::chrono::nanoseconds`) |
| `playback::seek(std::chrono::nanoseconds time)` | Seek to a time point (0 = start) |
| `playback::is_real_time()` / `playback::set_real_time(bool)` | Query or set real-time mode |
| `playback::set_playback_speed(float speed)` | Set the speed multiplier |
| `playback::set_status_changed_callback(T callback)` | Callback receiving `rs2_playback_status` |
| `playback::current_status()` | Current `rs2_playback_status` |

### C (`rs_record_playback.h`)

<!-- wikigen:table-c-api -->
| Function | Description |
|---|---|
| `rs2_playback_status_to_string` | Text for an `rs2_playback_status` value |
| `rs2_create_record_device` | Create a record device with the device's default compression |
| `rs2_create_record_device_ex` | Create a record device with compression set explicitly |
| `rs2_record_device_pause` | Pause recording |
| `rs2_record_device_resume` | Resume recording |
| `rs2_record_device_filename` | Output file name of a record device |
| `rs2_create_playback_device` | Create a playback device from a file |
| `rs2_playback_device_get_file_path` | File path of a playback device |
| `rs2_playback_get_duration` | Total duration, in nanoseconds |
| `rs2_playback_seek` | Seek to a time, in nanoseconds |
| `rs2_playback_get_position` | Current position, in nanoseconds |
| `rs2_playback_device_resume` | Resume playback |
| `rs2_playback_device_pause` | Pause playback |
| `rs2_playback_device_set_real_time` | Set real-time mode |
| `rs2_playback_device_is_real_time` | Query real-time mode |
| `rs2_playback_device_set_status_changed_callback` | Register a status-change callback |
| `rs2_playback_device_get_current_status` | Current playback status |
| `rs2_playback_device_set_playback_speed` | Set the speed multiplier |
| `rs2_playback_device_stop` | Stop playback |
<!-- /wikigen:table-c-api -->

Header files: [`rs_record_playback.h`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/h/rs_record_playback.h), [`rs_record_playback.hpp`](https://github.com/realsenseai/librealsense/blob/development/include/librealsense2/hpp/rs_record_playback.hpp).

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`e12faf3`](https://github.com/realsenseai/librealsense/commit/e12faf3cf8e19688f1ad2c832037c8a06f319429)._
<!-- /wikigen:footer -->
