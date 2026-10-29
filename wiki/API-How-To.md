### Table of Content
* [Get first RealSense device](#get-first-realsense-device)
* [Start Streaming with Default Configuration](#start-streaming-with-default-configuration)
* [Start Streaming Left and Right Infrared Imagers](#start-streaming-left-and-right-infrared-imagers)
* [Wait for Coherent Set of Frames](#wait-for-coherent-set-of-frames)
* [Poll for Frames](#poll-for-frames)
* [Do Processing on a Background-Thread](#do-processing-on-a-background-thread)
* [Get and Apply Depth-to-Color Extrinsics](#get-and-apply-depth-to-color-extrinsics)
* [Get Disparity Baseline](#get-disparity-baseline)
* [Get Video Stream Intrinsics](#get-video-stream-intrinsics)
* [Get Field of View](#get-field-of-view)
* [Get Depth Units](#get-depth-units)
* [Controlling the Laser](#controlling-the-laser)
* [Get Frame Dimensions and Stride](#get-frame-dimensions-and-stride)
* [Get Frame Timestamp and Metadata](#get-frame-timestamp-and-metadata)
* [Align Depth to Color](#align-depth-to-color)

All snippets below use the librealsense2 C++ API (`#include <librealsense2/rs.hpp>`). Fuller versions of some of them (getting a device, depth units, field of view, extrinsics, changing sensor options) live in [examples/sensor-control/api_how_to.h](https://github.com/realsenseai/librealsense/blob/development/examples/sensor-control/api_how_to.h).

### Get first RealSense device
```cpp
rs2::context ctx;
auto list = ctx.query_devices(); // Get a snapshot of currently connected devices
if (list.size() == 0) 
    throw std::runtime_error("No device detected. Is it plugged in?");
rs2::device dev = list.front();
```

### Start Streaming with Default Configuration
```cpp
rs2::pipeline pipe;
pipe.start();
```

### Start Streaming Left and Right Infrared Imagers
```cpp
rs2::config cfg;
cfg.enable_stream(RS2_STREAM_INFRARED, 1); // left imager
cfg.enable_stream(RS2_STREAM_INFRARED, 2); // right imager
rs2::pipeline pipe;
pipe.start(cfg);
```

### Wait for Coherent Set of Frames
```cpp
rs2::pipeline pipe;
pipe.start();
rs2::frameset frames = pipe.wait_for_frames();
rs2::frame frame = frames.first(RS2_STREAM_DEPTH);
if (frame)
    frame.get_data(); // Pointer to depth pixels, 
                      // invalidated when last copy of frame goes out of scope
```

### Poll for Frames
```cpp
rs2::pipeline pipe;
pipe.start();
rs2::frameset frames;
if (pipe.poll_for_frames(&frames))
{
    rs2::frame depth_frame = frames.first(RS2_STREAM_DEPTH);
    depth_frame.get_data();
}
```

### Do Processing on a Background-Thread
```cpp
rs2::pipeline pipe;
pipe.start();

const auto CAPACITY = 5; // allow max latency of 5 frames
rs2::frame_queue queue(CAPACITY);
std::thread t([&]() {
    while (true)
    {
        rs2::frame frame;
        if (queue.poll_for_frame(&frame))
        {
	        frame.get_data();
	        // Do processing on the frame
        }
    }
});
t.detach();

while (true)
{
    auto frames = pipe.wait_for_frames();
    queue.enqueue(frames.get_depth_frame());
}
```

### Get and Apply Depth-to-Color Extrinsics
```cpp
rs2::pipeline pipe;
rs2::pipeline_profile selection = pipe.start();
auto depth_stream = selection.get_stream(RS2_STREAM_DEPTH);
auto color_stream = selection.get_stream(RS2_STREAM_COLOR);
rs2_extrinsics e = depth_stream.get_extrinsics_to(color_stream);
// Apply extrinsics to the origin
float origin[3] { 0.f, 0.f, 0.f };
float target[3];
rs2_transform_point_to_point(target, &e, origin);
```

### Get Disparity Baseline
```cpp
rs2::config cfg;
cfg.enable_stream(RS2_STREAM_INFRARED, 1);
cfg.enable_stream(RS2_STREAM_INFRARED, 2);
rs2::pipeline pipe;
rs2::pipeline_profile selection = pipe.start(cfg);
auto ir1_stream = selection.get_stream(RS2_STREAM_INFRARED, 1);
auto ir2_stream = selection.get_stream(RS2_STREAM_INFRARED, 2);
rs2_extrinsics e = ir1_stream.get_extrinsics_to(ir2_stream);
auto baseline = e.translation[0];
```

### Get Video Stream Intrinsics
```cpp
rs2::pipeline pipe;
rs2::pipeline_profile selection = pipe.start();
auto depth_stream = selection.get_stream(RS2_STREAM_DEPTH)
                             .as<rs2::video_stream_profile>();
auto resolution = std::make_pair(depth_stream.width(), depth_stream.height());
auto i = depth_stream.get_intrinsics();
auto principal_point = std::make_pair(i.ppx, i.ppy);
auto focal_length = std::make_pair(i.fx, i.fy);
rs2_distortion model = i.model;
```

### Get Field of View
```cpp
rs2::pipeline pipe;
rs2::pipeline_profile selection = pipe.start();
auto depth_stream = selection.get_stream(RS2_STREAM_DEPTH)
                             .as<rs2::video_stream_profile>();
auto i = depth_stream.get_intrinsics();
float fov[2]; // X, Y fov
rs2_fov(&i, fov);
```

### Get Depth Units
```cpp
rs2::pipeline pipe;
rs2::pipeline_profile selection = pipe.start();

// Find first depth sensor (devices can have zero or more then one)
auto sensor = selection.get_device().first<rs2::depth_sensor>();
auto scale =  sensor.get_depth_scale();
```

### Controlling the Laser
```cpp
rs2::pipeline pipe; 
rs2::pipeline_profile selection = pipe.start();
rs2::device selected_device = selection.get_device();
auto depth_sensor = selected_device.first<rs2::depth_sensor>();

if (depth_sensor.supports(RS2_OPTION_EMITTER_ENABLED))
{
    depth_sensor.set_option(RS2_OPTION_EMITTER_ENABLED, 1.f); // Enable emitter
    depth_sensor.set_option(RS2_OPTION_EMITTER_ENABLED, 0.f); // Disable emitter
}
if (depth_sensor.supports(RS2_OPTION_LASER_POWER))
{
    // Query min and max values:
    auto range = depth_sensor.get_option_range(RS2_OPTION_LASER_POWER);
    depth_sensor.set_option(RS2_OPTION_LASER_POWER, range.max); // Set max power
    depth_sensor.set_option(RS2_OPTION_LASER_POWER, 0.f); // Disable laser
}
```

### Get Frame Dimensions and Stride
```cpp
rs2::pipeline pipe;
pipe.start();
rs2::frameset frames = pipe.wait_for_frames();
rs2::video_frame vf = frames.get_depth_frame();
int width  = vf.get_width();
int height = vf.get_height();
int bpp    = vf.get_bytes_per_pixel();
int stride = vf.get_stride_in_bytes(); // row pitch, may be larger than width * bpp
```

### Get Frame Timestamp and Metadata
```cpp
rs2::pipeline pipe;
pipe.start();
rs2::frame frame = pipe.wait_for_frames().get_depth_frame();
double ts_ms = frame.get_timestamp();           // in the domain reported by get_frame_timestamp_domain()
unsigned long long fn = frame.get_frame_number();
if (frame.supports_frame_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE))
{
    auto exposure = frame.get_frame_metadata(RS2_FRAME_METADATA_ACTUAL_EXPOSURE);
}
```
See [Frame Metadata](https://github.com/realsenseai/librealsense/blob/development/doc/frame_metadata.md) for the list of attributes and platform requirements.

### Align Depth to Color
```cpp
rs2::pipeline pipe;
pipe.start();
rs2::align align_to_color(RS2_STREAM_COLOR); // or RS2_STREAM_DEPTH to align color to depth
while (true)
{
    rs2::frameset frames = pipe.wait_for_frames();
    rs2::frameset aligned = align_to_color.process(frames);
    rs2::depth_frame aligned_depth = aligned.get_depth_frame(); // same resolution as color
    rs2::video_frame color = aligned.get_color_frame();
}
```
See [Projection in RealSense SDK 2.0](https://github.com/realsenseai/librealsense/wiki/Projection-in-RealSense-SDK-2.0#frame-alignment) and [examples/align](https://github.com/realsenseai/librealsense/tree/development/examples/align).

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`46cf1a6`](https://github.com/realsenseai/librealsense/commit/46cf1a67cc441ad9f55ce339a0de30f2a1c6fed0)._
<!-- /wikigen:footer -->
