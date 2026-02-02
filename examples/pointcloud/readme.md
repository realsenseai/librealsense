# rs-pointcloud Sample

## Overview

This sample demonstrates how to generate and visualize textured 3D pointcloud.

## Expected Output
The application should open a window with a pointcloud. Using your mouse, you should be able to interact with the pointcloud  rotating and zooming using the mouse.

![expected output](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/res/point-cloud-expected.png)

## Code Overview

Similar to the [first tutorial](../capture/) we include the Cross-Platform API:
```cpp
#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
```

Next, we prepared a [very short helper library](../example.hpp) encapsulating basic OpenGL rendering and window management:
```cpp
#include "example.hpp"          // Include short list of convenience functions for rendering
```

We also include the STL `<algorithm>` header for `std::min` and `std::max`.

Next, we define a `state` struct and two helper functions. `state` and `register_glfw_callbacks` handle the pointcloud's rotation in the application, and `draw_pointcloud` makes all the OpenGL calls necessary to display the pointcloud.
```cpp
// Struct for managing rotation of pointcloud view
struct state { double yaw, pitch, last_x, last_y; bool ml; float offset_x, offset_y; texture tex; };

// Helper functions
void register_glfw_callbacks(window& app, state& app_state);
void draw_pointcloud(window& app, state& app_state, rs2::points& points);
```

The `example.hpp` header lets us easily open a new window and prepare textures for rendering. The `state` class (declared above) is used for interacting with the mouse, with the help of some callbacks registered through glfw.
```cpp
// Create a simple OpenGL window for rendering:
window app(1280, 720, "RealSense Pointcloud Example");
// Construct an object to manage view state
state app_state = { 0, 0, 0, 0, false, 0, 0, 0 };
// register callbacks to allow manipulation of the pointcloud
register_glfw_callbacks(app, app_state);
```

We are going to use classes within the `rs2` namespace:
```cpp
using namespace rs2;
```

As part of the API we offer the `pointcloud` class which calculates a pointcloud and corresponding texture mapping from depth and color frames. To make sure we always have something to display, we also make a `rs2::points` object to store the results of the pointcloud calculation.
```cpp
// Declare pointcloud object, for calculating pointclouds and texture mappings
rs2::pointcloud pc;
// We want the points object to be persistent so we can display the last cloud when a frame drops
rs2::points points;
```

The `Pipeline` class is the entry point to the SDK's functionality:
```cpp
// Declare RealSense pipeline, encapsulating the actual device and sensors
pipeline pipe;
// Start streaming with default recommended configuration
pipe.start();
```

Next, we wait in a loop unit for the next set of frames:
```cpp
auto data = pipe.wait_for_frames(); // Wait for next set of frames from the camera
```

Using helper functions on the `frameset` object we check for new depth and color frames. We pass it to the `pointcloud` object to use as the texture, and also give it to OpenGL with the help of the `texture` class. We generate a new pointcloud.
```cpp
// Wait for the next set of frames from the camera
auto frames = pipe.wait_for_frames();
auto depth = frames.get_depth_frame();

// Generate the pointcloud and texture mappings
points = pc.calculate(depth);

auto color = frames.get_color_frame();

// Tell pointcloud object to map to this color frame
pc.map_to(color);

// Upload the color frame to OpenGL
app_state.tex.upload(color);

```

Finally we call `draw_pointcloud` to draw the pointcloud.
```cpp
draw_pointcloud(app, app_state, points);
```

`draw_pointcloud` is primarily calls to OpenGL, but the critical portion iterates over all the points in the pointcloud, and where we have depth data, we upload the point's coordinates and texture mapping coordinates to OpenGL.
```cpp
/* this segment actually prints the pointcloud */
auto vertices = points.get_vertices();              // get vertices
auto tex_coords = points.get_texture_coordinates(); // and texture coordinates
for (int i = 0; i < points.size(); i++)
{
    if (vertices[i].z)
    {
        // upload the point and texture coordinates only for points we have depth data for
        glVertex3fv(vertices[i]);
        glTexCoord2fv(tex_coords[i]);
    }
}
```
