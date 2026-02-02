# rs-align-advanced Sample

## Overview

This sample demonstrates one possible use-case of the `rs2::align` object, which allows users to align between the depth and other streams and vice versa. <br>

The alignment utility performs per-pixel geometric transformation based on the depth data provided and is not suited for aligning images that are intrinsically 2D, such Color, IR or Fisheye. In addition the transformation requires undistorted (rectified) images to proceed, and therefore is not applicable to the IR calibration streams.  


In this example, we align depth frames to their corresponding color frames.
We generate a new frame sized as color stream but the content being depth data calculated in the color sensor coordinate system. In other word to reconstruct a depth image being "captured" using the origin and dimensions of  the color sensor.
Then, we use the original color and the re-projected depth frames (which are aligned at this stage) to determine the depth value of each color pixel.

Using this information, we "remove" the background of the color frame that is further (away from the camera) than some user-define distance.

The example displays a GUI for controlling the maximum distance to show from the original color image.

## Expected Output

The application should open a window and display a video stream from the camera.

<p align="center"><img src="https://raw.githubusercontent.com/wiki/realsenseai/librealsense/res/align-expected.gif" alt="screenshot gif"/></p>

The window should have the following elements:
- On the left side of the window is a vertical slider for controlling the depth clipping distance.
- A color image with grayed out background
- A corresponding (colorized) depth image.



## Code Overview

As with any SDK application we include the RealSense Cross Platform API:

```cpp
#include <librealsense2/rs.hpp>
```

In this example we will also use the auxiliary library of `example.hpp`:

```cpp
#include "../example.hpp"    
```

`examples.hpp` lets us easily open a new window and prepare textures for rendering.

We include 2 more header files which will help us to render GUI controls in our window application:

```cpp
#include <imgui.h>
#include "imgui_impl_glfw.h"
```

