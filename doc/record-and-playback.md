Realsense Record and Playback
===================


Overview
-------------
In addition to streaming video and other data from devices and sensors, the RealSense SDK provides the ability to record a live session of streaming to a file. The recorded files can later be loaded by the SDK to create a device with "read-only" abilities of the recorded device (we will explain what "read-only" abilities mean later on).

The SDK records a single device to a single `.db3` file using the [ROS2 rosbag2](https://github.com/ros2/rosbag2) storage format (SQLite-based). The output file path must end with the `.db3` extension. This allows files recorded by the SDK to be inspected using standard ROS2-compatible tools.

> For example recording files, please see [Sample Data](./sample-data.md)

> For the legacy ROS1 `.bag` format documentation, see [Record and Playback (Legacy ROS1)](./record-and-playback-legacy-ros1.md)


Quick Start
-------------

#### `rs2::recorder`

> :exclamation: If you are not familiar with the basic streaming [examples](../examples), please follow them before moving on

To enable recording of any device, simply create a **rs2::recorder** from it and provide a path to the desired output file:
```cpp
//Create a context and get the first device
rs2::context ctx;
auto devices = ctx.query_devices();
if (devices.size() > 0)
{
    //Create a rs2::recorder from the first device, and desired file name
    //The file must have a '.db3' extension (ROS2 rosbag2 format)
    rs2::recorder device("my_file_name.db3", devices[0]);
    //recorder "is a" device, so just use it like any other device now
}
```
A `recorder` has the same functionality as a "real" device, with additional control for recording, such as pausing and resuming record.


#### `rs2::playback`

> :exclamation: If you are not familiar with the basic streaming [examples](../examples), please follow them before moving on

Recorded files can be loaded and used to create a playback device by simply loading a file to the context:
```cpp
//Create a context
rs2::context ctx;
//Load the recorded file to the context
rs2::playback device = ctx.load_device("my_file_name.db3");
//playback "is a" device, so just use it like any other device now
```
The above code creates a playback device, which can be used as any device, but has the obvious limitation of only playing the recorded streams.
Playback devices can be used to query information on the device and its sensors, and can be extended to whichever extension the "real" device could.
A `playback` provides additional functionalities such as seek, pause, resume and playback speed.

### Using `rs2::config` with `rs2::pipeline`

The `rs2::pipeline` can be configured to record or play a streaming session by providing it with a `rs2::config` of your choice:

Recording to file:
```cpp
rs2::config cfg;
cfg.enable_record_to_file("path_to_output_file.db3");
rs2::pipeline pipe;
pipe.start(cfg); //File will be opened in write mode at this point
for (int i = 0; i < 30; i++)
{
    auto frames = pipe.wait_for_frames();
    //use frames here
}
pipe.stop(); //File will be closed at this point
```

Playing from file:
```cpp
rs2::config cfg;
cfg.enable_device_from_file("path_to_input_file.db3");
rs2::pipeline pipe;
pipe.start(cfg); //File will be opened in read mode at this point
for (int i = 0; i < 30; i++)
{
    rs2::frameset frames;
    if (pipe.poll_for_frames(&frames))
    {
        //use frames here
    }
    //Note that we use poll_for_frames instead of wait_for_frames
    //This is because the file only contains a finite amount of frames
    // and if we use wait_for_frames then after reading the entire file
    // we will fail to wait for the next frame (and get an exception)
}
pipe.stop(); //File will be closed at this point
```

Playback and Record in RealSense Viewer
-------------

Among its many features, the [RealSense Viewer](../tools/realsense-viewer) allows recording a device, and loading a file to playback.

To record a streaming session, simply click the "bars" icon next to the device name, choose "Record to File...", and select the destination for the file.

![Recording a device](./img/record_screenshot.png)

After choosing a file destination, a red dot will appear next to the device's name, indicating that it is recording.
Starting a stream will save its frames to the file, and once all streams are stopped, recording will complete automatically.

To replay a file, click "Add Source", choose "Load Recorded Sequence" and select the file you want to play.
Once you select the file, the Viewer will automatically add it to the list of sources, and a popup should appear stating that the file was loaded:

![Loading Recorded Sequence](./img/playback_screenshot.png)
>Notice that devices that were loaded from file have a "movie" icon next to their name.

After loading the file, you can start streaming its streams, view its controls (with the values at time of record), pause the playback, choose speed, and use the seek bar to navigate through frames.


Inspecting `.db3` Recordings
------

To inspect the contents of a `.db3` recording (topics, messages, metadata), use any third-party application that supports `.db3` files, such as [Foxglove](https://foxglove.dev/). Foxglove is also [recommended by the official ROS 2 documentation](https://docs.ros.org/en/rolling/Related-Projects/Visualizing-ROS-2-Data-With-Foxglove.html) for visualizing ROS 2 data.


For inspecting legacy `.bag` files, see the [ROS Bag Inspector](../tools/rosbag-inspector) tool.


Converting Legacy `.bag` Files to `.db3`
------

If you have existing `.bag` recordings in the legacy ROS1 format, you can convert them to `.db3`:

#### Using `rs-convert`:
```bash
rs-convert -i recording.bag -D recording.db3
```

#### Using the C++ API:
```cpp
rs2::context ctx;
ctx.convert_bag_to_db3("recording.bag", "recording.db3");
```

#### Using the Python API:
```python
import pyrealsense2 as rs
ctx = rs.context()
ctx.convert_bag_to_db3("recording.bag", "recording.db3")
```


Under the Hood
------

#### Basics Terminology

A **Device** is a container of Sensors with some correlation between them (e.g - all sensors are on a single board, sensors are mounted on a robot and share calibration information, etc.). A **Sensor** is a data streaming object, that provides one or more Streams.
**Stream** is a sequence of data items of a single data type, which are ordered according to their time of creation or arrival. The Sensor provides the Streams frames to the user.

We call the device's sensors and streams, the **topology** of the device.

Devices and Sensors can have **Extensions** that provide additional functionalities. A **Snapshot** of an Extension is a snapshot of the data that is available by the extension at some point of time, it is a sort of "read-only" version of the extension. For example, say we have a `DigitalClockExtension`, that can set and show the time. If we take a snapshot of that extension at noon, then whenever we ask the snapshot to show the time it will show "12:00", and trying to set its time will fail.

Finally, we will refer to an actual implementation of devices and sensors as "live" or "real" devices and sensors.

#### `.db3` File Format

The `.db3` file is a SQLite database following the [ROS2 rosbag2](https://github.com/ros2/rosbag2) storage format. Unlike the legacy ROS1 `.bag` format (which used a custom binary format with chunks and connections), the `.db3` format stores messages in SQLite tables, making it queryable with standard SQLite tools.

Messages are serialized using [CDR (Common Data Representation)](https://en.wikipedia.org/wiki/Common_Data_Representation) encoding via the [Fast-CDR](https://github.com/eProsima/Fast-CDR) library.

The same RealSense topics and message types are used as in the legacy format — device info, sensor info, stream info, image data, IMU data, metadata, options, extrinsics, and notifications. For a detailed list of topics and message types, see the [legacy format documentation](./record-and-playback-legacy-ros1.md#topics).

#### Dependencies

The SDK embeds the following third-party components under `third-party/realsense-file/rosbag2/` to provide `.db3` support. No external ROS2 installation is required. Note that `fastcdr` is fetched from GitHub at CMake configure time (requires internet access for first build).

**Core storage:**
- `rosbag2_storage` — Core rosbag2 storage interface and SQLite3-based `.db3` implementation
- `sqlite3` — SQLite database engine for the `.db3` file format
- `rosbag2_storage_default_plugins` — Default storage format plugins

**Plugin infrastructure:**
- `class_loader` — Dynamic plugin loading
- `pluginlib` — Plugin library framework
- `ament_index_cpp` — Package resource index for ROS2 plugin discovery

**Serialization:**
- `fastcdr` (v1.0.25) — CDR serialization of ROS2 message types (fetched from GitHub at build time)

**Utilities:**
- `rcutils`, `rcpputils` — ROS2 core C/C++ utilities
- `console_bridge` — Logging abstraction layer
- `yaml_cpp` — YAML configuration handling
- `tinyxml2` — XML parsing for plugin configurations
- `zstd` — Zstandard compression codec
- `rosbag2_compression` — Compression codec interface

> **Note:** All dependencies are embedded in the source tree under `third-party/`. See [NOTICE.md](../NOTICE.md) for license information.

#### Recording

Recording is performed at the Device level, meaning that the device, its sensors, their streams' data (frames) and all extensions are saved to file.
To allow for a seamless experience when switching between a live device and a record or playback device we save the device's topology and all of the extensions' snapshots to the file, in addition to the streaming frames.

A record device is like a wrapper around a real device, that delegates actions to the device and sensors while recording new data in between actions.
When a record device is created, a record sensor is created per real sensor of the real device.
A record sensor will record newly arriving frames for each of its streams, and changes to extensions' data (snapshots).

Recording related files are:
 - [record/record_device.cpp](../src/media/record/record_device.cpp)
 - [record/record_device.h](../src/media/record/record_device.h)
 - [record/record_sensor.cpp](../src/media/record/record_sensor.cpp)
 - [record/record_sensor.h](../src/media/record/record_sensor.h)
 - [ros2/ros2_writer.h](../src/media/ros2/ros2_writer.h)
 - [ros2/ros2_writer.cpp](../src/media/ros2/ros2_writer.cpp)

A `librealsense::record_device` is constructed with a "live" device and a `device_serializer::writer`. The `ros2_writer` writes device information to a `.db3` file using the rosbag2 storage backend.

When constructing a `ros2_writer` the requested file is created if it does not exist, and then opened for writing. In addition, a single message containing the realsense file format version is written to the file.

#### Playback

Playback device is an implementation of device interface which reads from a file to simulate a real device.
Playback device holds playback sensors which simulate real sensors.
When creating the playback device, it will read the initial device snapshot from the file in order to map itself and its sensors in matters of functionality and data provided.
When creating each sensor, the device will create a sensor from the sensor's initial snapshot.
Each sensor will hold a single thread for each of the sensor's streams which is used to raise frames to the user.
The playback device holds a single reading thread that reads the next frame in a loop and dispatches the frame to the relevant sensor.
The reading of the file, as well as each sensor's handling of frames, are done in separate threads. All this is managed via a common `dispatcher` concurrency mechanism: an `invoke()` call enqueues an `action` and is dequeued and run from a worker thread.

### Sequence Diagram
![playback](./img/playback/playback-flow.png)

*Note: this flow uses the sensor API; `librealsense2` supports playback with the pipeline API as well, which looks similar inside the playback device.*
*Created using  [DrawIO](https://app.diagrams.net/)*
