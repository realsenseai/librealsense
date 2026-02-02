# rs-capture Sample

## Overview

This sample demonstrates how to configure the camera for streaming and rendering Depth & RGB data to the screen.  
We use OpenGL for cross-platform rendering and GLFW for window management.  
If you are using OpenCV, `imshow` is a good alternative. 

## Expected Output
![expected output](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/res/capture-expected.png)

## Code Overview 

First, we include the RealSense™ Cross-Platform API.  
All but advanced functionality is provided through a single header:
```cpp
#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
```

Next, we include a [very short helper library](../example.hpp) to encapsulate OpenGL rendering and window management:
```cpp
#include "example.hpp"          // Include a short list of convenience functions for rendering
```

This header lets us easily open a new window and prepare textures for rendering.  
```cpp
// Create a simple OpenGL window for rendering:
window app(1280, 720, "RealSense Capture Example");
```

Depth data is usually provided on a 12-bit grayscale which is not very useful for visualization.  
To enhance visualization, we provide an API that converts the grayscale image to RGB:
```cpp
// Declare depth colorizer for enhanced color visualization of depth data
rs2::colorizer color_map; 
```

The sample prints frame rates of each enabled stream, this is done by rates_printer filter._
```cpp
// Declare rates printer for showing streaming rates of the enabled streams.
rs2::rates_printer printer;
```

The SDK API entry point is the `pipeline` class:
```cpp
// Declare the RealSense pipeline, encapsulating the actual device and sensors
rs2::pipeline pipe;

// Start streaming with default recommended configuration
// The default video configuration contains Depth and Color streams
// If a device is capable to stream IMU data, both Gyro and Accelerometer are enabled by default 
pipe.start(); 
```

Next, we wait for the next set of frames, effectively blocking the program.
Once a frameset arrives, apply both the colorizer and the rates_printer filters._
```cpp
rs2::frameset data = pipe.wait_for_frames().    // Wait for next set of frames from the camera
                     apply_filter(printer).     // Print each enabled stream frame rate
                     apply_filter(color_map);   // Find and colorize the depth data
```

Finally, render the images by the `window` class from [example.hpp](../example.hpp)
```cpp
// Show method, when applied on frameset, break it to frames and upload each frame into a gl textures
// Each texture is displayed on different viewport according to it's stream unique id
app.show(data);
```
