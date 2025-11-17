<p align="center"><img src="doc/img/realsense.png" width="70%" /><br><br></p>

# Intel® RealSense™ SDK 2.0

<p align="center">
  <a href="https://github.com/IntelRealSense/librealsense/releases"><img src="https://img.shields.io/github/release/IntelRealSense/librealsense.svg" alt="Latest Release"></a>
  <a href="https://travis-ci.org/IntelRealSense/librealsense"><img src="https://travis-ci.org/IntelRealSense/librealsense.svg?branch=development" alt="Build Status"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache%202.0-blue.svg" alt="License"></a>
</p>

---

## Table of Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [Supported Platforms](#supported-platforms)
- [Getting Started](#getting-started)
  - [Download](#download)
  - [Installation](#installation)
  - [Quick Start Example](#quick-start-example)
- [What's Included](#whats-included)
- [Documentation & Resources](#documentation--resources)
- [Contributing](#contributing)
- [Support](#support)
- [License](#license)

---

## Overview

**Intel® RealSense™ SDK 2.0** is a cross-platform library for Intel® RealSense™ depth cameras (D400 series and SR300) and the [T265 tracking camera](./doc/t265.md).

The SDK enables:
- **Depth and color streaming** with high performance
- **Intrinsic and extrinsic calibration** information
- **Synthetic streams** (point cloud, aligned depth-to-color and vice-versa)
- **Record and playback** of streaming sessions
- **Post-processing filters** for depth data enhancement

> **Note:** For legacy Intel® RealSense™ devices (F200, R200, LR200, ZR300), please refer to the [latest legacy release](https://github.com/IntelRealSense/librealsense/tree/v1.12.1).

**Hardware & Information:**
- Purchase developer kits at [store.intelrealsense.com](https://store.intelrealsense.com/products.html)
- Learn more about the technology at [www.intelrealsense.com](https://www.intelrealsense.com/)
- No camera? Try our [sample data](./doc/sample-data.md)

---

## Key Features

- **Cross-platform support** - Linux, Windows, macOS, Android
- **Multiple language bindings** - C++, C, Python, C#/.NET, Node.js
- **Rich ecosystem** - Integration with ROS, ROS2, OpenCV, PCL, Unity, Unreal Engine, and more
- **Advanced processing** - Depth filtering, hole filling, spatial and temporal smoothing
- **Multi-camera support** - Synchronize and stream from multiple devices
- **Hardware-accelerated** - Optimized for performance
- **Open source** - Active community and transparent development

---

## Supported Platforms

| Platform | Architecture | Status |
|----------|-------------|--------|
| **Ubuntu 16/18/20** | x64 | ✅ Supported |
| **Windows 10/11** | x64, x86 | ✅ Supported |
| **macOS** | x64 | ✅ Supported |
| **Android** | ARM | ✅ Supported |

For detailed platform requirements and kernel compatibility, see the installation guides below.

---

## Getting Started

### Download

**Latest Release:** [Download here](https://github.com/IntelRealSense/librealsense/releases)

The release includes:
- Intel RealSense SDK
- RealSense Viewer application
- Depth Quality Tool
- Code samples and examples

📋 Check the [release notes](https://github.com/IntelRealSense/librealsense/wiki/Release-Notes) for:
- Supported platforms and hardware
- New features and capabilities
- Known issues and workarounds
- Firmware upgrade instructions

### Installation

Choose your platform for detailed installation instructions:

| Platform | Installation Guide |
|----------|-------------------|
| **Linux** | [Linux Installation](./doc/distribution_linux.md) |
| **Windows** | [Windows Installation](./doc/distribution_windows.md) |
| **macOS** | [macOS Installation](./doc/installation_osx.md) |
| **Android** | [Android Installation](./doc/android.md) |

**Build from source** instructions are included in each platform guide.

### Quick Start Example

Here's a simple C++ example to start streaming and get depth data:

```cpp
#include <librealsense2/rs.hpp>
#include <iostream>

int main()
{
    // Create a Pipeline - this serves as a top-level API for streaming and processing frames
    rs2::pipeline p;

    // Configure and start the pipeline
    p.start();

    while (true)
    {
        // Block program until frames arrive
        rs2::frameset frames = p.wait_for_frames();

        // Try to get a frame of a depth image
        rs2::depth_frame depth = frames.get_depth_frame();

        // Get the depth frame's dimensions
        float width = depth.get_width();
        float height = depth.get_height();

        // Query the distance from the camera to the object in the center of the image
        float dist_to_center = depth.get_distance(width / 2, height / 2);

        // Print the distance
        std::cout << "The camera is facing an object " << dist_to_center << " meters away \r";
    }

    return 0;
}
```

📚 **Learn more:** Explore our [examples](./examples) and [documentation](./doc) for detailed guides and advanced usage.

---

## What's Included

### Applications

| Application | Description | Download |
|-------------|-------------|----------|
| **[Intel® RealSense™ Viewer](./tools/realsense-viewer)** | Interactive application to view streams, visualize point clouds, configure camera settings, record/playback, and apply post-processing filters | [Intel.RealSense.Viewer.exe](https://github.com/IntelRealSense/librealsense/releases) |
| **[Depth Quality Tool](./tools/depth-quality)** | Measure and analyze depth quality metrics including plane fit accuracy, subpixel precision, distance accuracy, and fill rate | [Depth.Quality.Tool.exe](https://github.com/IntelRealSense/librealsense/releases) |
| **[Debug Tools](./tools/)** | Command-line utilities for device enumeration, firmware logging, and diagnostics | Included in [Intel.RealSense.SDK.exe](https://github.com/IntelRealSense/librealsense/releases) |

### Code Samples

Comprehensive examples demonstrating SDK capabilities:

- **[C++ Examples](./examples)** - Capture, point cloud, alignment, multi-camera, and more
- **[C Examples](./examples/C)** - Basic C API usage
- **[Python Examples](./wrappers/python/examples)** - Python integration samples

### Language Wrappers & Integrations

The SDK provides bindings for multiple languages and integrates with popular frameworks:

**Language Bindings:**
- [Python](./wrappers/python) - Full Python API
- [C#/.NET](./wrappers/csharp) - .NET integration
- [Node.js](./wrappers/nodejs) - JavaScript/Node.js wrapper
- [MATLAB](./wrappers/matlab) - MATLAB integration
- [LabVIEW](./wrappers/labview) - LabVIEW integration

**Framework Integrations:**
- [ROS](https://github.com/intel-ros/realsense/releases) - Robot Operating System
- [ROS2](https://github.com/intel/ros2_intel_realsense) - ROS 2.0
- [OpenCV](./wrappers/opencv) - Computer vision integration
- [PCL](./wrappers/pcl) - Point Cloud Library
- [Unity](./wrappers/unity) - Unity game engine
- [Unreal Engine 4](./wrappers/unrealengine4) - UE4 integration
- [OpenNI2](./wrappers/openni2) - OpenNI framework

See the [wrappers directory](./wrappers) for complete list and documentation.

---

## Documentation & Resources

- 📖 **[API Documentation](./doc)** - Complete API reference and guides
- 💡 **[Examples](./examples)** - Sample code and tutorials
- 📝 **[Release Notes](https://github.com/IntelRealSense/librealsense/wiki/Release-Notes)** - Version history and changelogs
- 🎥 **[Sample Data](./doc/sample-data.md)** - Pre-recorded datasets for testing
- 🔧 **[Troubleshooting](https://github.com/IntelRealSense/librealsense/wiki/Troubleshooting-Q%26A)** - Common issues and solutions

---

## Contributing

We welcome contributions from the community! To contribute to Intel RealSense SDK:

1. Read our [Contribution Guidelines](CONTRIBUTING.md)
2. Fork the repository
3. Create a feature branch
4. Submit a pull request

Please ensure your code follows the project's coding standards and includes appropriate tests.

---

## Support

**Need help?** Follow these steps:

1. **Check the FAQ** - [Troubleshooting & FAQ](https://github.com/IntelRealSense/librealsense/wiki/Troubleshooting-Q%26A)
2. **Search existing issues** - [Closed GitHub Issues](https://github.com/IntelRealSense/librealsense/issues?q=is%3Aclosed)
3. **Community forums** - [Intel RealSense Community](https://communities.intel.com/community/tech/realsense)
4. **Official support** - [Intel Support](https://www.intel.com/content/www/us/en/support/emerging-technologies/intel-realsense-technology.html)
5. **Still stuck?** - [Open a new issue](https://github.com/IntelRealSense/librealsense/issues/new)

When reporting issues, please include:
- SDK version and platform details
- Camera model and firmware version
- Steps to reproduce the issue
- Relevant logs or error messages

---

## License

This project is licensed under the [Apache License, Version 2.0](LICENSE).

Copyright 2018 Intel Corporation

---

<p align="center">
  <b>⭐ If you find this project useful, please consider giving it a star! ⭐</b>
</p>
