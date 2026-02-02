# RealSense&trade; D400 cameras with Raspberry Pi

This document describes how to use the RealSense D400 cameras on Raspberry PI 3 Model B (which offers only USB2 interface).

> **Note:** Camera capabilities are severely reduced when connected to USB2 port due to lack of bandwidth.<br />USB2 interface over D400 cameras is supported from FW 5.08.14.0 and on. Later firmware updates add more functionality, such as streaming reduced RGB video and additional Depth resolutions

<p align="center"><img src="https://raw.githubusercontent.com/wiki/realsenseai/librealsense/res/RaspberryPi3Model-B.png" /></p>

## Installation Instructions
1. Install [Ubuntu MATE](https://ubuntu-mate.org/) on your [Raspberry PI 3](https://www.raspberrypi.org/products/raspberry-pi-3-model-b/).

2. Clone and compile the latest [RealSense&trade; SDK 2.0](https://github.com/realsenseai/librealsense/releases/latest) by following the instructions under [Linux Installation](https://github.com/realsenseai/librealsense/blob/development/doc/installation.md).
In the section [Prerequisites](https://github.com/realsenseai/librealsense/blob/development/doc/installation.md##prerequisites), proceed with the steps till (not including) the kernel patches instructions.

> **Note**: In some cases the RAM capacity is not sufficient to compile the SDK, so if the compilation process crashes or exits with an error-code try to create a [swap file](https://www.howtoforge.com/ubuntu-swap-file) and then recompile the SDK.

3. [Activate](https://ubuntu-mate.community/t/tutorial-activate-opengl-driver-for-ubuntu-mate-16-04/7094) the OpenGL driver.
4. Plug the D400 camera and play with RealSense SDK [examples](https://github.com/realsenseai/librealsense/tree/master/examples)/[tools](https://github.com/realsenseai/librealsense/tree/master/tools).

## Expected Output
1. Streaming Depth data with 480x270 resolution and 30 FPS using the [RealSense Viewer](https://github.com/realsenseai/librealsense/tree/master/tools/realsense-viewer).
<p align="center"><img width=90% src="https://raw.githubusercontent.com/wiki/realsenseai/librealsense/res/Rasp3-Depth-Only.png" /></p>

2. Streaming Depth and IR with 424x240 resolution and 30 FPS:
<p align="center"><img width=90% src="https://raw.githubusercontent.com/wiki/realsenseai/librealsense/res/Rasp3-Depth-and-IR.png" /></p>

3. Point-cloud of depth and IR:
<p align="center"><img width=90% src="https://raw.githubusercontent.com/wiki/realsenseai/librealsense/res/Rasp3-Depth-and-IR-pointcloud.png" /></p>
