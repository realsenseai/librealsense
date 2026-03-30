# rs-record-playback Sample

## Overview

This example demonstrates usage of the recorder and playback devices. We'll show how to use `rs2::recorder` with `rs2::pipeline` to
record frames from the camera to a .db3 file ('a.db3' in the example), with an option to pause and resume the recording. 

After the file is ready, we'll demonstrate how to play, pause, seek and stop a .db3 file using `rs2::playback`.

Throughout the example, frames from the active device (default, recorder or playback) will be rendered.

## Code Overview

### Declarations

As with any SDK application we include the Intel RealSense Cross Platform API:

```cpp
#include <librealsense2/rs.hpp>
```

In this example we will also use the auxiliary library of `example.hpp`:

```cpp
#include "../example.hpp"    
```

`example.hpp` lets us easily open a new window and prepare textures for rendering.

In addition to the standard library included headers, we include 2 header files which will help us to render GUI controls in our window application:

```cpp
#include <imgui.h>
#include "imgui_impl_glfw.h"
```
These headers are part of the [ImGui](https://github.com/ocornut/imgui) library which we use to render GUI elements.

Also, we include header files used for measuring time in the playback and for convenient display of time:

```cpp
#include <sstream>
#include <iostream>
#include <iomanip>
```

We'll use 2 helper functions for rendering the GUI, to allow us to make the main code less verbose.

```cpp
// Helper function for displaying time conveniently
std::string pretty_time(std::chrono::nanoseconds duration);
// Helper function for rendering a seek bar
void draw_seek_bar(rs2::playback& playback, int* seek_pos, float2& location, float width);
```

### Main

We first define some variables that will be used to show the window and GUI.

```cpp
// Create a simple OpenGL window for rendering:
window app(1280, 720, "RealSense Record and Playback Example");
ImGui::CreateContext();
ImGui_ImplGlfw_InitForOpenGL(app, false);
ImGui_ImplOpenGL3_Init();

// Create booleans to control GUI (recorded - allow play button, recording - show 'recording to file' text)
bool recorded = false;
bool recording = false;
 ```

Next, we define a `texture`, which is a class designed to hold video frame data for rendering, `rs2::frameset ` which will hold
a set of frames from the camera and `rs2::frame` for the depth frame.
```cpp
// Declare a texture for the depth image on the GPU
texture depth_image;

// Declare frameset and frames which will hold the data from the camera
rs2::frameset frames;
rs2::frame depth;
 ```

Also, we define an `rs2::colorizer` to allow the point cloud visualization have a texture:
```cpp
// Declare depth colorizer for pretty visualization of depth data
 rs2::colorizer color_map;
 ```

Now we need to create an `rs2::pipeline` which is a top level API for using RealSense depth cameras.
`rs2::pipeline` automatically chooses a camera from all connected cameras which matches the given configuration,
so we can simply call `pipeline::start(cfg)` and the camera is configured and streaming.

In order to allow using different types of devices (`rs2::recorder`, `rs2::playback`) and read\write to the same file, we define a shared
pointer to `rs2::pipeline`. This way we'll be able to release previous resources when switching to a different device.

For now, we configure the pointed pipeline with the default configuration and start streaming the default stream. We'll use this stream
whenever there is no recorder or playback running, for continuous display of images from the camera.

```cpp
// Create a shared pointer to a pipeline
auto pipe = std::make_shared<rs2::pipeline>();
// Start streaming with default configuration
pipe->start();
```

Define `seek_pos`, which is used for allowing the user control the playback.

```cpp
// Create a variable to control the seek bar
int seek_pos;
```

### Record and Playback

First, we wait for frames from the camera for rendering. This is relevant for frames which arrive from the recorder or from the
device with the default configuration, because those two are streaming live. We can not use `wait_for_frames` with the playback,
because we might run out of frames.
    
```cpp
// If the device is sreaming live and not from a file
if (!device.as<rs2::playback>())
{
    frames = pipe->wait_for_frames(); // wait for next set of frames from the camera
    depth = color_map.process(frames.get_depth_frame()); // Find and colorize the depth data
}	
```

We begin with recorder functions.
First, we allow recording only if playback is not currently running.

```cpp
if (!device.as<rs2::playback>()) 
```

There are two cases where the 'record' button would be clicked: begin a recording, or resume a recorder that has already started.
We check whether `device` is a already recorder to indicate which of the cases it is.

```cpp
// If it is the start of a new recording (device is not a recorder yet)
if (!device.as<rs2::recorder>())
```


In the first case, we stop the pipeline and initiate the shared pointer with a new pipeline.

```cpp
pipe->stop(); // Stop the pipeline with the default configuration
pipe = std::make_shared<rs2::pipeline>();
```
Then we initiate a new configuration, allowing recording to the file 'a.db3' using the function `enable_record_to_file`, and start the
pipeline with the new configuration. Also, we update the `device` variable to hold the current device.

```cpp
rs2::config cfg; // Declare a new configuration
cfg.enable_record_to_file("a.db3");
pipe->start(cfg); //File will be opened at this point
device = pipe->get_active_profile().get_device();
```


In the second case, the device is already a recorder, so we can just use its `resume` function. In order to access functions of `rs2::recorder`,
we use `as<rs2::recorder>()` .

```cpp
else { // If the recording is resumed after a pause, there's no need to reset the shared pointers
    device.as<rs2::recorder>().resume(); // rs2::recorder allows access to 'resume' function
}
```

We can pause the recording using the 'pause' function of the `rs2::recorder`.

```cpp
device.as<rs2::recorder>().pause();
```

To stop recording, we need to stop the pipeline and release any resources that the pipeline holds,
including the 'a.db3' file. Therefore, we initiate the shared pointer with a new pipeline.

```cpp
pipe->stop(); // Stop the pipeline that holds the file and the recorder
pipe = std::make_shared<rs2::pipeline>(); //Reset the shared pointer with a new pipeline
```

Then, we start the new pipeline with the default configuration, which allows us to render frames from the camera while no recorder or
playback are running. Also, we update `device` to hold the current device.

```cpp
pipe->start(); // Resume streaming with default configuration
device = pipe->get_active_profile().get_device();
recorded = true; // Now we can run the file
recording = false;
```

Now that we have a recorded file, we can play it using the 'playback' device. The flow is the same as for the recorder, only this time
we use `enable_device_from_file` function on the configuration:

```cpp
if (!device.as<rs2::playback>())
{
    pipe->stop(); // Stop streaming with default configuration
    pipe = std::make_shared<rs2::pipeline>();
    rs2::config cfg;
    cfg.enable_device_from_file("a.db3");
    pipe->start(cfg); //File will be opened in read mode at this point
    device = pipe->get_active_profile().get_device();
}
else
{
    device.as<rs2::playback>().resume();
}
```

For pause of the playback, we use `device.as<rs2::playback>().pause()`.

Stopping the playback, similarly to stopping the recorder:

```cpp
pipe->stop();
pipe = std::make_shared<rs2::pipeline>();
pipe->start();
device = pipe->get_active_profile().get_device();
```


In order to render frames from the playback, we check if there are ready frames in the pipeline using `poll_for_frames`.

```cpp
if (pipe->poll_for_frames(&frames)) // Check if new frames are ready
{
    depth = color_map.process(frames.get_depth_frame()); // Find and colorize the depth data for rendering
}
```

Also, we call `draw_seek_bar` function which allows the user to control the playback.

```cpp
// Render a seek bar for the player
float2 location = { app.width() / 4, 4 * app.height() / 5 + 110 };
draw_seek_bar(playback, &seek_pos, location, app.width() / 2);
```

Finally, depth rendering is implemented by the `texture` class from [example.hpp](../example.hpp)
```cpp
// Render depth frames from the default configuration, the recorder or the playback
depth_image.render(depth, { app.width() * 0.25f, app.height() * 0.25f, app.width() * 0.5f, app.height() * 0.75f });
```
