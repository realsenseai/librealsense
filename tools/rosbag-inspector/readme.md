# Recording Inspector

## Inspecting `.db3` Recordings

The SDK now records to `.db3` (ROS2 rosbag2 format). To inspect these recordings, use any third-party application that supports `.db3` files, such as [Foxglove](https://foxglove.dev/).

Foxglove is also [recommended by the official ROS 2 documentation](https://docs.ros.org/en/rolling/Related-Projects/Visualizing-ROS-2-Data-With-Foxglove.html) for visualizing ROS 2 data.

### How to use

1. Download and install [Foxglove](https://foxglove.dev/download) (free for up to 3 users)
2. Open the application and select **"Open local file"**
3. Drag and drop your `.db3` file, or browse to it
4. Use the **Topics** panel to browse recorded topics and messages

> **Tip:** Foxglove is best used for inspecting metadata, topic structure, and info messages. Video playback of image streams may be slow — for frame-level playback, use the RealSense Viewer or the SDK's playback API instead.

## Legacy `.bag` Inspector

The `rs-rosbag-inspector` GUI tool can be used to inspect legacy ROS1 `.bag` files. See the [legacy documentation](./readme-legacy-ros1.md) for details.

To convert legacy `.bag` files to `.db3`, use:
```bash
rs-convert -i recording.bag -D recording.db3
```
