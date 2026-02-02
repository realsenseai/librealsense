# rs-hdr tutorial

## Overview

The tutorial demonstrates High Dynamic Range (HDR) implementation* applied to the Depth sensor.
It walks through the sensor configuration phase and visualizes the algorithm inputs and output to facilitate the feature adaptation in production code. 

\* For in-depth review of the subject please read the accompanying [white paper](https://dev.realsenseai.com/docs/high-dynamic-range-with-stereoscopic-depth-cameras).




## Expected Output
 ![](https://github.com/realsenseai/librealsense/blob/master/examples/hdr/hdr_demo.gif)
 
## Code Overview 

### Finding device
The HDR feature is applicable only for D400 Product line Devices. \
Finiding this device and getting its depth sensor:
```cpp
// finding a device of D400 product line for working with HDR feature
if (dev.supports(RS2_CAMERA_INFO_PRODUCT_LINE) && 
            std::string(dev.get_info(RS2_CAMERA_INFO_PRODUCT_LINE)) == "D400")
{
    device = dev;
}
rs2::depth_sensor depth_sensor = device.query_sensors().front();
 ```


### HDR Configuration
HDR is created from merging 2 sequential frames, therefore we need to create 2 sequences in the depth sensor. \
Each sequence is defined with its own exposure and gain value, in order to acquire a high dynamic range image when applying the HDR merging filter. \
\
Before starting the configuration, we need to disable the Auto Exposure option:
```cpp
// disable auto exposure before sending HDR configuration
if (depth_sensor.get_option(RS2_OPTION_ENABLE_AUTO_EXPOSURE))
    depth_sensor.set_option(RS2_OPTION_ENABLE_AUTO_EXPOSURE, 0);
```


Configuring the HDR sequence size to 2:
```cpp
// setting the HDR sequence size to 2 frames
depth_sensor.set_option(RS2_OPTION_SEQUENCE_SIZE, 2);
```
Choosing an ID for this HDR configuration. The configuration will not be saved in the firmware, but this ID can help users that configure another HDR configuration to know when the new HDR is streaming, by checking this value via the metadata:
```cpp
// configuring identifier for this HDR config (value must be in range [0,3])
depth_sensor.set_option(RS2_OPTION_SEQUENCE_NAME, 1);
```

Configuring the first HDR sequence ID: 
```cpp
// configuration for the first HDR sequence ID
depth_sensor.set_option(RS2_OPTION_SEQUENCE_ID, 1);
depth_sensor.set_option(RS2_OPTION_EXPOSURE, 8000); // setting exposure to 8000, so sequence 1 will be set to high exposure
depth_sensor.set_option(RS2_OPTION_GAIN, 25); // setting gain to 25, so sequence 1 will be set to high gain
```
Configuring the second HDR sequence ID:
```cpp
// configuration for the second HDR sequence ID
depth_sensor.set_option(RS2_OPTION_SEQUENCE_ID, 2);
depth_sensor.set_option(RS2_OPTION_EXPOSURE, 18);  // setting exposure to 18, so sequence 2 will be set to low exposure
depth_sensor.set_option(RS2_OPTION_GAIN, 16); // setting gain to 16, so sequence 2 will be set to low gain
```
Activating the HDR configuration in order to get the resulting streaming:
```cpp
// turning ON the HDR with the above configuration 
depth_sensor.set_option(RS2_OPTION_HDR_ENABLED, 1);
```

Then, the pipe is configured with depth and infrared streams. \
In HDR mode the Infrared stream is used as an auxiliary invalidation filter to handle outlier Depth pixels and therefore, to enhance the outcome. \
The HDR merge algorithm uses Depth and (optionally) Intensity input frames for processing.\
Adding the intensity frames as auxiliary inputs allows to enhance the algorithm robustness and filter out over or under-saturated areas.
```cpp
// Start streaming with depth and infrared configuration
// The HDR merging algorithm can work with both depth and infrared,or only with depth, 
// but the resulting stream is better when both depth and infrared are used.
rs2::config cfg;
cfg.enable_stream(RS2_STREAM_DEPTH);
cfg.enable_stream(RS2_STREAM_INFRARED, 1);
pipe.start(cfg);
```

### Setup view window
Initializing the view window of the hdr demo, for the frames 
from the camera, with number of tiles in row and column on the window.

```cpp
// init view window 
window app(width, height, title, tiles_in_row, tiles_in_col);
```
Initializing ImGui for sliders and text boxes and an object to manage hdr widgets (sliders and text boxes).

```cpp
// init ImGui with app (window object)
ImGui_ImplGlfw_Init(app, false);

// init HDR widgets object
// hdr_widgets holds the sliders, the text boxes and the frames_map 
hdr_widgets hdr_widgets(depth_sensor);
```

### Using Merging Filter
Initializing the merging filter -  to be used later in order to merge frames from both HDR sequence IDs that have been configured:
```cpp
// initializing the merging filter
rs2::depth_merge merging_filter;
```
After getting the frames, by using the wait_for_frames method, the merging filter is used:
```cpp
// merging the frames from the different HDR sequence IDs
auto merged_frameset = merging_filter.process(data). 
        apply_filter(color_map);   // Find and colorize the depth data;
```

### Render frames and widgets
Getting metadata from the last frame we received:
```cpp
//get frames data 
auto hdr_seq_size = frame.get_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_SIZE);
auto hdr_seq_id = frame.get_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_ID);
```
Getting the infrared and depth frames:
```cpp
//get frames
auto infrared_frame = data.get_infrared_frame();
auto depth_frame = data.get_depth_frame().apply_filter(color_map);
auto hdr_frame = merged_frame.as<rs2::frameset>().get_depth_frame().apply_filter(color_map);
```

Sending the new frames to hdr widget.
```cpp
//update frames in frames map in HDR widgets
hdr_widgets.update_frames_map(infrared_frame, depth_frame, hdr_frame, hdr_seq_id, hdr_seq_size);
```
Render the widgets and the frames:
```cpp
//render HDR widgets sliders and text boxes
hdr_widgets.render_widgets();

//the show method, when applied on frame map, break it to frames and upload each frame into its specific tile
app.show(hdr_widgets.get_frames_map());
```
