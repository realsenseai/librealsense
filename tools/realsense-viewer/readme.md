# Intel® RealSense™ Viewer

<p align="center"><img src="https://raw.githubusercontent.com/wiki/realsenseai/librealsense/res/realsense-viewer-backup.gif" /></p>

## Overview

RealSense Viewer is the flagship tool providing access to most camera functionality through simple, cross-platform UI.
The tool offers:
* Streaming from multiple RealSense devices at the same time
* Exploring pointcloud data in realtime or by exporting to file
* Recording RealSense data as well as playback of recorded files (Please refer to [Record and Playback](https://github.com/realsenseai/librealsense/tree/development/src/media) for further information)
* Access to most camera specific controls, including 3D-generation ASIC registers when available

## Implementation Notes

You can get RealSense Viewer in form of a binary package on Windows and Linux, or build it from source alongside the rest of the library. The viewer is designed to be lightweight, requiring only a handful of embeded dependencies. Cross-platform UI is a combination of raw OpenGL calls, [GLFW](http://www.glfw.org/) for cross-platform window and event management, and [IMGUI](https://github.com/ocornut/imgui) for the interface elements. Please see [COPYING](../../COPYING) for full list of attributions.

## Presets

Please refer to [D400 Series Visual Presets](https://github.com/realsenseai/librealsense/wiki/D400-Series-Visual-Presets) for information about the Presets

## Advanced Mode
Please note that some of the Viewer features are only available in Advanced Mode.
It's easy to enable Advanced Mode, and once done it is saved in flash, so there's no need to perfrom this again, even after the camera reset:
<p align="center"><img src="https://raw.githubusercontent.com/wiki/realsenseai/librealsense/res/viewer-advanced-mode.png" width="50%" /></p>

For further information please refer to [D400 Advanced Mode](https://github.com/realsenseai/librealsense/blob/master/doc/rs400_advanced_mode.md)

## Command Line Parameters

|Flag   |Description   |
|---|---|
|`--debug`|Turn on LibRS debug logs to console, regardless of other settings|
|`--sw-only`|Show only software devices: playback, DDS, etc. -- but not USB/HID/etc.|
