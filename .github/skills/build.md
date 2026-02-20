# Building librealsense

## Prerequisites

### Minimum Versions
- **CMake**: 3.10 (3.16.3 if using `-DBUILD_WITH_DDS=ON`)
- **C++ Standard**: The core library requires **C++14**; the public API/interface requires only **C++11**
  - Defined in `CMake/lrs_macros.cmake` via `target_compile_features(... PRIVATE cxx_std_14)` and `INTERFACE cxx_std_11`
  - The internal `rsutils` library (under `third-party/rsutils/`) also requires C++14

### Platform-Specific Requirements

| Platform | Compiler / Toolchain | Notes |
|---|---|---|
| **Windows 10/11** | Visual Studio 2019 or 2022 (MSVC) | CI uses `windows-2025` runners |
| **Ubuntu 22.04** | GCC | Primary Linux target |
| **Ubuntu 24.04** | GCC | Also tested in CI |
| **macOS 15+** | Clang (Xcode) | Tested in CI on `macos-15` |
| **NVIDIA Jetson** | GCC (ARM64, L4T) | See `doc/installation_jetson.md` |
| **Raspberry Pi** | GCC (ARM) | See `doc/installation_raspbian.md` |
| **Android** | NDK (r20b+) | Cross-compilation; see `doc/android.md` |

## Basic Build (All Platforms)

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## Windows Build (Visual Studio)

```powershell
mkdir build; cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
# Or open build/realsense2.sln in Visual Studio
```

To install:
```powershell
cmake --build . --config Release --target install
```

## Linux Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

## macOS Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DFORCE_RSUSB_BACKEND=ON
make -j$(sysctl -n hw.ncpu)
sudo make install
```

Note: macOS requires `FORCE_RSUSB_BACKEND=ON`.


## Common CMake Build Flags

All flags are defined in `CMake/lrs_options.cmake`.

| Flag | Default | Description |
|---|---|---|
| `BUILD_SHARED_LIBS` | ON | Build as shared library (`OFF` for static) |
| `BUILD_EXAMPLES` | ON | Build example applications |
| `BUILD_GRAPHICAL_EXAMPLES` | ON | Build viewer & graphical tools (requires GLFW) |
| `BUILD_UNIT_TESTS` | OFF | Build unit tests (requires Python 3, downloads test data) |
| `BUILD_PYTHON_BINDINGS` | OFF | Build Python bindings (pybind11) |
| `BUILD_CSHARP_BINDINGS` | OFF | Build C# (.NET) bindings |
| `BUILD_WITH_DDS` | OFF | Enable DDS (FastDDS) support; requires CMake 3.16.3 |
| `FORCE_RSUSB_BACKEND` | OFF | Use RS USB backend (required for macOS/Android, optional for Linux) |
| `BUILD_TOOLS` | ON | Build tools (fw-updater, enumerate-devices, etc.) |
| `BUILD_WITH_CUDA` | OFF | Enable CUDA support |
| `BUILD_EASYLOGGINGPP` | ON | Build EasyLogging++ for logging |
| `BUILD_WITH_STATIC_CRT` | ON | Link against static CRT (Windows/MSVC) |
| `CHECK_FOR_UPDATES` | ON (OFF on macOS) | Enable checking for SDK updates |
| `ENABLE_CCACHE` | ON | Use ccache if available |
| `IMPORT_DEPTH_CAM_FW` | ON | Download latest depth camera firmware |
| `BUILD_GLSL_EXTENSIONS` | ON | Build GLSL extensions API |
| `BUILD_RS2_ALL` | ON | Build `realsense2-all` static bundle (when `BUILD_SHARED_LIBS=OFF`) |
| `BUILD_ASAN` | OFF | Enable AddressSanitizer |
| `ENABLE_SECURITY_FLAGS` | OFF | Enable additional compiler security flags |

## Example: Build with Python Bindings and Tests

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_PYTHON_BINDINGS=ON \
         -DBUILD_UNIT_TESTS=ON
cmake --build . --config Release
```

## Example: Build with DDS Support

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_DDS=ON
```

Requires CMake >= 3.16.3. FastDDS and its dependencies will be automatically downloaded and built.

## Key Build Outputs

- **Library**: `librealsense2.so` / `realsense2.dll` / `librealsense2.dylib`
- **Viewer**: `realsense-viewer` (when `BUILD_GRAPHICAL_EXAMPLES=ON`)
- **Python module**: `pyrealsense2.*.pyd` / `pyrealsense2.*.so` (when `BUILD_PYTHON_BINDINGS=ON`)
- **Test executables**: Various `test-*` binaries under the build directory (when `BUILD_UNIT_TESTS=ON`)