These headers are part of the [ImGui](https://github.com/ocornut/imgui) library which we use to render GUI elements.

Next, we declare several functions to help the code look clearer:
```cpp
void render_slider(rect location, float& clipping_dist);
void remove_background(rs2::video_frame& color, const rs2::depth_frame& depth_frame, float depth_scale, float clipping_dist);
float get_depth_scale(rs2::device dev);
rs2_stream find_stream_to_align(const std::vector<rs2::stream_profile>& streams);
bool profile_changed(const std::vector<rs2::stream_profile>& current, const std::vector<rs2::stream_profile>& prev);
```

`render_slider(..)`  is where all the GUI code goes, and we will not cover this function in this overview.

`remove_background(..)` takes depth and color images (that are assumed to be aligned to one another), the depth scale units, and the maximum distance the user wishes to show, and updates the color frame so that its background (any pixel with depth distance larger than the maximum allowed) is removed.

`get_depth_scale(..)` tries to find a depth sensor from the pipeline's device and retrieves its depth scale units.

`find_stream_to_align(..)` goes over the given streams and verify that it has a depth profile and tries to find another profile to which depth should be aligned.

`profile_changed()` checks if the current streaming profiles contains all the previous one.


Heading to `main`:

We first define some variables that will be used to show the window and render the images and GUI to the screen:

```cpp
// Create and initialize GUI related objects
window app(1280, 720, "CPP - Align Example"); // Simple window handling
ImGui_ImplGlfw_Init(app, false);      // ImGui library intializition
rs2::colorizer c;                          // Helper to colorize depth images
texture renderer;                     // Helper for renderig images
```

Next, we define a `rs2::pipeline` which is a top level API for using RealSense depth cameras.
`rs2::pipeline` automatically chooses a camera from all connected cameras, so we can simply call `pipeline::start()` and the camera is configured and streaming:

```cpp
// Create a pipeline to easily configure and start the camera
rs2::pipeline pipe;
//Calling pipeline's start() without any additional parameters will start the first device
// with its default streams.
//The start function returns the pipeline profile which the pipeline used to start the device
rs2::pipeline_profile profile = pipe.start();
```

At this point of the program the camera is configured and streams are available from the pipeline.

Before actually using the frames, we try to get the depth scale units of the depth camera. Depth scale units are used to convert the depth pixel data (16-bit unsigned) into metric units.

```cpp
// Each depth camera might have different units for depth pixels, so we get it here
// Using the pipeline's profile, we can retrieve the device that the pipeline uses
float depth_scale = get_depth_scale(profile.get_device());
```

These units are expressed as depth in meters corresponding to a depth value of 1. For example if we have a depth pixel with a value of 2 and the depth scale units are 0.5 then that pixel is `2 X 0.5 = 1` meter away from the camera.

Then, we create an `align` object:

```cpp
//Pipeline could choose a device that does not have a color stream
//If there is no color stream, choose to align depth to another stream
rs2_stream align_to = find_stream_to_align(profile.get_streams());

// Create a rs2::align object.
// rs2::align allows us to perform alignment of depth frames to others frames
//The "align_to" is the stream type to which we plan to align depth frames.
rs2::align align(align_to);
```

`rs2::align` is a utility class that performs image alignment (registration) of 2 frames.
Basically, each pixel from the first image will be transformed so that it matches its corresponding pixel in the second image.
A `rs2::align` object transforms between two input images, from a source image to some target image which is specified with the `align_to` parameter.

Now comes the interesting part of the application. We start our main loop, which breaks only when the window is closed:


```cpp
while (app) // Application still alive?
{
```

Inside the loop, the first thing we do is block the program until the `align` object returns a `rs2::frameset`. A `rs2::frameset` is an object that holds a set of frames and provides an interface for easily accessing them.

```cpp
  // Using the align object, we block the application until a frameset is available
  rs2::frameset frameset = pipe.wait_for_frames();
```

The `frameset` returned from `wait_for_frames` should contain a set of aligned frames. In case of an error getting the frames an exception could be thrown, but if the pipeline manages to reconfigure itself with a new device it will do that and return a frame from the new device.
In the next lines we check if the pipeline switched its device and if so update the align object and the rest of objects required for the sample.

```cpp
// rs2::pipeline::wait_for_frames() can replace the device it uses in case of device error or disconnection.
// Since rs2::align is aligning depth to some other stream, we need to make sure that the stream was not changed
//  after the call to wait_for_frames();
if (profile_changed(pipe.get_active_profile().get_streams(), profile.get_streams()))
{
    //If the profile was changed, update the align object, and also get the new device's depth scale
    profile = pipe.get_active_profile();
    align_to = find_stream_to_align(profile.get_streams());
    align = rs2::align(align_to);
    depth_scale = get_depth_scale(profile.get_device());
}
```

At this point the `align` object is valid and will be able to align depth frames with other frames.

```cpp
    //Get processed aligned frame
    auto processed = align.process(frameset);

    // Trying to get both color and aligned depth frames
    rs2::video_frame other_frame = processed.first_or_default(align_to);
    rs2::depth_frame aligned_depth_frame = processed.get_depth_frame();

    //If one of them is unavailable, continue iteration
    if (!aligned_depth_frame || !other_frame)
    {
        continue;
    }
```
Notice that the color frame is of type `rs2::video_frame` and the depth frame if of type `rs2::depth_frame` (which derives from `rs2::video_frame` and adds special depth related functionality).

After getting the two aligned color and depth frames, we call the `remove_background(..)` function to strip the background from the color image.
This is a simple function that performs a naive algorithm for background segmentation.
```cpp
    // Passing both frames to remove_background so it will "strip" the background
    // NOTE: in this example, we alter the buffer of the color frame, instead of copying it and altering the copy
    //  This behavior is not recommended in real application since the color frame could be used elsewhere
    remove_background(color_frame, aligned_depth_frame, depth_scale, depth_clipping_distance);

```

The  rest of the loop contains code that takes care of rendering and GUI controls. We will not elaborate on it. Let's go over `remove_background(..)` instead:


```cpp
void remove_background(rs2::video_frame& other_frame, const rs2::depth_frame& depth_frame, float depth_scale, float clipping_dist)
{
```

In the beginning of function, we take a pointer to the raw buffer of both frames, so that we could alter the color image (instead of creating a new buffer).

```cpp
    const uint16_t* p_depth_frame = reinterpret_cast<const uint16_t*>(depth_frame.get_data());
    uint8_t* p_other_frame = reinterpret_cast<uint8_t*>(const_cast<void*>(other_frame.get_data()));
```

Next, we go over each pixel of the frame.

```cpp
    //Using OpenMP to try to parallelise the loop
    #pragma omp parallel for schedule(dynamic)
    for (int y = 0; y < height; y++)
    {
        auto depth_pixel_index = y * width;
        for (int x = 0; x < width; x++, ++depth_pixel_index)
        {
```
Calculate the depth distance of that pixel:
```cpp
            // Get the depth value of the current pixel
            auto pixels_distance = depth_scale * p_depth_frame[depth_pixel_index];

```

If that distance is invalid (`pixels_distance <= 0.f`) or further away than the maximum distance that the user requested (`pixels_distance > clipping_dist`) then we should strip off that pixel from the resulted color image.
```cpp
            // Check if the depth value is invalid (<=0) or greater than the threashold
            if (pixels_distance <= 0.f || pixels_distance > clipping_dist)
            {
```
By "strip off" we mean that we simply paint that pixel with a gray color.

```cpp
                // Calculate the offset in other frame's buffer to current pixel
                auto offset = depth_pixel_index * other_bpp;

                // Set pixel to "background" color (0x999999)
                std::memset(&p_other_frame[offset], 0x99, other_bpp);
            }
        }
    }
```
