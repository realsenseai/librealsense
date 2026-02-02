Librealsense Post-Processing Filters

## Table of Contents
* [Filters Description](#post-processing-filters)
  * [Decimation filter](#decimation-filter)
  * [Spatial Edge-Preserving filter](#spatial-filter)
  * [Temporal filter](#temporal-filter)
  * [Holes Filling filter](#hole-filling-filter)
  * [Rotation filter](#rotation-filter)
* [Design and Implementation](#post-processing-implementation)
  * [Using Filters in application code](#post-processing-api-usage)

## Filters Description

Librealsense implementation includes post-processing filters to enhance the quality of depth data and reduce noise levels. All the filters are implemented in the library core as independent blocks to be used in the customer code


### Decimation filter
Effectively reduces the depth scene complexity.
The filter runs on kernel sizes [2x2] to [8x8] pixels. For patches sized 2 and 3 the median depth value is selected. For larger kernels, 4-8 pixels, the mean depth is used due to performance considerations.

The image size is scaled down proportionally in both dimensions to preserve the aspect ratio.
Internally, the filter imposes 4-pixel block alignment for the output frame size width and height. E.g. for input size (1280X720) and scale factor 3 the output size calculation is:  

[1280,720]/3 -> [<span style="color:red">426.6666667</span>, 240] -> [<span style="color:blue">428</span>,240]  
\*The padded rows/columns are zero-filled.

After the resulted frame is produced, the frame intrinsic parameters are recalculated to compensate for the resolution changes.  
The filter also provides some hole filling capability, as the filter uses  valid (non-zero) pixels only.  


| Controls  | Operation |  Range | Default |
:---------: | :-------- | :----- | :-----: |
| Filter Magnitude | The decimation linear scale factor | Discrete steps in [2-8] range | 2

### Spatial Edge-Preserving filter
\*The implementation is based on [paper](http://inf.ufrgs.br/~eslgastal/DomainTransform/) by Eduardo S. L. Gastal and Manuel M. Oliveira.

Key characteristics:
* 1D edge-preserving spatial filter using high-order domain transform.
* Linear-time compute, not affected by the choice of parameters.


The filter performs a series of 1D horizontal and vertical passes or iterations, to enhance the smoothness of the reconstructed data.

Controls | Operation |  Range | Default
:-------:|:----------| :---- | :-----:
Filter Magnitude | Number of filter iterations | [1-5] | 2
Smooth Alpha | The Alpha factor in an exponential moving average with Alpha=1 - no filter. Alpha = 0 - infinite filter | [0.25-1] | 0.5
Smooth Delta | Step-size boundary. Establishes the threshold used to preserve "edges" | Discrete [1-50] | 20
Hole Filling | An in-place heuristic symmetric hole-filling mode applied horizontally during the filter passes. Intended to rectify minor artefacts with minimal performance impact | [0-5] range mapped to [none,2,4,8,16,unlimited] pixels. | 0 (none)


### Temporal filter
The temporal filter is intended to improve the depth data persistency by manipulating per-pixel values based on previous frames.
The filter performs a single pass on the data, adjusting the depth values while also updating the tracking history. In cases where the pixel data is missing or invalid, the filter uses a user-defined persistency mode to decide whether the missing value should be rectified with stored data.
Note that due to its reliance on historic data the filter may introduce visible blurring/smearing artifacts, and therefore is best-suited for static scenes.

Controls | Operation |  Range | Default
:------: | :-------- | :---- | :----:
Smooth Alpha | The Alpha factor in an exponential moving average with Alpha=1 - no filter. Alpha = 0 - infinite filter | [0-1] | 0.4
Smooth Delta |  Step-size boundary. Establishes the threshold used to preserve surfaces (edges) | Discrete [1-100] | 20
Persistency index | A set of predefined rules (masks) that govern when missing pixels will be replaced with the last valid value so that the data will remain persistent over time:<br/> __*Disabled*__ - The Persistency filter is not activated and no hole filling occurs.<br/> __*Valid in 8/8*__ - Persistency activated if the pixel was valid in 8 out of the last 8 frames<br/> __*Valid in 2/last 3*__ - Activated if the pixel was valid in two out of the last 3 frames<br/> __*Valid in 2/last 4*__ - Activated if the pixel was valid in two out of the last 4 frames<br/> __*Valid in 2/8*__ - Activated if the pixel was valid in two out of the last 8 frames<br/> __*Valid in 1/last 2*__ - Activated if the pixel was valid in one of the last two frames<br/> __*Valid in 1/last 5*__ - Activated if the pixel was valid in one out of the last 5 frames<br/> __*Valid in 1/last 8*__ - Activated if the pixel was valid in one out of the last 8 frames<br/> __*Persist Indefinitely*__ - Persistency will be imposed regardless of the stored history (most aggressive filtering) | [0-8] enumerated  | 3 (__*Valid in 2/last 4*__)  

### Holes Filling filter

The filter implements several methods to rectify missing data in the resulting image.
The filter obtains the four immediate pixel "neighbors" (up, down, left, right), and selects one of them according to a user-defined rule.

Controls | Operation |  Range | Default
:------: | :-------- | :---- | :----:
Hole Filling| Control the data that will be used to fill the invalid pixels | [0-2] enumerated:<br/>__*fill_from_left*__ - Use the value from the left neighbor pixel to fill the hole<br/>__*farest_from_around*__ - Use the value from the neighboring pixel which is furthest away from the sensor<br/>__*nearest_from_around*__ - - Use the value from the neighboring pixel closest to the sensor| 1 (Farest from around)

### Rotation filter

The rotation filter transforms depth and IR frames by rotating them by specified angles: 0°, 90°, 180°, and -90°.
This allows re-orienting frames to fit the required perspective for various applications.

After the resulted frame is produced, the frame intrinsic parameters are recalculated to account for rotation.

Controls | Operation |  Range | Default
:------: | :-------- | :---- | :----:
Rotation| The frame rotation in degrees | Discrete steps: 0, 90, 180, -90| 0° 

## Design and Implementation
Post-processing modules are encapsulated into self-contained processing blocks, that provide for the following key requirements:
1. Synchronous/Asynchronous invocation
2. Internal frames memory/lifetime management

The filters are capable to receive and process frames from different sources, though in the general case it is not practical for the following reasons:
- Performance overhead as each time a new type/source of the frame is recognized, some filters require re-initialization.
- The Temporal filter effectiveness relies on the frame's history being preserved. Switching the frame sources invalidates the preserved history and incapacitates the filter.
Therefore establishing and maintaining a filter pipe per camera source is strongly recommended.

The filters preserve the original data and always generate a new (filtered) frame to pass on. The re-generation of new frames allows to share frame among different consumers (thread) without the risk of the data being overwritten by another user.

All filters support discreet as well as floating point input data formats.
The floating point inputs are utilized by D400 stereo-based Depth cameras that support Disparity data representation.
The discreet version of the filters can be applied to D400 devices, though this is not recommended.


## Using Filters in application code
The post-processing blocks are designed and built for concatenation into processing pipes.
There are no software-imposed constraints that mandate the order in which the filters shall be applied.
At the same time the recommended scheme used in librealsense tools and demos is elaborated below:  <br/>
**Depth Frame** >>  **Decimation Filter** >> **Depth2Disparity Transform**<span style="color:blue">\*\*</span> -> **Spatial Filter** >> **Temporal Filter** >> **Disparity2Depth Transform**<span style="color:blue">\*\*</span> >> **Hole Filling Filter** >>  **Filtered Depth**.  <br/>
<span style="color:blue">\*\*</span> Applicable for stereo-based depth cameras (D4XX).  
Note that even though the filter order in the demos is predefined, each filter is controlled individually and can be toggled on/off at run-time.

Demos and tools that have the post-processing code blocks embedded:
1. [RealSense-Viewer](https://github.com/realsenseai/librealsense/tree/master/tools/realsense-viewer)
2. [Depth Quality Tool](https://github.com/realsenseai/librealsense/tree/master/tools/depth-quality)
2. [Post-Processing Demo](https://github.com/realsenseai/librealsense/tree/master/examples/post-processing)

Filter initialization and activation flows:
Using `C` API calls:
```c
// Establishing a frame_queue object for each processing block that will receive the processed frames
rs2_frame_queue* decimated_queue = rs2_create_frame_queue(1, NULL);
rs2_frame_queue* spatial_queue = rs2_create_frame_queue(1, NULL);
...
// Creating processing blocks/ filters
rs2_processing_block* decimation_filter = rs2_create_decimation_filter_block(NULL);
rs2_processing_block* spatial_filter = rs2_create_spatial_filter_block(NULL);
...

// Direct the output of the filters to a dedicated queue
rs2_start_processing_queue(decimation_filter, decimated_queue, NULL);
rs2_start_processing_queue(spatial_filter, spatial_queue, NULL);
...
// Get depth frame from the device
rs2_frame* depth_frame = ...

// Apply decimation filter
rs2_process_frame(decimation_filter, depth_frame, NULL);
rs2_frame* decimated_frame = rs2_wait_for_frame(decimated_queue, 5000, NULL);
// Inject the decimated frame to spatial filter
rs2_process_frame(spatial_filter, decimated_frame, NULL);
// Get the filtered frame
rs2_frame* spatial_filter_frame = rs2_wait_for_frame(spatial_queue, 5000, NULL);
// Use the filtered data
...

// Control filter options
rs2_set_option((rs2_options*)decimation_filter, RS2_OPTION_FILTER_MAGNITUDE, 3, NULL);
rs2_set_option((rs2_options*)spatial_filter, RS2_OPTION_FILTER_SMOOTH_ALPHA, 0.5f, NULL);
```


Using `C++` API:  
```cpp
    // Streaming initialization
    rs2::pipeline pipe;
    ...
    // Declare filters
    rs2::decimation_filter dec_filter;
    rs2::spatial_filter spat_filter;

    // Configure filter parameters
    dec_filter.set_option(RS2_OPTION_FILTER_MAGNITUDE, 3);
    ...
    spat_filter.set_option(RS2_OPTION_FILTER_SMOOTH_ALPHA, 0.55f);
    ...


    // Main Loop
    while (true) {
       rs2::frameset data = pipe.wait_for_frames();
       rs2::frame depth_frame = data.get_depth_frame();
       ...
       rs2::frame filtered = depth_frame;
       // Note the concatenation of output/input frame to build up a chain
       filtered = dec_filter.process(filtered);
       filtered = spatial_filter.process(filtered);

    }
```
