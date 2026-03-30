# rs-record Tool

## Overview

This tool is designed to collect raw sensor data to a `.db3` file (ROS2 rosbag2 format).

## Description
The tool records for a certain amount of time to a file as specified by the user.
The goal is to offer a command line recorder with low latency and zero frame drops.


## Command Line Parameters

|Flag   |Description   |Default|
|---|---|---|
|`-t X`|Stop recording after X seconds|10|
|`-f <filename>`|Save recording to `<filename>` (must have `.db3` extension)|(required)|

For example:
`rs-record -f ./test1.db3 -t 60`
will collect the data for 60 seconds.
The data will be saved to `./test1.db3`.

To inspect the recorded file, use any third-party application that supports `.db3` files, such as [Foxglove](https://foxglove.dev/) (see [Recording Inspector](../rosbag-inspector) for details).

# Recording file
The recorded `.db3` file can be replayed within librealsense using the playback API or the RealSense Viewer, and inspected using any third-party application that supports `.db3` files (e.g. [Foxglove](https://foxglove.dev/)) or standard ROS2 tools (`ros2 bag info`, `ros2 bag play`).

The `.db3` format is a SQLite database following the ROS2 rosbag2 storage format, using CDR serialization.
