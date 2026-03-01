# Copilot Instructions for librealsense

## Project Overview

**librealsense** is the Intel® RealSense™ cross-platform open-source SDK for working with Intel RealSense depth cameras (D400, D500 series and others). It provides C, C++, Python, C#, and other language bindings.

## Architecture

- **Core library** (`src/`): The `realsense2` shared/static library written in C++14 (public API requires only C++11)
- **Public API headers** (`include/librealsense2/`): C and C++ headers; version is defined in `rs.h`
- **Common UI code** (`common/`): Shared code for the viewer and graphical tools
- **Examples** (`examples/`): Sample applications demonstrating SDK usage
- **Tools** (`tools/`): Utilities like `realsense-viewer`, `fw-updater`, `enumerate-devices`, etc.
- **Wrappers** (`wrappers/`): Language bindings — Python (pybind11), C#, Unity, OpenCV, PCL, etc.
- **Unit tests** (`unit-tests/`): Proprietary Python-based test framework orchestrated by `run-unit-tests.py`
- **Third-party** (`third-party/`): Vendored dependencies (`rsutils`, `realsense-file`, `json`, `glfw`, etc.)
- **CMake modules** (`CMake/`): Build configuration, platform detection, and external dependency management

### Layer Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│  (User Applications, Tools, Examples, Language Wrappers)    │
├─────────────────────────────────────────────────────────────┤
│                    Public C/C++ API                         │
│         (include/librealsense2/, rs.h, rs.hpp)              │
├─────────────────────────────────────────────────────────────┤
│                     Core Library                            │
│    (Context, Device, Sensor, Stream, Frame Management)      │
├─────────────────────────────────────────────────────────────┤
│                 Processing Pipeline                         │
│  (Format Conversion, Post-Processing, Synchronization)      │
├─────────────────────────────────────────────────────────────┤
│                 Platform Abstraction                        │
│       (UVC, HID, USB abstractions, Backend Interface)       │
├─────────────────────────────────────────────────────────────┤
│              Platform-Specific Backends                     │
│    (Windows: WMF/WinUSB, Linux: V4L2/libusb, macOS)        │
└─────────────────────────────────────────────────────────────┘
```

## Code Conventions

- **Copyright header**: every new source file must include `# License: Apache 2.0. See LICENSE file in root directory.` followed by `# Copyright(c) <current year> RealSense, Inc. All Rights Reserved.` — always use the actual current year, not the year of a nearby file being used as a template
- The core library compiles as **C++14** (`cxx_std_14` — see `CMake/lrs_macros.cmake`)
- The public interface only requires **C++11** (`cxx_std_11`)
- Examples and wrappers generally use **C++11**
- CMake minimum version: **3.10** (3.16.3 when `BUILD_WITH_DDS` is enabled)
- Use the existing code style in surrounding files; the project does not enforce a formatter
- Logging uses EasyLogging++ (controlled by `BUILD_EASYLOGGINGPP` option)

## Build System

The project uses **CMake**. Key build options are defined in `CMake/lrs_options.cmake`. Platform-specific configuration lives in:
- `CMake/windows_config.cmake` — Windows (MSVC)
- `CMake/unix_config.cmake` — Linux / macOS
- `CMake/android_config.cmake` — Android NDK

## Supported Platforms

| Platform | Notes |
|---|---|
| **Windows 10/11** | MSVC (Visual Studio 2019/2022) |
| **Ubuntu 20.04 / 22.04 / 24.04** | GCC, primary Linux target |
| **macOS** | Clang, macOS 15+ tested in CI |
| **NVIDIA Jetson** | ARM64, L4T |
| **Raspberry Pi** | ARM (Raspbian) |
| **Android** | NDK cross-compilation |

## Key CMake Build Flags

| Flag | Default | Description |
|---|---|---|
| `BUILD_SHARED_LIBS` | ON | Build as shared library |
| `BUILD_EXAMPLES` | ON | Build example applications |
| `BUILD_GRAPHICAL_EXAMPLES` | ON | Build viewer & graphical tools |
| `BUILD_UNIT_TESTS` | OFF | Build unit tests |
| `BUILD_PYTHON_BINDINGS` | OFF | Build Python bindings |
| `BUILD_WITH_DDS` | OFF | Enable DDS (FastDDS) support |
| `FORCE_RSUSB_BACKEND` | OFF | Use RS USB backend (required for Win7/macOS/Android) |
| `BUILD_TOOLS` | ON | Build tools (fw-updater, etc.) |

## Testing

Tests use a custom Python-based test framework orchestrated by `unit-tests/run-unit-tests.py`. To run tests, build with `-DBUILD_UNIT_TESTS=ON -DBUILD_PYTHON_BINDINGS=ON`, then from the `unit-tests/` directory run `py -3 run-unit-tests.py -s`. Use `--help` to see all available options.

## Naming Conventions

- **Namespaces**: `librealsense` (main), `librealsense::platform` (platform layer)
- **Files**: kebab-case (e.g., `backend-v4l2.h`, `device-model.cpp`)
- **Classes**: snake_case (e.g., `uvc_device`, `frame_interface`, `device_info`)
- **Functions/Methods**: snake_case (e.g., `get_device_count()`, `start_streaming()`)
- **Constants**: UPPER_CASE (e.g., `RS2_CAMERA_INFO_NAME`, `DEFAULT_TIMEOUT`)
- **Public C API enums**: `rs2_*` prefix (e.g., `rs2_format`, `rs2_stream`)
- **Interface classes**: `*_interface` suffix (e.g., `device_interface`, `sensor_interface`)
- **Factory classes**: `*_factory` suffix (e.g., `device_factory`, `backend_factory`)
- **Callback types**: `*_callback` suffix (e.g., `frame_callback`, `devices_changed_callback`)
- **UI model classes**: `*_model` suffix (e.g., `device_model`, `stream_model`)

## Key Classes & Interfaces

Core class hierarchy:

```
librealsense::context           // Device discovery & management
└── librealsense::device        // Hardware device representation
    └── librealsense::sensor    // Individual camera sensor
        ├── stream_profile      // Stream configuration
        └── frame               // Data frame
```

Key base interfaces:
- `device_interface` — base for all devices
- `sensor_interface` — base for all sensors
- `frame_interface` — base for frame data
- `option_interface` — configuration option abstraction
- `backend_interface` — platform backend abstraction

## Threading Model

- Context, device, and sensor objects are **thread-safe** for concurrent access
- Frame **callbacks execute on internal library threads** — keep them fast and avoid blocking
- Each active sensor runs its own **streaming thread**
- All public APIs use internal mutexes for state protection

## Memory Management

- All resources use **RAII** with `shared_ptr` / `unique_ptr`
- Frame objects are **pooled and reused** to minimize allocations
- Frames use **reference counting** for safe multi-consumer access
- Platform backends manage kernel/hardware buffer lifecycles

## API Contracts & Error Handling

- **C API**: Functions report failures via `rs2_error*` out-parameters
- **C++ API**: Wrapper throws `rs2::error` exceptions on failure
- Public C API maintains **ABI compatibility** within major versions
- Not all stream formats are supported on all platforms/devices
- Some features require minimum firmware versions on the device

## Working with This Repo

- When modifying core library code under `src/`, ensure it compiles as C++14
- When modifying public headers under `include/`, maintain C++11 compatibility
- Changes to CMake should work with CMake 3.10+
- Platform-specific code should be guarded appropriately (check `CMake/include_os.cmake` for the pattern)
- `rsutils` (under `third-party/rsutils/`) is a foundational utility library linked publicly into `realsense2`
