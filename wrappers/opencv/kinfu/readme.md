# rs-kinfu Sample


## Overview
KinectFusion is a real-time 3D reconstruction algorithm, which uses depth stream to estimate the camera motion. The current sensor pose is simultaneously obtained by tracking the live depth frame relative to the global model using a coarse-to-fine iterative closest point (ICP) algorithm, which uses all of the observed depth data available.

A detailed description of the algorithm can be found in the [full paper](https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/ismar2011.pdf).

This example utilizes the KinectFusion implementation available in `RGBD` module of `opencv_contrib`. For more details see the [official documentation](https://docs.opencv.org/trunk/d8/d1f/classcv_1_1kinfu_1_1KinFu.html).

## Installation
Install `OpenCV` and `opencv_contrib` as described in the [official guide](https://github.com/opencv/opencv_contrib/blob/master/README.md). Note that version 4 of OpenCV is required.

In step 5, along with configuring `OPENCV_EXTRA_MODULES_PATH`, check the `OPENCV_ENABLE_NONFREE` option.

For additional guidelines on building librealsene with OpenCV please follow [OpenCV Samples for Intel RealSense cameras](https://github.com/realsenseai/librealsense/blob/master/wrappers/opencv/readme.md). 

When configuring CMake for librealsense, in addition to checking `BUILD_CV_EXAMPLES`, check the `BUILD_CV_KINFU_EXAMPLE` flag. Also, ensure that `BUILD_GRAPHICAL_EXAMPLES` is checked.

## Expected Output
The application should open a window and display a pointcloud which grows according to camera movement, fusing new frames into the existing scene. 

The pointcloud is responsive to mouse movements and this way view from different perspectives is possible.

The following animation shows output retrieved using a D415 camera:

<p align="center"><img src="res/result.gif" /></p>

## Code Overview
First we declare pointers to the objects of `KinFu` and `Params`. We initialize `Params` with the default values, later on we'll change the relevant parameters manually.
```cpp
Ptr<KinFu> kf;
Ptr<Params> params = Params::defaultParams();
```

With D415, we use the preset `RS2_RS400_VISUAL_PRESET_HIGH_DENSITY` for optimization. Alternatively, `RS2_RS400_VISUAL_PRESET_HIGH_DENSITY` can be used in D435. Also, we define `depth_scale` which we'll use as a parameter for `KinFu`.
```cpp
for (rs2::sensor& sensor : dev.query_sensors())
{
    if (rs2::depth_sensor dpt = sensor.as<rs2::depth_sensor>())
    {
        // Set some presets for better results
        dpt.set_option(RS2_OPTION_VISUAL_PRESET, RS2_RS400_VISUAL_PRESET_HIGH_DENSITY);
        // Depth scale is needed for the kinfu set-up
        depth_scale = dpt.get_depth_scale();
        break;
    }
}
```

Initialize post-processing filters (with default configuration), we'll use them later to improve depth quality before passing it to `KinFu` as input.
```cpp
// Declare post-processing filters for better results
auto decimation = rs2::decimation_filter();
auto spatial = rs2::spatial_filter();
auto temporal = rs2::temporal_filter();
```

To avoid excessive noise from far away objects (that are not part of the scene we are interested in), we declare `clipping_dist` which sets the maximal depth to consider. Adjust `max_dist` according to your needs.
```cpp
auto clipping_dist = max_dist;
```

Acquire frame size and camera intrinsics which will be used as parameters for `KinFu`.
```cpp
auto intrin = stream_depth.as<rs2::video_stream_profile>().get_intrinsics();
// Use decimation once to get the final size of the frame
d = decimation.process(d);
auto w = d.get_width();
auto h = d.get_height();
Size size = Size(w, h);
auto intrin = stream_depth.as<rs2::video_stream_profile>().get_intrinsics();
```

Set frame size, intrinsics and depth factor using the values we previously found. Now `params` is ready to initialize a new KinFu object.
```cpp
// Configure kinfu's parameters
params->frameSize = size;
params->intr = Matx33f(intrin.fx, 0, intrin.ppx,
                       0, intrin.fy, intrin.ppy,
                       0, 0, 1);
params->depthFactor = 1 / depth_scale;

// Initialize KinFu object
kf = KinFu::create(params);
```

Set a boolean to indicate whether it is safe to acquire the current pointcloud.
```cpp
bool after_reset = false;
```

`calc_cloud_thread` handles the post processing of the depth frames, as well as running KinFu and obtaining the pointcloud.
First, post-processing filters are applied.
```cpp
// Use post processing to improve results
d = decimation.process(d);
d = spatial.process(d);
d = temporal.process(d);
```

As mentioned earlier, we discard pixels with depth higher than `clipping_dist`. This might help the algorithm focus on objects within the relevant range.
```cpp
// Set depth values higher than clipping_dist to 0, to avoid unnecessary noise in the pointcloud
#pragma omp parallel for schedule(dynamic) //Using OpenMP to try to parallelise the loop
uint16_t* p_depth_frame = reinterpret_cast<uint16_t*>(const_cast<void*>(d.get_data()));
for (int y = 0; y < h; y++)
{
    auto depth_pixel_index = y * w;
    for (int x = 0; x < w; x++, ++depth_pixel_index)
    {
        // Get the depth value of the current pixel
        auto pixels_distance = depth_scale * p_depth_frame[depth_pixel_index];

        // Check if the depth value is invalid (<=0) or greater than the threshold
        if (pixels_distance <= 0.f || pixels_distance > clipping_dist)
        {
            p_depth_frame[depth_pixel_index] = 0;
        }
    }
}
```

`KinFu` is able to run on the GPU to improve performance. Since we perform depth processing and rendering on the CPU, copying frames from GPU to CPU and vice versa is required.
Running on CPU alone using OpenCV's `Mat` instead of `UMat` is also possible.
```cpp
// Define matrices on the GPU for KinFu's use
UMat points;
UMat normals;
// Copy frame from CPU to GPU
Mat f(h, w, CV_16UC1, (void*)d.get_data());
UMat frame(h, w, CV_16UC1);
f.copyTo(frame);
f.release();
```

Here we actually run `KinFu`. The algorithm attemtps to fuse the current depth frame with the existing scene. If it fails, the model obtained so far is reset and reconstruction of a new scene begins.
```cpp
// Run KinFu on the new frame(on GPU)
if (!kf->update(frame))
{
    kf->reset(); // If the algorithm failed, reset current state
```

If `after_reset` is true, there is no pointcloud available (the scene was reset in the current iteration). Therefore, we can safely try to acquire the new pointcloud only when `after_reset` is false. `getCloud` stores the pointcloud's points and normals in OpenCV's `UMat` objects, and we'll use them for rendering.
```cpp
if (!after_reset)
{
    kf->getCloud(points, normals);
    after_reset = false;
}
```

The main thread handles the rendering of the pointcloud, using the functions `colorize_pointcloud`, which assigns an RGB value for each pixel based on it's depth, and `draw_kinfu_pointcloud`, which renders the pointcloud using `OpenGL`. 

The function `export_to_ply` is also available and creates a PLY file containing the pointcloud, using colorized points and normals.
