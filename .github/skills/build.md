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

## Windows Build (Visual Studio)

### Full Build

```powershell
mkdir build; cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release --parallel
# Or open build/realsense2.sln in Visual Studio
```

**Note**: Always use `--parallel` flag on Windows for faster parallel builds.

### Build Specific Target

To build only a specific target:
```powershell
cmake --build . --config Release --parallel --target <target-name>
```

Examples:
```powershell
# Build only the core library
cmake --build . --config Release --parallel --target realsense2

# Build only the viewer
cmake --build . --config Release --parallel --target realsense-viewer
```

### Install

```powershell
cmake --build . --config Release --parallel --target install
```

## Linux Build

### Full Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Build Specific Target

To build only a specific target:
```bash
make <target-name>
# Or with cmake:
cmake --build . --target <target-name>
```

Examples:
```bash
# Build only the core library
make realsense2

# Build only the viewer
make realsense-viewer
```

### List Available Targets

```bash
make help
```

### Install

```bash
sudo make install
```

## macOS Build

**Note**: macOS requires `FORCE_RSUSB_BACKEND=ON`.

### Full Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DFORCE_RSUSB_BACKEND=ON
make -j$(sysctl -n hw.ncpu)
```

### Build Specific Target

To build only a specific target:
```bash
make <target-name>
# Or with cmake:
cmake --build . --target <target-name>
```

Examples:
```bash
# Build only the core library
make realsense2

# Build only the viewer
make realsense-viewer
```

### List Available Targets

```bash
make help
```

### Install

```bash
sudo make install
```


## Common Target Reference

| Target | Description |
|---|---|
| `realsense2` | Core library only |
| `realsense-viewer` | Viewer application |
| `rs-enumerate-devices` | Device enumeration tool |
| `rs-fw-update` | Firmware update tool |
| `pyrealsense2` | Python bindings |

## Common CMake Build Flags

All flags are defined in `CMake/lrs_options.cmake`.

> **Rule:** When adding a new CMake build flag, always declare it with `option()` in `CMake/lrs_options.cmake` — never inline in a subdirectory `CMakeLists.txt`. This keeps all flags discoverable in one place and ensures they appear in `cmake-gui`/`ccmake` regardless of which subdirectory is being configured.

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
| `BUILD_GLSL_EXTENSIONS` | ON | Build GLSL extensions API |
| `BUILD_RS2_ALL` | ON | Build `realsense2-all` static bundle (when `BUILD_SHARED_LIBS=OFF`) |
| `BUILD_ASAN` | OFF | Enable AddressSanitizer |
| `ENABLE_SECURITY_FLAGS` | OFF | Enable additional compiler security flags |
| `BUILD_WITH_MINZ` | OFF | Enable MinZ depth improvement in viewer (Jetson only); requires `librealsense2-enhanced-depth` installed on every system that runs the binary — **do not distribute this binary to systems without the package** |

## Example: Build with Python Bindings and Tests

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_PYTHON_BINDINGS=ON \
         -DBUILD_UNIT_TESTS=ON
cmake --build . --config Release
```

**On Windows**, always add `--parallel`:
```powershell
cmake --build . --config Release --parallel
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
