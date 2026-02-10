# LibRealSense Project Context

## Overview

LibRealSense is Intel's open-source, cross-platform SDK for RealSense depth cameras. It provides streaming access to depth, color, and IMU data along with intrinsic and extrinsic calibration information. The SDK supports various RealSense camera models (D400, D400f, D455, D457 GMSL/FAKRA, D555 PoE series) across Windows, Linux, macOS, Android, and Docker platforms.

## High-Level Architecture

### Layer-Based Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│  (User Applications, Tools, Examples, Language Wrappers)   │
├─────────────────────────────────────────────────────────────┤
│                    Public C/C++ API                        │
│         (include/librealsense2/, rs.h, rs.hpp)            │
├─────────────────────────────────────────────────────────────┤
│                     Core Library                           │
│    (Context, Device, Sensor, Stream, Frame Management)     │
├─────────────────────────────────────────────────────────────┤
│                 Processing Pipeline                        │
│  (Format Conversion, Post-Processing, Synchronization)     │
├─────────────────────────────────────────────────────────────┤
│                 Platform Abstraction                       │
│       (UVC, HID, USB abstractions, Backend Interface)      │
├─────────────────────────────────────────────────────────────┤
│              Platform-Specific Backends                    │
│    (Windows: WMF/WinUSB, Linux: V4L2/libusb, macOS)       │
└─────────────────────────────────────────────────────────────┘
```

### Key Architectural Components

1. **Context Management**: Central coordination point for device discovery and lifecycle management
2. **Device Abstraction**: Hardware-agnostic device representation with sensor hierarchy
3. **Stream Processing**: Format conversion, post-processing filters, and synchronization
4. **Platform Backends**: OS-specific implementations for camera communication
5. **Extension System**: Modular features via interfaces and factory patterns

## Module Responsibilities

### Core Modules

- **`src/context.{h,cpp}`**: Device discovery, factory management, and context lifecycle
- **`src/device.{h,cpp}`**: Device abstraction, sensor management, and device-specific functionality
- **`src/sensor.{h,cpp}`**: Individual sensor control, stream management, and data acquisition
- **`src/stream.{h,cpp}`**: Stream profile management and metadata handling
- **`src/frame.{h,cpp}`**: Frame data structure, memory management, and frame queuing

### Platform Abstraction

- **`src/platform/`**: Platform-agnostic interfaces for UVC, HID, and USB devices
- **`src/backend.{h,cpp}`**: Backend factory and device enumeration coordination

### Platform-Specific Backends

- **`src/linux/backend-v4l2.{h,cpp}`**: Linux V4L2 implementation for UVC devices
- **`src/win/backend-*`**: Windows Media Foundation and WinUSB implementations
- **`src/libusb/`**: Cross-platform USB backend using libusb
- **`src/libuvc/`**: Cross-platform UVC backend using libuvc

### Processing & Algorithms

- **`src/proc/`**: Post-processing filters (decimation, spatial, temporal, hole filling)
- **`src/algo.{h,cpp}`**: Core computer vision algorithms and utilities
- **`src/sync.{h,cpp}`**: Multi-stream synchronization and frame matching

### Device-Specific Support

- **`src/ds/`**: D400 series camera family implementation
- **`src/hid/`**: HID sensor support (IMU, motion tracking)
- **`src/fw-update/`**: Firmware update mechanisms

### Utilities & Common

- **`src/types.{h,cpp}`**: Core type definitions, utility functions, and math helpers
- **`common/`**: Shared UI models, configuration, and helper utilities
- **`tools/`**: Command-line tools and utilities (realsense-viewer, depth-quality-tool)

### Language Bindings

- **`wrappers/`**: Language-specific bindings (Python, C#, Unity, ROS, etc.)

## Key Classes & Data Flows

### Core Hierarchy

```cpp
// Main entry points
librealsense::context                    // Device discovery & management
├── librealsense::device                 // Hardware device representation
    ├── librealsense::sensor             // Individual camera sensors
        ├── librealsense::stream_profile // Stream configuration
        └── librealsense::frame          // Data frames
```

### Key Interfaces & Base Classes

- **`device_interface`**: Base interface for all devices
- **`sensor_interface`**: Base interface for all sensors
- **`frame_interface`**: Base interface for frame data
- **`option_interface`**: Configuration option abstraction
- **`extension_interface`**: Feature extension system
- **`backend_interface`**: Platform backend abstraction

### Data Flow Architecture

```
Camera Hardware
      ↓
Platform Backend (V4L2/WMF/etc.)
      ↓
UVC/HID Device Abstraction
      ↓
Sensor Layer (format, controls)
      ↓
Format Converter (raw → user formats)
      ↓
Post-Processing Pipeline (filters)
      ↓
