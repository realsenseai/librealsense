The MATLAB wrapper lets you control RealSense cameras from MATLAB through a `realsense` package of MATLAB classes backed by a single MEX file, `librealsense_mex`. Its sources live in [`wrappers/matlab`](https://github.com/realsenseai/librealsense/tree/development/wrappers/matlab). The LabVIEW folder, [`wrappers/labview`](https://github.com/realsenseai/librealsense/tree/development/wrappers/labview), holds only a readme that points to a separately distributed VI library. This page covers building and setting up the MATLAB package, a first program, the bundled examples, the class reference and a summary of the LabVIEW readme.

## MATLAB: how the wrapper is put together

- [`librealsense_mex.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/matlab/librealsense_mex.cpp) (with `Factory.cpp`, `Factory.h`, `MatlabParamParser.h`, `rs2_type_traits.h` and `types.h`) builds one MEX function. MATLAB code calls it as `realsense.librealsense_mex('<C++ class>', '<method>', handle, args...)`, for example `realsense.librealsense_mex('rs2::pipeline', 'start', ...)`. The MEX file has a table of librealsense C++ classes (`rs2::pipeline`, `rs2::config`, `rs2::frame`, `rs400::advanced_mode` and others) and calls the C++ method you ask for.
- Each class file that wraps librealsense (for example `pipeline.m` or `colorizer.m`) wraps one C++ class and keeps the native object handle in `objectHandle`. The enumeration files (for example `stream.m`) are plain `int64` enumerations.
- Because the wrapper class files call the MEX file and each other with the `realsense.` prefix, and the examples call the classes the same way, the `.m` files and the compiled MEX file must all be in a MATLAB package folder called `+realsense`. MATLAB must be able to find the parent folder of `+realsense` (it can be your current folder or a folder on the MATLAB path).

If the MEX function gets a bad request, it raises errors from `librealsense_mex.cpp` such as `At least class and command name are needed.`, `Unknown Command received.`, or `<class>::<method>: Wrong number of outputs` / `Wrong number of inputs`. If the called method throws a C++ exception (for example a librealsense error), the exception's text becomes the MATLAB error; any other failure gives `An unknown error occured`.

## Building the MATLAB wrapper with CMake

The wrapper is part of the main librealsense CMake build. Turn it on with `BUILD_MATLAB_BINDINGS`, which defaults to `OFF` (defined in [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake)). When the option is on, [`wrappers/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/CMakeLists.txt) adds `wrappers/matlab`. For the other build options, see [[Build Configuration]].

| Item | Value (from [`wrappers/matlab/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/matlab/CMakeLists.txt)) |
|---|---|
| Option | `BUILD_MATLAB_BINDINGS` (default `OFF`) |
| MATLAB lookup | `find_package(Matlab COMPONENTS MX_LIBRARY MAT_LIBRARY REQUIRED)`, so configuration fails if CMake cannot find a MATLAB installation |
| Target | `librealsense_mex`, created with CMake's `matlab_add_mex`, shown under the `Wrappers` folder in the IDE |
| Links to | `realsense2` and the MATLAB MAT library |
| Linker flag | Always adds `/NODEFAULTLIB:realsense2`. This is a workaround for Debug builds with MSVC. |

The wrapper's [readme](https://github.com/realsenseai/librealsense/blob/development/wrappers/matlab/readme.md) only describes using it on Windows. Example configure and build on Windows:

```
git clone https://github.com/realsenseai/librealsense.git
cd librealsense
mkdir build
cd build
cmake .. -DBUILD_MATLAB_BINDINGS:BOOL=ON
cmake --build . --config Release --target librealsense_mex
```

On Windows, [`CMake/windows_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/windows_config.cmake) sends build outputs to the build directory. With the Visual Studio generator they land in `build/<Configuration>` (for example `build/Release`), next to `realsense2.dll`.

### Setting up the `+realsense` package after a CMake build

The CMake build only compiles the MEX file. It does **not** copy the `.m` files and does not create a `+realsense` folder. The CMakeLists file has a `TODO` noting that the MEX file and the `.m` files still need to be copied into a `+realsense` folder. Do these steps yourself:

1. Create a folder named `+realsense`. Its parent can be any folder MATLAB can reach.
2. Copy the built `librealsense_mex` MEX file from `build/<Configuration>` into it.
3. Copy all `*.m` files from `wrappers/matlab` into it. This includes the example scripts, because they are called as `realsense.<example>`.
4. Make sure Windows can find `realsense2.dll` from the same build when MATLAB loads the MEX file. One way is to add its folder to `PATH`.
5. In MATLAB, `cd` to the parent of `+realsense` or add the parent to the MATLAB path (`addpath`).

## Building with the Visual Studio project

`wrappers/matlab` also has a standalone Visual Studio solution, [`librealsense_mex.sln`](https://github.com/realsenseai/librealsense/blob/development/wrappers/matlab/librealsense_mex.sln), with the project [`librealsense_mex.vcxproj`](https://github.com/realsenseai/librealsense/blob/development/wrappers/matlab/librealsense_mex.vcxproj). The project file shows the following:

- It has `Debug` and `Release` configurations for `Win32` and `x64`. The platform toolset is `v140`.
- It builds a `.mexw64` file into `$(SolutionDir)$(Platform)\$(Configuration)\+realsense\`. In the x64 configurations a pre-build step (`xcopy /y $(ProjectDir)*.m $(OutDir)`, "Copy Matlab files to output dir") copies the `.m` files there, so the output folder is a ready-to-use `+realsense` package.
- MATLAB include and library paths are hard-coded to `C:\MATLAB\R2017a` and `C:\MATLAB\R2017b`. librealsense libraries are expected under `..\..\build\$(Configuration)`.
- It links `libmx.lib`, `libmex.lib`, `libmat.lib`, `realsense2.lib`, `realsense-file.lib`, `fw.lib`, `tm.lib` and `usb.lib`. It also searches `build\third-party\libtm\...` and `build\libusb_install\lib` for libraries.

The current tree has no `third-party/libtm` folder, so the `fw.lib` and `tm.lib` dependencies don't match the current build output. The default Windows build does not build libusb, and when it does (for example with `FORCE_RSUSB_BACKEND`), the library it installs is `libusb-1.0.lib`, not `usb.lib`. To use this project, you would need to edit its MATLAB paths and library list. The CMake path above is the one that follows the current build.

## First program

[`depth_example.m`](https://github.com/realsenseai/librealsense/blob/development/wrappers/matlab/depth_example.m) streams from the first available camera with default settings, takes a depth frame, colorizes it and shows it. Run it from the MATLAB prompt:

```matlab
realsense.depth_example
```

The core of the example:

```matlab
% Make Pipeline object to manage streaming
pipe = realsense.pipeline();
% Make Colorizer object to prettify depth output
colorizer = realsense.colorizer();

% Start streaming on an arbitrary camera with default settings
profile = pipe.start();

% Get streaming device's name
dev = profile.get_device();
name = dev.get_info(realsense.camera_info.name);

% Discard the first frames to let the camera settle
for i = 1:5
    fs = pipe.wait_for_frames();
end
pipe.stop();

% Colorize the depth frame
depth = fs.get_depth_frame();
color = colorizer.colorize(depth);

% Color data arrives as an [R, G, B, R, G, B, ...] vector
data = color.get_data();
img = permute(reshape(data', [3, color.get_width(), color.get_height()]), [3 2 1]);
imshow(img);
title(sprintf("Colorized depth frame from %s", name));
```

### Reading a distance

`realsense.depth_frame` has `get_distance(x, y)`, which returns meters, and `get_units()`. `realsense.config.enable_stream` can request a specific stream profile. The call below takes stream, width, height, format and frame rate, and the device must support the profile you ask for:

```matlab
cfg = realsense.config();
cfg.enable_stream(realsense.stream.depth, 640, 480, realsense.format.z16, 30);

pipe = realsense.pipeline();
pipe.start(cfg);
fs = pipe.wait_for_frames();
depth = fs.get_depth_frame();
w = depth.get_width();
h = depth.get_height();
fprintf("Distance at the image center: %.3f m\n", depth.get_distance(floor(w/2), floor(h/2)));
pipe.stop();
```

`wait_for_frames` also takes an optional timeout in milliseconds, given as a number or a `duration`. `pipeline.start` takes no arguments, a `realsense.config`, a `realsense.frame_queue`, or a config together with a frame queue.

## Example scripts

All examples are in [`wrappers/matlab`](https://github.com/realsenseai/librealsense/tree/development/wrappers/matlab). Run them as `realsense.<name>` once the package is set up.

| Example | What it does |
|---|---|
| [`depth_example.m`](https://github.com/realsenseai/librealsense/blob/development/wrappers/matlab/depth_example.m) | Function. Streams with default settings, colorizes one depth frame and shows it with `imshow`. |
| [`rosbag_example.m`](https://github.com/realsenseai/librealsense/blob/development/wrappers/matlab/rosbag_example.m) | Function `rosbag_example(filename)`. Plays a recorded file through `config.enable_device_from_file` and shows a colorized depth frame. See [[Record and Playback]]. |
| [`pointcloud_example.m`](https://github.com/realsenseai/librealsense/blob/development/wrappers/matlab/pointcloud_example.m) | Function. Runs `realsense.pointcloud.calculate` on live depth frames and draws the vertices with `plot3` in a loop. |
| [`depth_view_example.m`](https://github.com/realsenseai/librealsense/blob/development/wrappers/matlab/depth_view_example.m) | `matlab.apps.AppBase` app. Shows live colorized depth and color side by side, with Start/Stop buttons and a timer. It puts the serial number of the first device from `realsense.context().query_devices()` into a `realsense.config`, but starts the pipeline without that config. |
| [`capture_example.m`](https://github.com/realsenseai/librealsense/blob/development/wrappers/matlab/capture_example.m) | `matlab.apps.AppBase` app. Shows depth, color, accelerometer and gyro data for whichever of these streams the device's default profile has. Streams it doesn't have are marked "not supported". |
| [`advanced_mode_example.m`](https://github.com/realsenseai/librealsense/blob/development/wrappers/matlab/advanced_mode_example.m) | Function. Checks `dev.is('advanced_mode')`, turns advanced mode on if needed, reads the settings with `serialize_json`, changes `controls-color-gain` and uploads them with `load_json`. See [[D400 Advanced Mode]]. |

If no camera is connected, `depth_view_example` and `capture_example` stop with the error `Not found RealSense device`. Both apps print this message.

The CMakeLists file lists `depth_example.m`, `pointcloud_example.m`, `rosbag_example.m`, `depth_view_example.m` and `capture_example.m` as `MATLAB_EXAMPLES`. `advanced_mode_example.m` and the `advanced_mode.m` class it needs are in the folder but not in the CMake or Visual Studio file lists. After a CMake build, setup step 3 above copies them along with all other `*.m` files; in the x64 configurations of the Visual Studio project, the pre-build `xcopy` of `*.m` copies them too.

## Class reference

All classes are in the `realsense` package (`realsense.<name>`). Handle classes wrap a native librealsense object. Enumeration classes derive from `int64` and mirror the C API enums.

### Core and device classes

| Class | Base | Wraps |
|---|---|---|
| `realsense.context` | `handle` | `rs2::context` |
| `realsense.device` | `handle` | `rs2::device` |
| `realsense.advanced_mode` | `realsense.device` | `rs400::advanced_mode` |
| `realsense.playback` | `realsense.device` | `rs2::playback` |
| `realsense.recorder` | `realsense.device` | `rs2::recorder` |
| `realsense.device_hub` | `handle` | `rs2::device_hub` |
| `realsense.options` | `handle` | `rs2::options` |
| `realsense.sensor` | `realsense.options` | `rs2::sensor` |
| `realsense.roi_sensor` | `realsense.sensor` | `rs2::roi_sensor` |
| `realsense.depth_sensor` | `realsense.sensor` | `rs2::depth_sensor` |
| `realsense.depth_stereo_sensor` | `realsense.depth_sensor` | `rs2::depth_stereo_sensor` |
| `realsense.pipeline` | `handle` | `rs2::pipeline` |
| `realsense.pipeline_profile` | `handle` | `rs2::pipeline_profile` |
| `realsense.config` | `handle` | `rs2::config` |
| `realsense.syncer` | `handle` | `rs2::syncer` |
| `realsense.frame_queue` | `handle` | `rs2::frame_queue` |

### Stream profiles and frames

| Class | Base | Wraps |
|---|---|---|
| `realsense.stream_profile` | `handle` | `rs2::stream_profile` |
| `realsense.video_stream_profile` | `realsense.stream_profile` | `rs2::video_stream_profile` |
| `realsense.motion_stream_profile` | `realsense.stream_profile` | `rs2::motion_stream_profile` |
| `realsense.frame` | `handle` | `rs2::frame` |
| `realsense.frameset` | `realsense.frame` | `rs2::frameset` |
| `realsense.video_frame` | `realsense.frame` | `rs2::video_frame` |
| `realsense.depth_frame` | `realsense.video_frame` | `rs2::depth_frame` |
| `realsense.disparity_frame` | `realsense.depth_frame` | `rs2::disparity_frame` |
| `realsense.motion_frame` | `realsense.frame` | `rs2::motion_frame` |
| `realsense.pose_frame` | `realsense.frame` | `rs2::pose_frame` |
| `realsense.points` | `realsense.frame` | `rs2::points` |

`realsense.frame` has `is(type)` and `as(type)` for type checks and casts by name, for example `fs.first(realsense.stream.accel).as('motion_frame')` in `capture_example.m`. It also has `get_data`, `get_timestamp`, `get_frame_number`, `get_frame_metadata` and `supports_frame_metadata`.

### Processing blocks

All of these derive from `realsense.filter`, which derives from `realsense.options`. See [[Post Processing Filters]].

| Class | Wraps |
|---|---|
| `realsense.filter` | `rs2::filter` |
| `realsense.align` | `rs2::align` |
| `realsense.colorizer` | `rs2::colorizer` |
| `realsense.pointcloud` | `rs2::pointcloud` |
| `realsense.decimation_filter` | `rs2::decimation_filter` |
| `realsense.spatial_filter` | `rs2::spatial_filter` |
| `realsense.temporal_filter` | `rs2::temporal_filter` |
| `realsense.hole_filling_filter` | `rs2::hole_filling_filter` |
| `realsense.disparity_transform` | `rs2::disparity_transform` |
| `realsense.save_to_ply` | `rs2::save_to_ply` |
| `realsense.save_single_frameset` | `rs2::save_single_frameset` |

### Enumerations

| Class | Mirrors |
|---|---|
| `realsense.camera_info` | `rs2_camera_info` (for example `realsense.camera_info.name`, `realsense.camera_info.serial_number`) |
| `realsense.stream` | `rs2_stream` (for example `realsense.stream.depth`, `realsense.stream.color`) |
| `realsense.format` | `rs2_format` (for example `realsense.format.z16`, `realsense.format.rgb8`) |
| `realsense.option` | `rs2_option` |
| `realsense.frame_metadata_value` | `rs2_frame_metadata_value` |
| `realsense.timestamp_domain` | `rs2_timestamp_domain` |
| `realsense.playback_status` | `rs2_playback_status` |

The enumeration values are written out in the `.m` files, so check the file (for example [`option.m`](https://github.com/realsenseai/librealsense/blob/development/wrappers/matlab/option.m)) for the values that are available.

## LabVIEW

[`wrappers/labview`](https://github.com/realsenseai/librealsense/tree/development/wrappers/labview) has only a [`readme.md`](https://github.com/realsenseai/librealsense/blob/development/wrappers/labview/readme.md). The repository has no LabVIEW code, VIs or build steps, so nothing in this section can be checked against code. The repository guide says the following:

- A separately downloaded VI library wraps most of the core functions of `realsense2.dll`. The readme links to several versions, the newest labelled "RealSense SDK2 for LabVIEW 1.0, 2.33.0, Feb 2020".
- The package targets LabVIEW 2016 on 64-bit Windows 10. It ships a 64-bit `realsense2.dll`. To use a different build, compile `realsense2.dll` yourself (see [[Installation Windows]]) and replace the file the LabVIEW demos load.
- It includes five "Hello World" demos: Color + Depth, Left-Right, Left-Right UnRectified, Point Cloud, and Point Cloud Plus (point cloud with temporal and spatial filtering).
- Programming model: enumerate devices, sensors and streams to find the profiles you want. Pass them to the Configure VI, start with the Start VI, call the Capture VI in a loop and read the payload with Get Frame Data. Release every frame with the Release Frame VI, or streaming soon stops. Close everything when done.

For the device, sensor and stream hierarchy that the LabVIEW VIs follow, see [[API Architecture]].

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`46cf1a6`](https://github.com/realsenseai/librealsense/commit/46cf1a67cc441ad9f55ce339a0de30f2a1c6fed0)._
<!-- /wikigen:footer -->
