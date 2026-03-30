# Tools for Intel® RealSense™ Camera

1. [RealSense-Viewer](./realsense-viewer) - This application allows you to quickly access your Intel® RealSense™ Depth cameras and modules.  It allows you to view the depth stream, record and playback streams, configure your camera and much more.
2. [Depth Quality Tool](./depth-quality) - Application that calculates and visualizes depth metrics to assess and characterize the quality of the depth data.
3. [Convert Tool](./convert) - Console application for converting recording files to various formats, including legacy `.bag` to `.db3` conversion
4. [Recorder](./recorder) - Simple command line data recorder (records to `.db3`)

### Debug Tools

3. [Enumerate-Devices](./enumerate-devices) - Console application providing information about connected devices
4. [Firmware-Logger](./fw-logger) - Console application for collecting internal camera logs.
5. [Data-Collect](./data-collect) - Console application capable of generating CSV report of frame statistics
6. [Terminal](./terminal) - Troubleshooting tool that sends commands to the camera firmware
7. [Recording Inspector](./rosbag-inspector) - For inspecting `.db3` recordings use any third-party application that supports `.db3` files (e.g. [Foxglove](https://foxglove.dev/)); for legacy `.bag` files use `rs-rosbag-inspector`
8. [dds-sniffer](./dds/dds-sniffer) - Console application providing information about active DDS domain entities