Frame Queue & Synchronization
      ↓
Public API (callbacks, polling)
      ↓
User Application
```

### Critical Classes

- **`v4l_uvc_device`**: Linux V4L2 device implementation
- **`uvc_sensor`**: UVC camera sensor abstraction
- **`syncer_process_unit`**: Multi-stream synchronization
- **`formats_converter`**: Format transformation pipeline
- **`frame_archive`**: Frame memory management and pooling
- **`device_watcher`**: Hot-plug device monitoring

## Naming Conventions

### General Conventions

- **Namespaces**: `librealsense` (main), `librealsense::platform` (platform layer)
- **Files**: kebab-case (e.g., `backend-v4l2.h`, `device-model.cpp`)
- **Classes**: snake_case (e.g., `uvc_device`, `frame_interface`, `device_info`)
- **Functions/Methods**: snake_case (e.g., `get_device_count()`, `start_streaming()`)
- **Constants**: UPPER_CASE (e.g., `RS2_CAMERA_INFO_NAME`, `DEFAULT_TIMEOUT`)
- **Enums**: rs2\_\* prefix for public API (e.g., `rs2_format`, `rs2_stream`)

### Specific Patterns

- **Interface classes**: `*_interface` suffix (e.g., `device_interface`, `sensor_interface`)
- **Implementation classes**: concrete names (e.g., `uvc_device`, `hid_sensor`)
- **Factory classes**: `*_factory` suffix (e.g., `device_factory`, `backend_factory`)
- **Model/View classes**: `*_model` suffix (e.g., `device_model`, `stream_model`)
- **Thread/Worker classes**: `*_thread`, `*_worker` suffixes
- **Callback types**: `*_callback` suffix (e.g., `frame_callback`, `devices_changed_callback`)

### File Organization

- **Headers**: `.h` extension, matching `.cpp` files
- **Platform-specific**: subdirectories (`linux/`, `win/`, `android/`)
- **Core interfaces**: `src/core/` directory
- **Public API**: `include/librealsense2/` hierarchy

## Invariants & Assumptions

### Threading Model

- **Thread-safe**: Context, device, and sensor objects are thread-safe for concurrent access
- **Callback context**: Frame callbacks execute on internal library threads
- **Synchronization**: All public APIs use internal mutexes for state protection
- **Streaming threads**: Each active sensor runs its own streaming thread

### Memory Management

- **RAII**: All resources use RAII pattern with shared_ptr/unique_ptr
- **Frame pooling**: Frame objects are pooled and reused to minimize allocations
- **Reference counting**: Frames use reference counting for safe multi-consumer access
- **Buffer management**: Platform backends manage kernel/hardware buffer lifecycles

### Device State Management

- **State consistency**: Device power states are maintained consistently across sensors
- **Exclusive access**: Only one application can stream from a device simultaneously
- **Hot-plug support**: Device connections/disconnections are handled gracefully
- **Error recovery**: Streaming errors trigger automatic recovery attempts

### Platform Assumptions

- **Modern kernels**: Linux support requires kernel 3.16+ (UVC 1.1+ support)
- **USB compatibility**: USB 2.0 minimum, USB 3.0+ recommended for full functionality
- **Metadata support**: Metadata streaming requires kernel 4.16+ on Linux
- **Permissions**: Linux requires proper udev rules for non-root access

### API Contracts

- **C API stability**: Public C API maintains ABI compatibility within major versions
- **Error handling**: C API functions report failures via `rs2_error*` out-parameters; the C++ wrapper typically throws `rs2::error` exceptions on failure
- **Stream format support**: Not all formats supported on all platforms/devices
- **Synchronization guarantees**: Synchronized streams maintain timestamp correlation

### Configuration Assumptions

- **JSON settings**: Device behavior configurable via JSON configuration files
- **Default profiles**: Each device provides recommended default stream configurations
- **Calibration data**: Intrinsic/extrinsic calibration stored on-device and accessible
- **Firmware dependencies**: Some features require minimum firmware versions

### Performance Expectations

- **Zero-copy**: Minimize data copies in streaming pipeline where possible
- **Low latency**: Frame-to-callback latency typically < 50ms for most configurations
- **Memory efficiency**: Frame pooling prevents excessive allocation/deallocation
- **CPU utilization**: Post-processing filters may significantly impact CPU usage

### Build System

- **CMake**: Primary build system with modular configuration options
- **Cross-compilation**: Supports cross-compilation for embedded targets
- **Optional dependencies**: Many features are optional with compile-time flags
- **Package management**: Supports both system packages and bundled dependencies

This context document provides agents with essential knowledge about LibRealSense architecture, conventions, and operational assumptions for effective development and troubleshooting.
