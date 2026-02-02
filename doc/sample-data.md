# Recorded Camera Data

**RealSense™ SDK 2.0** supports working with pre-recorded data ([learn more](./record-and-playback.md)).

All files below can be opened using the [RealSense Viewer](https://github.com/realsenseai/librealsense/releases/download/v2.57.4/RealSense.Viewer.exe) (`Add Source` > `Load Recorded Sequence` or just drag & drop into the Viewer) 
In addition, you can make any of the provided [examples](https://github.com/realsenseai/librealsense/tree/master/examples) run from file, by replacing `pipe.start()` with:
```cpp
rs2::config cfg;
cfg.enable_device_from_file(<filename>);
pipe.start(cfg); // Load from file
```
`*.bag` files contain uncompressed and unfiltered data and hence tend to be rather large (in the order of 100 MB per one second of recording). Our file-format is an extension of open [ROS-bag](http://wiki.ros.org/rosbag) format, meaning they can also be opened using existing robotics tools like [rqt-bag](http://wiki.ros.org/rqt_bag)


> **Disclaimer**: The files below are not official marketing material. These are captured using pre-production hardware with the sole purpose of helping developers, makers and students get started with RealSense technology

### Files

| []() | []() |
| ------------- |---------------|
| <a href="https://librealsense.realsenseai.com/rs-tests/TestData/outdoors.bag" title="Outdoors Stereo Example">![](https://librealsense.realsenseai.com/rs-tests/TestData/outdoor-preview.png)</a> | Outdoors scene captured with D415 pre-production sample (Depth from Stereo) |
| <a href="https://librealsense.realsenseai.com/rs-tests/TestData/stairs.bag" title="Staircase Example">![](https://librealsense.realsenseai.com/rs-tests/TestData/stairs-preview.png)</a> | Flight of stairs captured D435 pre-production sample (Depth from Stereo) |
| <a href="https://librealsense.realsenseai.com/rs-tests/TestData/depth_under_water.bag" title="D415 + Submerged objects">![](https://librealsense.realsenseai.com/rs-tests/TestData/depth_under_water.png)</a> | Scene captured with pre-production D415 device (Depth from Stereo) with objects submerged under water [![Binder](https://mybinder.org/badge.svg)](https://mybinder.org/v2/gh/realsenseai/librealsense/jupyter?filepath=notebooks/depth_under_water.ipynb) |
| <a href="https://librealsense.realsenseai.com/rs-tests/TestData/d435i_sample_data.zip" title="D435i_sample">![](https://librealsense.realsenseai.com/rs-tests/TestData/d435i_preview.png)</a> | Outdoor scene with D435i pre-production sample (Depth from Stereo with IMU) |
