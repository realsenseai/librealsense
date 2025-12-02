<p align="center">
<!-- Light mode -->
<img src="doc/img/realsense-logo-light-mode.png#gh-light-mode-only" alt="Logo for light mode" width="30%"/>

<!-- Dark mode -->
<img src="doc/img/realsense-logo-dark-mode.png#gh-dark-mode-only" alt="Logo for dark mode" width="30%"/>
<br><br>
</p>

<p align="center">RealSense SDK 2.0 is a cross-platform library for RealSense depth cameras.
The SDK allows depth and color streaming, and provides intrinsic and extrinsic calibration information.</p>


<p align="center">
  <a href="https://www.apache.org/licenses/LICENSE-2.0"><img src="https://img.shields.io/github/license/realsenseai/librealsense.svg" alt="License"></a>
  <a href="https://github.com/realsenseai/librealsense/releases/latest"><img src="https://img.shields.io/github/v/release/realsenseai/librealsense?sort=semver" alt="Latest release"></a>
  <a href="https://github.com/realsenseai/librealsense/compare/master...development"><img src="https://img.shields.io/github/commits-since/realsenseai/librealsense/master/development?label=commits%20since" alt="Commits since"></a>
  <a href="https://github.com/realsenseai/librealsense/issues"><img src="https://img.shields.io/github/issues/realsenseai/librealsense.svg" alt="Issues"></a>
  <a href="https://github.com/realsenseai/librealsense/actions/workflows/buildsCI.yaml?query=branch%3Adevelopment"><img src="https://github.com/realsenseai/librealsense/actions/workflows/buildsCI.yaml/badge.svg?branch=development" alt="GitHub CI"></a>
  <a href="https://github.com/realsenseai/librealsense/network/members"><img src="https://img.shields.io/github/forks/realsenseai/librealsense.svg" alt="Forks"></a>
</p>

## Important Notice

We are happy to announce that the RealSense GitHub repositories have been successfully migrated to the RealSenseAI organization.
Please make sure to update your links to the new RealSenseAI organization for both cloning the repositories and accessing specific files within them.

