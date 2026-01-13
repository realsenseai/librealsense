# Dlib Samples for RealSense cameras
Examples in this folder are designed to complement existing [SDK examples](../../examples) and demonstrate how Intel RealSense cameras can be used together with `dlib` in domain of computer-vision.

> RealSense examples have been designed and tested with dlib version 19.17,
> Working with newer version may require code changes.

## List of Samples:
1. [Face](./face) - Facial recognition with simple anti-spoofing

## Getting Started:
This page is certainly **not** a comprehensive guide to getting started with Dlib and CMake, but it can help get on the right track.

* [Windows Installation](#windows)
* [Linux Installation](#linux)

### Windows
This section describes how to use CMake to generate a VisualStudio project to build the Dlib library and a VisualStudio project to build the Dlib samples.

First, download and install `CMake` from [cmake.org/download](https://cmake.org/download/)

#### Building the Dlib samples as part of librealsense's VisualStudio project
1. Download the latest Dlib release from [dlib.net](http://dlib.net/) and extract into a local directory (`C:/work/dlib-19.17`)
2. Enable the ENABLED_DLIB_EXAMPLES entry in the CMake configuration for your copy of librealsense
3. Point the DLIB_DIR entry to the location of your Dlib installation (`C:/work/dlib-19.17`)
4. Click `Configure`, then `Generate`
5. Click `Open Project` to open Visual Studio (2015 or later, as required by librealsense)
6. Press `Ctrl+Shift+B` to build solution

> Dlib will build as part of the librealsense build!

7. Right-click on one of the examples to `Set as StartUp Project`
8. Press `F5` to compile and run the example


### Linux

1. Download the latest Dlib release from [dlib.net](http://dlib.net/) and extract into a local directory (`~/work/dlib-19.17`)
2. Follow [the instructions](https://github.com/realsenseai/librealsense/blob/master/doc/installation.md) to build `librealsense` from source, but:
 * Add `-DBUILD_DLIB_EXAMPLES=true -DDLIB_DIR=~/work/dlib-19.17` to your `cmake` command
