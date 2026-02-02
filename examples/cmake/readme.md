# CMake Sample

## Overview

This sample demonstrates how to create a basic librealsense application using CMake.
Currently this sample is supported only by Linux OS.

## Prerequisite
Install the SDK or build it from source  ([Linux](https://github.com/realsenseai/librealsense/blob/master/doc/distribution_linux.md))

## Expected Output
![cmake_example](https://user-images.githubusercontent.com/18511514/48919868-06bb9400-ee9e-11e8-9c93-5bca41d5954c.PNG)

## Code Overview 

Set minimum required CMake version
```
cmake_minimum_required(VERSION 3.10)
```

Name the project, in this sample the project name will be also the executable name
```
project(hello_librealsense2)
```

Find librealsense installation, this feature is currently available only for Linux 
```
# Find librealsense2 installed package
find_package(realsense2 REQUIRED)
```

Enable C++11 standard in the application
```
# Enable C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
```

Point to the source files, in this simple example we have only one cpp file
```
# Add the application sources to the target
add_executable(${PROJECT_NAME} hello_librealsense2.cpp)
```

Link librealsense, the variable ${realsense2_LIBRARY} is set by "find_package"
```
# Link librealsense2 to the target
target_link_libraries(${PROJECT_NAME} ${realsense2_LIBRARY})
```

