librealsense includes sample integrations with common computer-vision and 3D libraries: OpenCV, PCL (Point Cloud Library), Open3D, the OpenVINO toolkit, dlib, TensorFlow and OpenNI2, plus a point-cloud stitching sample. They live under [`wrappers/`](https://github.com/realsenseai/librealsense/tree/development/wrappers) and show how to move RealSense frames into each library's data types. This page is for developers who want to build those samples, or reuse their patterns in their own applications.

Most of these integrations are example programs, not bindings. They are built together with librealsense when you turn on the matching CMake option. TensorFlow uses only Python scripts, and the Open3D folder has two C++ programs that link only to Open3D.

## Overview

| Integration | Folder | Enabled by | Language | What you get |
|---|---|---|---|---|
| OpenCV | [`wrappers/opencv`](https://github.com/realsenseai/librealsense/tree/development/wrappers/opencv) | `BUILD_CV_EXAMPLES` (+ `BUILD_CV_KINFU_EXAMPLE` for `rs-kinfu`) | C++ | Six samples plus KinectFusion; `cv-helpers.hpp` for `rs2::frame` to `cv::Mat` conversion |
| PCL | [`wrappers/pcl`](https://github.com/realsenseai/librealsense/tree/development/wrappers/pcl) | `BUILD_PCL_EXAMPLES` | C++ | `rs-pcl`, `rs-pcl-color` |
| Open3D | [`wrappers/open3d`](https://github.com/realsenseai/librealsense/tree/development/wrappers/open3d) | `BUILD_OPEN3D_EXAMPLES` | C++ | `RealSenseBagReader`, `RealSenseRecorder` (use Open3D's own RealSense support) |
| OpenVINO | [`wrappers/openvino`](https://github.com/realsenseai/librealsense/tree/development/wrappers/openvino) | `BUILD_OPENVINO_EXAMPLES` | C++ | `rs-face-vino`, `rs-dnn-vino` |
| dlib | [`wrappers/dlib`](https://github.com/realsenseai/librealsense/tree/development/wrappers/dlib) | `BUILD_DLIB_EXAMPLES` | C++ | `rs-face-dlib` |
| OpenNI2 | [`wrappers/openni2`](https://github.com/realsenseai/librealsense/tree/development/wrappers/openni2) | `BUILD_OPENNI2_BINDINGS` | C++ | `rs2driver`, an OpenNI2 driver library |
| Point-cloud stitching | [`wrappers/pointcloud/pointcloud-stitching`](https://github.com/realsenseai/librealsense/tree/development/wrappers/pointcloud/pointcloud-stitching) | `BUILD_PC_STITCHING` | C++ | `rs-pointcloud-stitching` |
| TensorFlow | [`wrappers/tensorflow`](https://github.com/realsenseai/librealsense/tree/development/wrappers/tensorflow) | none (Python scripts) | Python | Three example scripts and two tools |

## Building the integrations

All options are defined in [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake) and default to `OFF`. The top-level `CMakeLists.txt` always adds `wrappers/`. [`wrappers/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/CMakeLists.txt) then adds each integration folder only when its option is on.

| Option | Default | Description string in `lrs_options.cmake` | Folder added |
|---|---|---|---|
| `BUILD_CV_EXAMPLES` | `OFF` | "Build OpenCV examples" | `wrappers/opencv` |
| `BUILD_CV_KINFU_EXAMPLE` | `OFF` | "Build OpenCV KinectFusion example" | `wrappers/opencv/kinfu` (only if `BUILD_CV_EXAMPLES` is also on) |
| `BUILD_PCL_EXAMPLES` | `OFF` | "Build PCL examples" | `wrappers/pcl` |
| `BUILD_OPEN3D_EXAMPLES` | `OFF` | "Build Open3D examples" | `wrappers/open3d` |
| `BUILD_OPENVINO_EXAMPLES` | `OFF` | "Build Intel OpenVINO Toolkit examples - requires INTEL_OPENVINO_DIR" | `wrappers/openvino` |
| `BUILD_DLIB_EXAMPLES` | `OFF` | "Build DLIB examples - requires DLIB_DIR" | `wrappers/dlib` |
| `BUILD_OPENNI2_BINDINGS` | `OFF` | "Build OpenNI bindings" | `wrappers/openni2` |
| `BUILD_PC_STITCHING` | `OFF` | "Build pointcloud-stitching example" | `wrappers/pointcloud/pointcloud-stitching` |

For example, on Linux you can enable OpenCV and PCL in one configure step:

```shell
mkdir build && cd build
cmake .. -DBUILD_CV_EXAMPLES=ON -DBUILD_PCL_EXAMPLES=ON
make -j4
```

The sample targets set an `install(TARGETS ... RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)` rule (the stitching sample uses `${CMAKE_INSTALL_BINDIR}`), so `make install` copies them next to the other RealSense tools. The Open3D programs and the OpenNI2 driver have no install rule. In Visual Studio, the samples appear under the `Examples/OpenCV`, `Examples/PCL`, `Examples/openvino` and `Examples/dlib` solution folders, and the stitching sample under `Examples`.

For the general build flow and the other options see [[Build Configuration]], [[Installation Linux]] and [[Installation Windows]].

### What each option requires

- **`BUILD_CV_EXAMPLES`**: runs `find_package(OpenCV REQUIRED)`. If OpenCV is not installed in a standard location, point `OpenCV_DIR` to the folder that contains `OpenCVConfig.cmake`. The `dnn` sample downloads its model files (`MobileNetSSD_deploy.prototxt` and `MobileNetSSD_deploy.caffemodel`) into its build folder during configuration, so configuring needs network access. The `depth-filter` sample adds `-msse4.1` to its compile flags and copies `camera-settings.json` into its build folder.
- **`BUILD_CV_KINFU_EXAMPLE`**: `rs-kinfu` includes `opencv2/rgbd/kinfu.hpp`, from the `rgbd` module of `opencv_contrib`. Its CMake file also requires `glfw3` 3.3 and OpenGL, and it uses [`examples/example.hpp`](https://github.com/realsenseai/librealsense/blob/development/examples/example.hpp) for rendering. According to the sample readme it needs OpenCV 4 built with `opencv_contrib` and `OPENCV_ENABLE_NONFREE`.
- **`BUILD_PCL_EXAMPLES`**: runs `find_package(PCL REQUIRED)`, needs OpenGL, and on non-Windows systems also runs `find_package(glfw3 REQUIRED)`. `rs-pcl-color` also requires OpenCV. The PCL readme recommends `BUILD_SHARED_LIBS=ON` on Windows, which is the default. It also suggests `-DCMAKE_BUILD_TYPE=RelWithDebInfo` to work around a crash in Debug builds.
- **`BUILD_OPEN3D_EXAMPLES`**: the folder requires CMake 3.12 and looks for an installed Open3D (`find_package(Open3D ...)`). It uses `${CMAKE_INSTALL_PREFIX}/lib/cmake` as a hint on Linux and macOS, and `${CMAKE_INSTALL_PREFIX}/CMake` on Windows. If Open3D 0.12 or newer is not found, configuration stops with `Open3D v0.12+ not found, please use -DCMAKE_INSTALL_PREFIX=open3d_install_dir`. The folder defines its own `STATIC_WINDOWS_RUNTIME` option (default `ON`), which must match the setting Open3D was built with.
- **`BUILD_OPENVINO_EXAMPLES`**: set the cache path `INTEL_OPENVINO_DIR` to your OpenVINO installation. Otherwise configuration stops with `Invalid OpenVINO directory specified with INTEL_OPENVINO_DIR`. The script includes `${INTEL_OPENVINO_DIR}/inference_engine/share/InferenceEngineConfig.cmake`, so it only works with OpenVINO installations that have this Inference Engine layout. Both samples also need `OpenCV_DIR` (`OpenVINO examples require OpenCV; specify OpenCV_DIR`). The pre-trained models are downloaded into each sample's build folder during configuration (see [OpenVINO](#openvino)).
- **`BUILD_DLIB_EXAMPLES`**: set `DLIB_DIR` to an extracted dlib source tree. Otherwise configuration stops with `Invalid DLIB directory specified with DLIB_DIR`. dlib is compiled as part of the librealsense build (`add_subdirectory(${DLIB_DIR} build)`).
- **`BUILD_OPENNI2_BINDINGS`**: builds the shared library `rs2driver` into `<build>/_out`. Include paths come from the cache paths `OPENNI2_DIR` (default `c:/Program Files/OpenNI2`) and `REALSENSE2_DIR` (default `c:/Program Files (x86)/Intel RealSense SDK 2.0`). On Linux, macOS and other Unix-like systems (CMake `UNIX`), set `OPENNI2_DIR` to the folder that contains the OpenNI2 headers. On Windows the build adds `OPENNI2_DIR/Include`.
- **`BUILD_PC_STITCHING`**: if `BUILD_GLSL_EXTENSIONS` is off, the top-level `CMakeLists.txt` turns it on. It also builds the bundled GLFW when GLFW 3.3 is not found. The sample needs OpenGL.

## OpenCV

The OpenCV samples complement the core [SDK examples](https://github.com/realsenseai/librealsense/tree/development/examples) (see [[Code Samples]]). Each sample is a single executable linked against `realsense2` and `${OpenCV_LIBS}`.

| Sample | Executable | Description |
|---|---|---|
| [imshow](https://github.com/realsenseai/librealsense/tree/development/wrappers/opencv/imshow) | `rs-imshow` | Minimal OpenCV application that shows colorized depth in an OpenCV window |
| [grabcuts](https://github.com/realsenseai/librealsense/tree/development/wrappers/opencv/grabcuts) | `rs-grabcuts` | Foreground extraction with the GrabCut algorithm, initialized from depth |
| [latency-tool](https://github.com/realsenseai/librealsense/tree/development/wrappers/opencv/latency-tool) | `rs-latency-tool` | Rough visual-latency estimate: renders a binary clock on screen and decodes it from the camera image |
| [dnn](https://github.com/realsenseai/librealsense/tree/development/wrappers/opencv/dnn) | `rs-dnn` | MobileNet-SSD (Caffe) object detection with OpenCV DNN; uses depth to estimate distance to each object |
| [depth-filter](https://github.com/realsenseai/librealsense/tree/development/wrappers/opencv/depth-filter) | `rs-depth-filter` | Depth post-processing for collision avoidance, with a high-confidence preset in `camera-settings.json` |
| [rotate-pointcloud](https://github.com/realsenseai/librealsense/tree/development/wrappers/opencv/rotate-pointcloud) | `rs-rotate-pc` | Rotates the point cloud before colorization (`c` changes mode, `q` exits) |
| [kinfu](https://github.com/realsenseai/librealsense/tree/development/wrappers/opencv/kinfu) | `rs-kinfu` | 3D reconstruction with OpenCV's KinectFusion (`cv::kinfu`); needs `BUILD_CV_KINFU_EXAMPLE` |

### Converting frames to `cv::Mat`

[`wrappers/opencv/cv-helpers.hpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/opencv/cv-helpers.hpp) has two helpers that the `grabcuts`, `dnn` and `latency-tool` samples use:

- `frame_to_mat(const rs2::frame&)` wraps the frame buffer without copying for `RS2_FORMAT_BGR8` (`CV_8UC3`), `RS2_FORMAT_Z16` (`CV_16UC1`), `RS2_FORMAT_Y8` (`CV_8UC1`) and `RS2_FORMAT_DISPARITY32` (`CV_32FC1`). It converts `RS2_FORMAT_RGB8` into a new BGR matrix, and throws `std::runtime_error` for other formats.
- `depth_frame_to_meters(const rs2::depth_frame&)` returns a `CV_64F` matrix of distances in meters (raw values multiplied by `get_units()`).

The `cv::Mat` objects that wrap the buffer are valid only while the `rs2::frame` exists. Call `clone()` if you need to keep the image.

### Example: show aligned color and depth

This program combines [`rs-imshow.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/opencv/imshow/rs-imshow.cpp) with the alignment step from [`rs-grabcuts.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/opencv/grabcuts/rs-grabcuts.cpp). It streams with the default configuration, aligns depth to color, and shows the color image next to the colorized depth image:

```cpp
#include <librealsense2/rs.hpp>
#include <opencv2/opencv.hpp>
#include "cv-helpers.hpp"   // from wrappers/opencv

int main() try
{
    rs2::colorizer color_map;
    rs2::align align_to(RS2_STREAM_COLOR);

    rs2::pipeline pipe;
    pipe.start();                                   // default configuration

    const auto window_name = "RealSense";
    cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);

    while (cv::waitKey(1) < 0 && cv::getWindowProperty(window_name, cv::WND_PROP_AUTOSIZE) >= 0)
    {
        rs2::frameset data = align_to.process(pipe.wait_for_frames());
        rs2::frame color = data.get_color_frame();
        rs2::frame depth = data.get_depth_frame().apply_filter(color_map);

        cv::Mat color_mat = frame_to_mat(color);    // BGR8 / RGB8 -> CV_8UC3
        cv::Mat depth_mat = frame_to_mat(depth);    // colorizer output is RGB8 -> CV_8UC3

        cv::Mat both;
        cv::hconcat(color_mat, depth_mat, both);
        cv::imshow(window_name, both);
    }
    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
```

This needs a device that streams both depth and color with the default configuration. For a depth-only view, use `rs-imshow`, which builds the `cv::Mat` directly:

```cpp
const int w = depth.as<rs2::video_frame>().get_width();
const int h = depth.as<rs2::video_frame>().get_height();
cv::Mat image(cv::Size(w, h), CV_8UC3, (void*)depth.get_data(), cv::Mat::AUTO_STEP);
```

### Building outside the librealsense tree

The repository guide [`doc/stepbystep/getting_started_with_openCV.md`](https://github.com/realsenseai/librealsense/blob/development/doc/stepbystep/getting_started_with_openCV.md) walks through small stand-alone programs that show color, infrared and depth frames with OpenCV. They are compiled directly against an installed SDK, for example:

```shell
g++ -std=c++11 BGR_sample.cpp -lrealsense2 -lopencv_core -lopencv_highgui -o BGR && ./BGR
```

For Python, the [`opencv_viewer_example.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/opencv_viewer_example.py) and [`opencv_pointcloud_viewer.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/python/examples/opencv_pointcloud_viewer.py) examples show the same integration with NumPy (see [[Python Wrapper]]).

## PCL

| Sample | Executable | Description |
|---|---|---|
| [pcl](https://github.com/realsenseai/librealsense/tree/development/wrappers/pcl/pcl) | `rs-pcl` | Captures a depth frame, converts it to `pcl::PointCloud` and applies a `PassThrough` filter. Points that pass the filter are drawn in green and the rest in red. |
| [pcl-color](https://github.com/realsenseai/librealsense/tree/development/wrappers/pcl/pcl-color) | `rs-pcl-color` | Same flow with RGB color per point (`pcl::PointXYZRGB`); writes the unfiltered colored cloud to a `Captured_Frame<N>.pcd` file and shows it in a PCL viewer (the `PassThrough` result is not used) |

The conversion takes an `rs2::points` object from `rs2::pointcloud::calculate()` and copies its vertices into `pcl::PointXYZ` (or `pcl::PointXYZRGB`) points. See [`rs-pcl.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/pcl/pcl/rs-pcl.cpp) and [`rs-pcl-color.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/pcl/pcl-color/rs-pcl-color.cpp).

## Open3D

Open3D has its own RealSense support (`open3d::t::io::RealSenseSensor` and `RSBagReader`), and you enable it when you build Open3D. The [`wrappers/open3d`](https://github.com/realsenseai/librealsense/tree/development/wrappers/open3d) folder has two C++ programs that use that support. They link only to `Open3D::Open3D`:

| Program | Source | Usage (printed by the program) |
|---|---|---|
| `RealSenseBagReader` | [`cpp/RealSenseBagReader.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/open3d/cpp/RealSenseBagReader.cpp) | `RealSenseBagReader [-V] --input input.bag [--output path]`: plays the color and depth streams of a bag file; with `--output`, also writes the frames as images |
| `RealSenseRecorder` | [`cpp/RealSenseRecorder.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/open3d/cpp/RealSenseRecorder.cpp) | `RealSenseRecorder [-h\|--help] [-V] [-l\|--list-devices] [--align] [--record rgbd_video_file.bag] [-c\|--config rs-config.json]`: shows live capture and optionally records to a bag file; a JSON config file is required |

To build them, install Open3D 0.12 or newer and configure librealsense with `-DBUILD_OPEN3D_EXAMPLES=ON -DCMAKE_INSTALL_PREFIX=<open3d_install_dir>` (see [What each option requires](#what-each-option-requires)). On Windows, if Open3D is a shared library, the build copies `Open3D.dll` into `<build>/wrappers/open3d/<config>`, not into the folder that holds the executables.

For Python, Open3D's own package provides the same classes (`o3d.t.io.RealSenseSensor`, `o3d.t.io.RSBagReader`). The [Open3D readme](https://github.com/realsenseai/librealsense/blob/development/wrappers/open3d/readme.md) has C++ and Python snippets for listing devices, configuring a camera with a JSON file, capturing and recording, and for passing a bag file to Open3D's reconstruction pipeline. Open3D's API documentation is at [open3d.org](http://www.open3d.org/docs/latest/introduction.html).

## OpenVINO

| Sample | Executable | Description |
|---|---|---|
| [face](https://github.com/realsenseai/librealsense/tree/development/wrappers/openvino/face) | `rs-face-vino` | Face detection with the `face-detection-adas-0001` model; uses depth to estimate distance |
| [dnn](https://github.com/realsenseai/librealsense/tree/development/wrappers/openvino/dnn) | `rs-dnn-vino` | Object detection with MobileNet-SSD (`mobilenet-ssd.xml` / `.bin` / `.labels`), the same network as the OpenCV `dnn` sample |

Both samples share helper classes in [`wrappers/openvino/rs-vino`](https://github.com/realsenseai/librealsense/tree/development/wrappers/openvino/rs-vino) (`openvino_helpers::object_detection` and related classes) and log with EasyLogging++. The sample readmes say they need a camera with both depth and RGB sensors.

When you configure, [`check_vino_version.cmake`](https://github.com/realsenseai/librealsense/blob/development/wrappers/openvino/check_vino_version.cmake) checks which OpenVINO API the installation provides:

- If `inference_engine/src/extension/ext_list.hpp` exists, it treats the installation as "2019 or lower" and links `ie_cpu_extension`.
- If `deployment_tools/ngraph/include/ngraph/ngraph.hpp` exists, it treats the installation as supporting nGraph ("2020.1 or higher") and links the nGraph libraries.

[`dl_vino_model.cmake`](https://github.com/realsenseai/librealsense/blob/development/wrappers/openvino/dl_vino_model.cmake) downloads each model file into the sample's build folder, for example `build/wrappers/openvino/face`, and checks its SHA1. Each version family has its own set of files. Run the sample from the folder that contains the model files. When `BUILD_WITH_CPU_EXTENSIONS` (default `ON`) is off, the OpenVINO CMake script also turns off `ENABLE_AVX2` and `ENABLE_AVX512F`.

In builds with `BUILD_SHARED_LIBS`, the samples no longer initialize their own EasyLogging++ storage on Linux; they use the storage that librealsense exports *(changed in the next release)*. In v2.59.1, both samples initialized it in every shared-library build.

According to the [OpenVINO readme](https://github.com/realsenseai/librealsense/blob/development/wrappers/openvino/readme.md), the samples were tested with OpenVINO releases from 2019.3 to 2021.3. It also explains how to use the OpenCV that comes with OpenVINO (`-DOpenCV_DIR=<openvino-dir>/opencv/cmake`).

## dlib

| Sample | Executable | Description |
|---|---|---|
| [face](https://github.com/realsenseai/librealsense/tree/development/wrappers/dlib/face) | `rs-face-dlib` | Facial landmark detection; uses depth for a simple anti-spoofing check (landmarks drawn in green for a face that passes, red otherwise) |

[`rs_frame_image.h`](https://github.com/realsenseai/librealsense/blob/development/wrappers/dlib/rs_frame_image.h) lets dlib use an `rs2::frame` as an image without copying, for example `rs_frame_image< dlib::rgb_pixel, RS2_FORMAT_RGB8 > image( color_frame );`.

At runtime, the sample loads `shape_predictor_68_face_landmarks.dat` from the current directory. If the file is missing, it prints `You need dlib's default face landmarking model file to run this example.` with the download URL `http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2`. The dlib readme says the sample was tested with dlib 19.17.

## OpenNI2

[`wrappers/openni2`](https://github.com/realsenseai/librealsense/tree/development/wrappers/openni2) builds `rs2driver`, a driver that lets OpenNI2 applications use RealSense devices. It exposes depth (`ONI_SENSOR_DEPTH`), color (`ONI_SENSOR_COLOR`) and infrared (`ONI_SENSOR_IR`) streams.

To use it:

1. Configure with `-DBUILD_OPENNI2_BINDINGS=ON` and set `OPENNI2_DIR` (and `REALSENSE2_DIR` if needed).
2. Build. The library is written to `<build>/_out` (`<build>/_out/<config>` with Visual Studio and other multi-config generators).
3. Copy the driver and the RealSense library into the OpenNI2 drivers folder. The [OpenNI2 readme](https://github.com/realsenseai/librealsense/blob/development/wrappers/openni2/README.md) gives `OPENNI2_DIR/Samples/Bin/OpenNI2/Drivers/`:
   - Windows: `rs2driver.dll` and `realsense2.dll`
   - Linux: `librs2driver.so` and `librealsense2.so`
4. Run an OpenNI2 application, such as the OpenNI2 samples in `OPENNI2_DIR/Samples/Bin/`.

The readme also lists recording and playback, depth-to-color mapping and NiTE2 user tracking as supported.

## Point-cloud stitching

[`rs-pointcloud-stitching`](https://github.com/realsenseai/librealsense/tree/development/wrappers/pointcloud/pointcloud-stitching) combines two devices into one wide field-of-view virtual device. It projects both point clouds onto the image of a virtual pinhole camera. Build it with `-DBUILD_PC_STITCHING=ON` and run:

```
rs-pointcloud-stitching <working_directory> [calibration_file]
```

When run without arguments, or with `-h`/`--help`, it prints its usage text. The text explains that exactly two RealSense devices must be connected, and that `<working_directory>` must contain a `<serial_no>.cfg` stream configuration for each camera and a `virtual_dev.cfg` that describes the combined camera. The [stitching guide](https://github.com/realsenseai/librealsense/blob/development/wrappers/pointcloud/pointcloud-stitching/doc/pointcloud-stitching-demo.md) covers mounting, calibration with a checkerboard (using the MATLAB scripts in the folder) and the configuration files.

## TensorFlow

[`wrappers/tensorflow`](https://github.com/realsenseai/librealsense/tree/development/wrappers/tensorflow) is a Python tutorial. It has no CMake option and is not compiled. The three example scripts and `tools/convert_to_bag.py` import `pyrealsense2`, `numpy` and `cv2`, and the first two examples also import `tensorflow`. `tools/rmse.py` uses `cv2`, `numpy` and `matplotlib` without `pyrealsense2`. The repository contains no notebooks; the material is the [README](https://github.com/realsenseai/librealsense/blob/development/wrappers/tensorflow/README.md) plus these scripts:

| Script | Description |
|---|---|
| [`example1 - object detection.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/tensorflow/example1%20-%20object%20detection.py) | Object detection on the 1280x720 BGR8 color stream with a frozen TensorFlow graph (`frozen_inference_graph.pb` in the working directory) |
| [`example2 - person height.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/tensorflow/example2%20-%20person%20height.py) | Adds the depth stream to detect people and estimate their height |
| [`example3 - opencv deploy.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/tensorflow/example3%20-%20opencv%20deploy.py) | Runs the same model with OpenCV DNN (`cv2.dnn.readNetFromTensorflow("frozen_inference_graph.pb", "faster_rcnn_inception_v2_coco_2018_01_28.pbtxt")`) |
| [`tools/rmse.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/tensorflow/tools/rmse.py) | Plane-fit RMS of a selected region in a depth image (file path as the first argument) |
| [`tools/convert_to_bag.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/tensorflow/tools/convert_to_bag.py) | Converts a folder of depth, ground-truth and IR images into `1.bag` in that folder (folder path as the first argument) through a software device and `rs.recorder` |

The model files are not in the repository. The README links to the TensorFlow Object Detection API model list. The bag files written by `convert_to_bag.py` can be opened in the [[Realsense Viewer Manual]] (see also [[Record and Playback]]).

## See also

- [`wrappers/readme.md`](https://github.com/realsenseai/librealsense/blob/development/wrappers/readme.md): list of all wrappers
- [[Python Wrapper]], [[CSharp Wrapper]], [[Unity and Unreal Engine]], [[MATLAB and LabVIEW]], [[REST API and Web Viewer]]
- [[Code Samples]], [[Post Processing Filters]], [[Projection in RealSense SDK 2.0]]

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`46cf1a6`](https://github.com/realsenseai/librealsense/commit/46cf1a67cc441ad9f55ce339a0de30f2a1c6fed0)._
<!-- /wikigen:footer -->