[https://github.com/**IntelRealSense**/librealsense](https://github.com/IntelRealSense/librealsense) --> [https://github.com/**realsenseai**/librealsense](https://github.com/realsenseai/librealsense)

Note: A redirection from the previous name IntelRealSense is currently in place, but we cannot guarantee how long it will remain active. We recommend that all users update their references to point to the new GitHub location.

## Use Cases

Below are some of the many real-world applications powered by RealSense technology:

Robotics | Depth Sensing | 3D Scanning |
:------------: | :----------: | :-------------: |
<a href="https://realsenseai.com/case-studies/?capability_application=autonomous-mobile-robots"><img src="https://librealsense.realsenseai.com/readme-media/realsense_examplerealsense_example.gif" width="240"/></a> |<a href="https://realsenseai.com/case-studies/?q=/case-studies&"><img src="https://librealsense.realsenseai.com/readme-media/align-expectede.gif" width="240"/></a> | <a href="https://realsenseai.com/case-studies/?capability_application=autonomous-mobile-robots"><img src="https://librealsense.realsenseai.com/readme-media/realsense_dynamic_example.gif" width="240"/></a>

Drones | Skeletal and People Tracking | Facial Authentication |
:--------------------------: | :-----: | :----------------------: |
<a href="https://realsenseai.com/case-studies/?q=/case-studies&"><img src="https://librealsense.realsenseai.com/readme-media/drone-demo.gif" width="240"/></a> |<a href="https://realsenseai.com/case-studies/?capability_application=monitoring-and-tracking"><img src="https://librealsense.realsenseai.com/readme-media/SkeletalTracking.gif" width="240"/></a> | <a href="https://realsenseai.com/case-studies/?capability_application=biometrics"><img src="https://librealsense.realsenseai.com/readme-media/face-demo.gif" width="240"/></a>



## Why RealSense?

- **High-resolution color and depth** at close and long ranges
- **Open source SDK** with rich examples and wrappers (Python, ROS, C#, Unity and [more...](https://github.com/realsenseai/librealsense/tree/master/wrappers))
- **Active developer community and defacto-standard 3D stereo camera for robotics**
- **Cross-platform** support: Windows, Linux, macOS, Android, and Docker

## Product Line

RealSense stereo depth products use stereo vision to calculate depth, providing high-quality performance in various lighting and environmental conditions.

Here are some examples of the supported models:

| Product | Image | Description |
|---------|-------|-------------|
| [**D555 PoE**](https://realsenseai.com/ruggedized-industrial-stereo-depth/d555-poe/) | <img src="https://realsenseai.com/wp-content/uploads/2025/07/D555.png" width="1000"> | The RealSense™ Depth Camera D555 introduces Power over Ethernet (PoE) interface on chip, expanding our portfolio of USB and GMSL/FAKRA products. |
| [**D457 GMSL/FAKRA**](https://realsenseai.com/ruggedized-industrial-stereo-depth/d457-gmsl-fakra/) | <img src="https://realsenseai.com/wp-content/uploads/2025/07/D457.png" width="1000"> | The RealSense™ Depth Camera D457 is our first GMSL/FAKRA high bandwidth stereo camera. The D457 has an IP65 grade enclosure protecting it from dust ingress and projected water. |
| [**D455**](https://realsenseai.com/stereo-depth-cameras/real-sense-depth-camera-d455/) | <img src="https://www.realsenseai.com/wp-content/uploads/2021/11/455.png" width="1000"> | The RealSense D455 is a long-range stereo depth camera with a 95 mm baseline, global-shutter depth sensors, an RGB sensor, and a built-in IMU, delivering accurate depth at distances up to 10 m.. |
| [**D435if**](https://realsenseai.com/stereo-depth-cameras/depth-camera-d435i/) | <img src="https://realsenseai.com/wp-content/uploads/2025/06/D435if-a.png" width="1000"> | The D435if is one of [RealSense™ Depth Camera with IR pass filter family](https://realsenseai.com/stereo-depth-with-ir-pass-filter/) expanding our portfolio targeting the growing robotic market. The D400f family utilizes an IR pass filter to enhance depth quality and performance range in many robotic environments.|
| [**D405**](https://realsenseai.com/stereo-depth-cameras/stereo-depth-camera-d405/) | <img src="https://realsenseai.com/wp-content/uploads/2025/07/D-405.png" width="1000"> | The RealSense™ Depth Camera D405 is a short-range stereo camera providing sub-millimeter accuracy for your close-range computer vision needs. |


> 🛍️ [Explore more stereo products](https://store.realsenseai.com/)

## Getting Started

Start developing with RealSense in minutes using either method below.

### 1️. Precompiled SDK

This is the best option if you want to plug in your camera and get started right away.
1. Download the latest SDK bundle from the [Releases page](https://github.com/realsenseai/librealsense/releases).
2. Connect your RealSense camera.
3. Run the included tools:
    - [RealSense Viewer](./tools/realsense-viewer/): View streams, tune settings, record and playback.
    - [Depth Quality Tool](./tools/depth-quality/): Measure accuracy and fill rate.

### Setup Guides - precompiled SDK

<a href="./doc/distribution_linux.md"><img src="https://img.shields.io/badge/Ubuntu_Guide-333?style=flat&logo=ubuntu&logoColor=white" style="margin: 5px;" alt="Linux\Jetson Guide"/></a>
<a href="./doc/distribution_windows.md"><img src="https://custom-icon-badges.demolab.com/badge/Windows_Guide-333?logo=windows11&logoColor=white" style="margin: 5px;" alt="Windows Guide"/></a>

> **Note:** For **minor releases**, we publish Debian packages as release artifacts that you can download and install directly.

### 2️. Build from Source
For a more custom installation, follow these steps to build the SDK from source.
1. Clone the repository and create a build directory:
   ```bash
   git clone https://github.com/realsenseai/librealsense.git
   cd librealsense
   mkdir build && cd build
   ```
2. Run CMake to configure the build:
    ```bash
    cmake ..
    ```
3. Build the project:
    ```bash
    cmake --build .
    ```

### Setup Guides - build from source

<a href="./doc/installation.md"><img src="https://img.shields.io/badge/Ubuntu_Guide-333?style=flat&logo=ubuntu&logoColor=white" style="margin: 5px;" alt="Linux Guide"/></a>
<a href="./doc/installation_jetson.md"><img src="https://img.shields.io/badge/Jetson_Guide-333?style=flat&logo=nvidia&logoColor=white" style="margin: 5px;" alt="Jetson Guide"/></a>
<a href="./doc/installation_windows.md"><img src="https://custom-icon-badges.demolab.com/badge/Windows_Guide-333?logo=windows11&logoColor=white" style="margin: 5px;" alt="Windows Guide"/></a>
<a href="./doc/installation_osx.md"><img src="https://img.shields.io/badge/macOS_Guide-333?style=flat&logo=apple&logoColor=white" style="margin: 5px;" alt="macOS Guide"/></a>


## Python Packages
[![pyrealsense2](https://img.shields.io/pypi/v/pyrealsense2.svg?label=pyrealsense2&logo=pypi)](https://pypi.org/project/pyrealsense2/)
[![PyPI - pyrealsense2-beta](https://img.shields.io/pypi/v/pyrealsense2-beta.svg?label=pyrealsense2-beta&logo=pypi)](https://pypi.org/project/pyrealsense2-beta/)

**Which should I use?**
- **Stable:** `pyrealsense2` — validated releases aligned with SDK tags (Recommended for most users).  
- **Beta:** `pyrealsense2-beta` — fresher builds for early access and testing. Expect faster updates.  

### Install
```bash
pip install pyrealsense2 # Stable
pip install pyrealsense2-beta # Beta
```
> Both packages import as `pyrealsense2`. Install **only one** at a time.

## Ready to Hack!

Our library offers a high level API for using RealSense depth cameras (in addition to lower level ones).
The following snippets show how to start streaming frames and extracting the depth value of a pixel:

**C++**
```cpp
#include <librealsense2/rs.hpp>
#include <iostream>

int main() {
    rs2::pipeline p;                 // Top-level API for streaming & processing frames
    p.start();                       // Configure and start the pipeline

    while (true) {
        rs2::frameset frames = p.wait_for_frames();        // Block until frames arrive
        rs2::depth_frame depth = frames.get_depth_frame(); // Get depth frame
        if (!depth) continue;

        int w = depth.get_width(), h = depth.get_height();
        float dist = depth.get_distance(w/2, h/2);         // Distance to center pixel
        std::cout << "The camera is facing an object " << dist << " meters away\r";
    }
}
```

**Python**
```python
import pyrealsense2 as rs

pipeline = rs.pipeline() # Create a pipeline
pipeline.start() # Start streaming

try:
    while True:
        frames = pipeline.wait_for_frames()
        depth_frame = frames.get_depth_frame()
        if not depth_frame:
            continue

        width, height = depth_frame.get_width(), depth_frame.get_height()
        dist = depth_frame.get_distance(width // 2, height // 2)
        print(f"The camera is facing an object {dist:.3f} meters away", end="\r")

finally:
    pipeline.stop() # Stop streaming
```

For more information on the library, please follow our [examples](./examples) or [tools](./tools/), and read the [documentation](./doc) to learn more.

## Supported Platforms

### Operating Systems and Platforms

| Ubuntu | Windows | macOS High Sierra | Jetson | Raspberry Pi |
|--------|---------|-------|--------|----------------|
| <div align="center"><a href="https://dev.realsenseai.com/docs/compiling-librealsense-for-linux-ubuntu-guide"><img src="https://librealsense.realsenseai.com/readme-media/ubuntu.png" width="40%" alt="Ubuntu" /></a></div> | <div align="center"><a href="https://dev.realsenseai.com/docs/compiling-librealsense-for-windows-guide"><img src="https://librealsense.realsenseai.com/readme-media/Windows_logo.png" width="40%" alt="Windows" /></a></div> | <div align="center"><a href="https://dev.realsenseai.com/docs/macos-installation-for-intel-realsense-sdk"><picture><source media="(prefers-color-scheme: dark)" srcset="https://librealsense.realsenseai.com/readme-media/apple-dark.png"><img src="https://librealsense.realsenseai.com/readme-media/apple-light.png" width="40%" alt="macOS" /></picture></a></div> | <div align="center"><a href="https://dev.realsenseai.com/docs/nvidia-jetson-tx2-installation"><img src="https://librealsense.realsenseai.com/readme-media/nvidia.png" width="40%" alt="" /></a></div> | <div align="center"><a href="https://dev.realsenseai.com/docs/using-depth-camera-with-raspberry-pi-3"><img src="https://librealsense.realsenseai.com/readme-media/raspberry-pi.png" width="40%" alt="" /></a></div> 



### Programming Languages and Wrappers

| C++ | C | C# | Python | ROS 2 |Rest API |
|-----|---|----|--------|-------|---------|
| <div align="center"><a href="https://dev.realsenseai.com/docs/code-samples"><img src="https://librealsense.realsenseai.com/readme-media/cpp.png" width="50%" alt="" /></a></div> | <div align="center"><a href="https://dev.realsenseai.com/docs/code-samples"><img src="https://librealsense.realsenseai.com/readme-media/c.png" width="55%" alt="" /></a></div> | <div align="center"><a href="https://dev.realsenseai.com/docs/csharp-wrapper"><img src="https://librealsense.realsenseai.com/readme-media/c-sharp.png" width="50%" alt="" /></a></div> | <div align="center"><a href="https://dev.realsenseai.com/docs/python2"><img src="https://librealsense.realsenseai.com/readme-media/python.png" width="30%" alt="" /></a></div> | <div align="center"><a href="https://dev.realsenseai.com/docs/ros2-wrapper"><picture><source media="(prefers-color-scheme: dark)" srcset="https://librealsense.realsenseai.com/readme-media/ros2-dark.png"><img src="https://librealsense.realsenseai.com/readme-media/ROS2-light.png" width="80%" alt="ROS 2" /></picture></a></div> | <div align="center"><a href="https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/README.md"><img src="https://librealsense.realsenseai.com/readme-media/REST_API.png" width="50%" alt="Rest API" /></a></div>|

For more platforms and wrappers look over [here](https://dev.realsenseai.com/docs/docs-get-started).
> Full feature support varies by platform – refer to the [release notes](https://github.com/realsenseai/librealsense/wiki/Release-Notes) for details.

## Community & Support

- [📚 Wiki & Docs](https://github.com/realsenseai/librealsense/wiki)
- [🐞 Report Issues](https://github.com/realsenseai/librealsense/issues)- Found a bug or want to contribute? Read our [contribution guidelines](./CONTRIBUTING.md).

> 🔎 Looking for legacy devices (F200, R200, LR200, ZR300)? Visit the [legacy release](https://github.com/realsenseai/librealsense/tree/v1.12.1).

---
<p align="center">
You can find us at
</p>
<p align="center">
  <a href="https://github.com/realsenseai" target="_blank" aria-label="GitHub"><picture><source media="(prefers-color-scheme: dark)" srcset="https://librealsense.realsenseai.com/readme-media/github_light.PNG"><img src="https://librealsense.realsenseai.com/readme-media/github.png" width="32" alt="GitHub"></picture></a>
  &nbsp;&nbsp;&nbsp;
  <a href="https://x.com/RealSenseai" target="_blank" aria-label="X (Twitter)"><img src="https://librealsense.realsenseai.com/readme-media/twitter.png" width="32" alt="X (Twitter)" /></a>
  &nbsp;&nbsp;&nbsp;
  <a href="https://www.youtube.com/@RealSenseai" target="_blank" aria-label="YouTube"><img src="https://librealsense.realsenseai.com/readme-media/social.png" width="32" alt="YouTube" /></a>
  &nbsp;&nbsp;&nbsp;
  <a href="https://www.linkedin.com/company/realsenseai?trk=similar-pages" target="_blank" aria-label="LinkedIn"><img src="https://librealsense.realsenseai.com/readme-media/linkedin.png" width="32" alt="LinkedIn" /></a>
  &nbsp;&nbsp;&nbsp;
  <a href="https://realsenseai.com/" target="_blank" aria-label="Community"><img src="https://librealsense.realsenseai.com/readme-media/Real-sense-badge-rgb-c.png" width="32" alt="Community" /></a>
</p>



