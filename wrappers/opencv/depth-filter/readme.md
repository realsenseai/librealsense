# rs-depth-filter Sample

## Overview
This sample shows advanced depth-map processing techniques, developed by Daniel Pohl and Markus Achtelik for collision avoidance in outdoor drones with D400-series cameras. 

The code follows closely ["Depth Map Improvements for Stereo-based
Depth Cameras on Drones"](http://www.qwrt.de/pdf/Depth_Map_Improvements_for_Stereo_based_Depth_Cameras_on_Drones.pdf) paper.

## Problem Statement

The problem of collision avoidance prioritizes having reliable depth over high fill-rates. 
In stereo-based systems unreliable readings can occur due to several optical and algorithmic effects, including repetative geometry and moire pattern during rectification. 
There are several best-known methods for removing such invalid values of depth:

1. When possible, increasing IR projector will introduce sufficient amount of noise into the image and help the algorithm correctly resolve problematic cases. 
2. In addition to 1, blocking out the visible light using an optical filter to leave out only the projector pattern will remove false near-by depth values.
3. D400 series of cameras include a set of on-chip parameters controlling depth invalidation. Loading custom "High-Confidence" preset will help the ASIC discard ambiguous pixels.
4. Finally, software post-processing can be applied to keep only the high confidence depth values.

## High Confidence Preset

The follow code snippet can be used to load custom preset to the device prior to streaming:

```cpp
rs2::pipeline pipe;

rs2::config cfg;
cfg.enable_stream(RS2_STREAM_DEPTH, 848, 480);
cfg.enable_stream(RS2_STREAM_INFRARED, 1);

std::ifstream file("./camera-settings.json");
if (file.good())
{
    std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    auto prof = cfg.resolve(pipe);
    if (auto advanced = prof.get_device().as<rs400::advanced_mode>())
    {
        advanced.load_json(str);
    }
}
```

## `high_confidence_filter` Class

Next, we define `high_confidence_filter` class. Inheriting from `rs2::filter` and implementing SDK processing-block pattern makes this algorithm composable with other SDK methods, such as `rs2::pointcloud` and `rs2::align`.
In particular, `high_confidence_filter` is going to consume synchronized depth and infrared pair and output new synchronized pair of downsampled and filtered depth and infrared frames. 

The core idea behind this algorithm is that areas with well defined features in the infrared image are more likely to have high-confidence corresponding depth. The algorithm runs simultaneously on depth and infrared images and masks out everything except edges and corners.

## Downsample Step

Downsampling is a very common first step in any depth processing algorithm. The key observation is that downsampling reduces spatial (X-Y) accuracy but preserves Z-accuracy.
It is so widespread that the SDK offers built-in downsampling method in form of [`rs2::decimation_filter`](https://github.com/realsenseai/librealsense/blob/master/doc/post-processing-filters.md#decimation-filter).
It's important to note that using standard OpenCV downsampling is not ideal for depth images. 
In this example we show another correct way to implement depth downsampling. It is conceptually similar to `rs2::decimation_filter`, picking one of the non-zero depth values for every 4x4 block, but unlike `rs2::decimation_filter` it is picking the *closest* depth value instead of median value. This makes sense in context of collision avoidance, since we want to preserve the minimal distance to an object. 

Naive implementation for this approach:
```cpp
for (int y = 0; y < sizeYresized; y++)
        for (int x = 0; x < source.cols; x += DOWNSAMPLE_FACTOR)
        {
            uint16_t min_value = MAX_DEPTH;

            // Loop over 4x4 quad
            for (int i = 0; i < DOWNSAMPLE_FACTOR; i++)
                for (int j = 0; j < DOWNSAMPLE_FACTOR; j++)
                {
                    auto pixel = source.at<uint16_t>(y * DOWNSAMPLE_FACTOR + i, x + j);
                    // Only include non-zero pixels in min calculation
                    if (pixel) min_value = std::min(min_value, pixel);
                }

            // If no non-zero pixels were found, mark the output as zero
            if (min_value == MAX_DEPTH) min_value = 0;

            pDest->at<uint16_t>(y, x / DOWNSAMPLE_FACTOR) = min_value;
        }
```

## Main Filter

The core filter is doing by the following sequence of operations:

```cpp
filter_edges(&sub_areas[i]); // Find edges in the infrared image
filter_harris(&sub_areas[i]); // Find corners in the infrared image
// Combine the two masks together:
cv::bitwise_or(sub_areas[i].edge_mask, sub_areas[i].harris_mask, sub_areas[i].combined_mask);

// morphology: open(src, element) = dilate(erode(src,element))
cv::morphologyEx(sub_areas[i].combined_mask, sub_areas[i].combined_mask,
    cv::MORPH_OPEN, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3)));
// Copy masked depth values:
sub_areas[i].decimated_depth.copyTo(sub_areas[i].output_depth, sub_areas[i].combined_mask);
```

> All OpenCV matrices are split into parts - `sub_areas[i]`. This is done to help parallelize the code, this way each execution thread can run on a seperate image area.


Edge filtering is done using [OpenCV Scharr operator](https://www.tutorialspoint.com/opencv/opencv_scharr_operator.htm):

```cpp
cv::Scharr(area->decimated_ir, area->scharr_x, CV_16S, 1, 0);
cv::convertScaleAbs(area->scharr_x, area->abs_scharr_x);
cv::Scharr(area->decimated_ir, area->scharr_y, CV_16S, 0, 1);
cv::convertScaleAbs(area->scharr_y, area->abs_scharr_y);
cv::addWeighted(area->abs_scharr_y, 0.5, area->abs_scharr_y, 0.5, 0, area->edge_mask);
cv::threshold(area->edge_mask, area->edge_mask, 192, 255, cv::THRESH_BINARY);
```

Corner filtering is done using [OpenCV Harris detector](https://docs.opencv.org/2.4/modules/imgproc/doc/feature_detection.html?highlight=cornerharris):
```cpp
area->decimated_ir.convertTo(area->float_ir, CV_32F);
cv::cornerHarris(area->float_ir, area->corners, 2, 3, 0.04);
cv::threshold(area->corners, area->corners, 300, 255, cv::THRESH_BINARY);
area->corners.convertTo(area->harris_mask, CV_8U);|
```

## Optimisation

To achieve lowest possible latency the code utilizes several optimisation techniques:

1. Custom (min non-zero) depth downsampling takes advantage of SSE4 instruction set when available
2. Image processing is split between several execution threads using OpenMP. `-DBUILD_WITH_OPENMP=True` CMake flag needs to be enabled for this to take effect. 
3. All temporary image buffers are allocated once in `init_matrices` and reused

Here are some rough performance measurements with these optimisations in-place:

|Platform |Average Runtime|
|---|---|
|Intel® Core™ i7-3770 CPU @ 3.40GHz, x 4|650 usec|
|Intel® Core™ i7-4600U CPU @ 2.10GHz x 4|1220 usec|
|Intel® Core™ i5-4250U CPU @ 1.30GHz x 4|2500 usec|
|Intel® Atom™ x5-Z8350 CPU @ 1.44GHz x 4|10000 usec|

> [Intel TBB (Threading Building Blocks)](https://github.com/uxlfoundation/oneTBB) is a good alternative to OpenMP and can be used with minimal code changes

## SDK Integration

The method `sdk_handle` is responsible for converting input frames to `cv::Mat` objects and resulting `cv::Mat` objects to new `rs2::frame` objects. This additional layer ensures seamless interoperability between the algorithm and the SDK. 
Algorithm outputs can be later used for point-cloud generation and export, stream alignment, colorized visualization, and combined with other SDK post-processing blocks. 

Upon detecting new input frame type, `sdk_handle` will generate new SDK video stream profile with decimated resolution and updated intrinsics. 

```cpp
if (!_output_ir_profile || _input_ir_profile.get() != ir_frame.get_profile().get())
{
    auto p = ir_frame.get_profile().as<rs2::video_stream_profile>();
    auto intr = p.get_intrinsics() / DOWNSAMPLE_FACTOR;
    _input_ir_profile = p;
    _output_ir_profile = p.clone(p.stream_type(), p.stream_index(), p.format(),
        p.width() / DOWNSAMPLE_FACTOR, p.height() / DOWNSAMPLE_FACTOR, intr);
}
```

Once output image is ready, it's copied into a new `rs2::frame`:
```cpp
auto res_ir = src.allocate_video_frame(_output_ir_profile, ir_frame, 0,
    newW, newH, ir_frame.get_bytes_per_pixel() * newW);
memcpy((void*)res_ir.get_data(), _decimated_ir.data, newW * newH);
```

Finally the two resulting frames (depth and infrared) are outputed together in a `rs2::frameset`:
```cpp
std::vector<rs2::frame> fs{ res_ir, res_depth };
auto cmp = src.allocate_composite_frame(fs);
src.frame_ready(cmp);
```

Once wrapped as an `rs2::filter` the algorithm can be applied like any other SDK processing block:

```
rs2::frameset data = pipe.wait_for_frames();
data = data.apply_filter(filter);
```
