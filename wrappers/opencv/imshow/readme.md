# rs-imshow Sample

## Overview
This example is a "hello-world" code snippet for RealSense cameras integration with OpenCV. The sample will open an OpenCV UI window and render colorized depth stream to it. 
The following code snippet is used to create `cv::Mat` from `rs2::frame`:
```cpp
// Query frame size (width and height)
const int w = depth.as<rs2::video_frame>().get_width();
const int h = depth.as<rs2::video_frame>().get_height();

// Create OpenCV matrix of size (w,h) from the colorized depth data
Mat image(Size(w, h), CV_8UC3, (void*)depth.get_data(), Mat::AUTO_STEP);
``` 
