# Sample Code for Intel® RealSense™ Python Wrapper

These Examples demonstrate how to use the python wrapper of the SDK.

## List of Examples:

1. [Tutorial 1](./python-tutorial-1-depth.py) - Demonstrates how to start streaming depth frames from the camera and display the image in the console as an ASCII art.
2. [NumPy and OpenCV](./opencv_viewer_example.py) - Example of rendering depth and color images using the help of OpenCV and Numpy
3. [Stream Alignment](./align-depth2color.py) - Demonstrate a way of performing background removal by aligning depth images to color images and performing simple calculation to strip the background.
4. [RS400 Advanced Mode](./python-rs400-advanced-mode-example.py) - Example of the advanced mode interface for controlling different options of the D400 cameras
5. [Realsense Backend](./pybackend_example_1_general.py) - Example of controlling devices using the backend interface
6. [Read bag file](./read_bag_example.py) - Example on how to read bag file and use colorizer to show recorded depth stream in jet colormap.
7. [Box Dimensioner Multicam](./box_dimensioner_multicam/box_dimensioner_multicam_demo.py) - Simple demonstration for calculating the length, width and height of an object using multiple cameras.
8. [Realsense over Ethernet](./ethernet_client_server/README.md) - This example shows how to stream depth data from RealSense depth cameras over ethernet.
9. [D400 self-calibration demo](./depth_auto_calibration_example.py) - Provides a reference implementation for D400 Self-Calibration Routines flow. The scripts performs On-Chip Calibration, followed by Focal-Length calibration and finally, the Tare Calibration sub-routines. Follow the [White Paper Link](https://dev.realsenseai.com/docs/self-calibration-for-depth-cameras) for in-depth description of the provided calibration methods.
10. [Numpy To Pyrealsense Frame](./numpy_to_pyrealsense_frame.py) - Example of how to convert a numpy array to a pyrealsense frame using a software device.
11. [Stream alignment using SW device](./align-with-software-device.py) - Demonstrates how to align depth and RGB images using a software device.
## Pointcloud Visualization

1. [OpenCV software renderer](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/opencv_pointcloud_viewer.py)
2. [PyGlet pointcloud renderer](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/pyglet_pointcloud_viewer.py) - requires `pip install pyglet`

## Interactive Examples:

1. [Distance to Object](https://github.com/realsenseai/librealsense/blob/jupyter/notebooks/distance_to_object.ipynb) [![Binder](https://mybinder.org/badge.svg)](https://mybinder.org/v2/gh/realsenseai/librealsense/jupyter?filepath=notebooks/distance_to_object.ipynb)
2. [Depth Filters](https://github.com/realsenseai/librealsense/blob/jupyter/notebooks/depth_filters.ipynb) [![Binder](https://mybinder.org/badge.svg)](https://mybinder.org/v2/gh/realsenseai/librealsense/jupyter?filepath=notebooks/depth_filters.ipynb)
