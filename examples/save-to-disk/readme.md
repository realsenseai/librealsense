# rs-save-to-disk Sample

## Overview

This sample demonstrates how to configure the camera for streaming in a textual environment and save depth and color data to PNG format. In addition, it touches on the subject of [per-frame metadata](../../doc/frame_metadata.md)

## Expected Output
The application should run for about a second and exit after saving PNG and CSV file to disk: 
![expected output](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/res/save-to-disk-expected.PNG)

## Code Overview 

Similar to the [first tutorial](../capture/) we include the Cross-Platform API:
```cpp
#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
```

We are using [nothings/stb](https://github.com/nothings/stb) to quickly save data to disk in PNG format: 
```cpp
// 3rd party header for writing png files
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
```

We start like the last time by defining depth colorizer and starting the pipeline:
```cpp
// Declare depth colorizer for pretty visualization of depth data
rs2::colorizer color_map;

// Declare RealSense pipeline, encapsulating the actual device and sensors
rs2::pipeline pipe;
// Start streaming with default recommended configuration
pipe.start();
```

We prefer not to save the absolute first frame that arrives from the device, but rather wait for auto-exposure to stabalize:
```cpp
// Capture 30 frames to give autoexposure, etc. a chance to settle
for (auto i = 0; i < 30; ++i) pipe.wait_for_frames();
```

RealSense™ devices are not limited to just video streaming, some can offer motion tracking and 6-DOF positioning. For this example we are only interested in video frames, however: 
```cpp
// We can only save video frames as pngs, so we skip the rest
if (auto vf = frame.as<rs2::video_frame>())
```

To better visualize the depth data, we apply the colorizer on any incoming depth frames:
```cpp
// Use the colorizer to get an rgb image for the depth stream
if (vf.is<rs2::depth_frame>()) vf = color_map(frame);
```

Then we save frame data to PNG: 
```cpp
stbi_write_png(png_file.str().c_str(), vf.get_width(), vf.get_height(),
               vf.get_bytes_per_pixel(), vf.get_data(), vf.get_stride_in_bytes());
```

Each frame may come with some metadata feilds. We iterate over all possible metadata feilds and save to CSV these that are available:
```cpp
// Record all the available metadata attributes
for (size_t i = 0; i < RS2_FRAME_METADATA_COUNT; i++)
{
    if (frm.supports_frame_metadata((rs2_frame_metadata)i))
    {
        csv << rs2_frame_metadata_to_string((rs2_frame_metadata)i) << ","
            << frm.get_frame_metadata((rs2_frame_metadata)i) << "\n";
    }
}
```
Please see [per-frame metadata](../../doc/frame_metadata.md) for more information.
