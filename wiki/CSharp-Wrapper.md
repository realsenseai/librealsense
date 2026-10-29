The C# wrapper is a .NET class library, `Intel.RealSense.dll`, that exposes the librealsense C API to .NET languages through P/Invoke. Its sources are in [`wrappers/csharp`](https://github.com/realsenseai/librealsense/tree/development/wrappers/csharp) and all public types live in the `Intel.RealSense` namespace. It is meant for Windows developers writing C# (console, WPF or other .NET applications), and it is also the managed layer used by the Unity integration (see [[Unity and Unreal Engine]]). This page covers building the wrapper, a first program, the bundled tutorials and the main classes.

## Building with CMake and Visual Studio

The wrapper is built as part of the main librealsense CMake build by turning on `BUILD_CSHARP_BINDINGS` (default `OFF`, defined in [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake)). For the full list of build options see [[Build Configuration]]; for the native Windows build itself see [[Installation Windows]].

From the librealsense root folder:

```
mkdir build
cd build
cmake .. -DBUILD_CSHARP_BINDINGS=ON -DBUILD_SHARED_LIBS=ON
```

Open the generated `realsense2.sln` in Visual Studio. The wrapper library and the tutorials appear in the solution folder `Wrappers/csharp`. Each tutorial project depends on the `Intel.RealSense` project, which in turn depends on the native `realsense2` project, so building a tutorial builds everything it needs.

The wrapper's [`CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/csharp/CMakeLists.txt) declares `project(LANGUAGES CSharp)`, so CMake must be run with a Visual Studio generator.

### CMake options and variables

| Option / variable | Default | Effect |
|---|---|---|
| `BUILD_CSHARP_BINDINGS` | `OFF` | Adds `wrappers/csharp` to the build ([`wrappers/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/CMakeLists.txt)). |
| `BUILD_SHARED_LIBS` | `ON` | Must stay `ON`. With `OFF`, configuration stops with `BUILD_SHARED_LIBS flag must be enabled when generating CSharp bindings`. |
| `DOTNET_VERSION_LIBRARY` | `3.5` | .NET Framework target version of the `Intel.RealSense` library (set in [`CMake/windows_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/windows_config.cmake)). |
| `DOTNET_VERSION_EXAMPLES` | `4.0` | .NET Framework target version of the tutorial executables (same file). |
| `BUILD_UNITY_BINDINGS` | `OFF` | Windows only. Forces `BUILD_CSHARP_BINDINGS` on and `DOTNET_VERSION_LIBRARY` to `3.5`, and adds post-build steps that copy `Intel.RealSense.dll` and `realsense2.dll` into the Unity project's `Assets\RealSenseSDK2.0\Plugins` folder under the build directory. |

### Outputs and runtime files

- The library assembly is `Intel.RealSense.dll`. The install rule in [`Intel.RealSense/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/csharp/Intel.RealSense/CMakeLists.txt) places it in `${CMAKE_INSTALL_BINDIR}`.
- An application needs both `Intel.RealSense.dll` and the native `realsense2` library next to it. [`NativeMethods.cs`](https://github.com/realsenseai/librealsense/blob/development/wrappers/csharp/Intel.RealSense/NativeMethods.cs) imports `realsense2` in a Release build of the wrapper and `realsense2d` when the wrapper is compiled with `DEBUG` defined, which matches the `d` debug postfix that `CMake/windows_config.cmake` sets for MSVC builds.
- The `cs-tutorial-load-json` project copies its `JsonConfigs` folder next to the executable before building.

### Platform notes

- CMake treats the C# bindings as Windows-only: on other systems, `wrappers/CMakeLists.txt` prints the warning `BUILD_CSHARP_BINDINGS is supported only for Windows OS` but still adds the folder. `DOTNET_VERSION_LIBRARY` and `DOTNET_VERSION_EXAMPLES` are only set by `CMake/windows_config.cmake`.
- The tutorials other than `depth` and `pose` are WPF applications (they reference `PresentationCore`, `PresentationFramework`, `System.Xaml` and `WindowsBase`).
- The library code itself chooses `memcpy` from `msvcrt.dll` on Windows and from `libc` on Unix and macOS at run time (`NativeMethods.MemCpy`).
- [`.nuget/Intel.RealSense.targets`](https://github.com/realsenseai/librealsense/blob/development/wrappers/csharp/Intel.RealSense/.nuget/Intel.RealSense.targets) copies native DLLs from `runtimes\win-x64\native` (platform `x64`) or `runtimes\win-x86\native` (`x86` or `AnyCPU`) to the output folder of a project that imports it.

## First program

This console program streams depth and prints the distance at the image centre. It is adapted from the [`depth`](https://github.com/realsenseai/librealsense/blob/development/wrappers/csharp/tutorial/depth/Program.cs) and [`pose`](https://github.com/realsenseai/librealsense/blob/development/wrappers/csharp/tutorial/pose/Program.cs) tutorials. Among others, it uses members defined in [`Pipeline.cs`](https://github.com/realsenseai/librealsense/blob/development/wrappers/csharp/Intel.RealSense/Pipeline/Pipeline.cs), [`Config.cs`](https://github.com/realsenseai/librealsense/blob/development/wrappers/csharp/Intel.RealSense/Pipeline/Config.cs), [`FrameSet.cs`](https://github.com/realsenseai/librealsense/blob/development/wrappers/csharp/Intel.RealSense/Frames/FrameSet.cs) and [`DepthFrame.cs`](https://github.com/realsenseai/librealsense/blob/development/wrappers/csharp/Intel.RealSense/Frames/DepthFrame.cs):

```cs
using System;
using Intel.RealSense;

class Program
{
    static void Main()
    {
        using (var ctx = new Context())
        {
            var devices = ctx.QueryDevices();
            if (devices.Count == 0)
            {
                Console.WriteLine("RealSense devices are not connected.");
                return;
            }

            using (var pipeline = new Pipeline(ctx))
            using (var config = new Config())
            {
                config.EnableStream(Stream.Depth);
                using (var profile = pipeline.Start(config))
                    Console.WriteLine("Device: {0}", profile.Device.Info[CameraInfo.Name]);

                for (int i = 0; i < 30; i++)
                {
                    using (var frames = pipeline.WaitForFrames())
                    using (var depth = frames.DepthFrame)
                    {
                        if (depth == null) continue;
                        Console.WriteLine("Distance at centre: {0:N3} m",
                            depth.GetDistance(depth.Width / 2, depth.Height / 2));
                    }
                }
                pipeline.Stop();
            }
        }
    }
}
```

- `Pipeline.Start()` has overloads with no arguments, with a `Config`, with a `FrameCallback`, and with a `Config` and a `FrameCallback`. It returns a `PipelineProfile`.
- `Pipeline.WaitForFrames(uint timeout_ms = 5000u)` blocks for the next `FrameSet`; `TryWaitForFrames` returns `false` instead of a frameset when nothing arrives in time, and `PollForFrames` returns immediately.
- `FrameSet.DepthFrame` returns the first depth frame in `Z16` format or `null`; `FrameSet.ColorFrame` returns the first color frame.
- `DepthFrame.GetDistance(x, y)` returns the depth in meters at the given pixel.
- Dispose frames deterministically (with `using`, or with `DisposeWith` and a `FramesReleaser`). The wrapper wraps native frame handles, and frames that are left for the garbage collector keep native resources busy.

For lower-level access the [`depth`](https://github.com/realsenseai/librealsense/blob/development/wrappers/csharp/tutorial/depth/Program.cs) tutorial skips the pipeline: it picks a depth `VideoStreamProfile` from `dev.FirstDepthSensor().StreamProfiles`, calls `Sensor.Open(profile)` and `Sensor.Start(callback)`, and copies each frame into a `ushort[]` with `VideoFrame.CopyTo`. More short recipes are in the [C# cookbook](https://github.com/realsenseai/librealsense/blob/development/wrappers/csharp/Documentation/cookbook.md); for the equivalent C++ samples see [[Code Samples]] and [[API How To]].

## Tutorials

All tutorials are in [`wrappers/csharp/tutorial`](https://github.com/realsenseai/librealsense/tree/development/wrappers/csharp/tutorial). Their sources declare the `Intel.RealSense` namespace.

| Tutorial (CMake project) | What it shows |
|---|---|
| [`depth`](https://github.com/realsenseai/librealsense/tree/development/wrappers/csharp/tutorial/depth) (`cs-tutorial-depth`) | Console app: list devices, print name, serial number and firmware version, open the depth sensor directly and draw near objects (closer than one meter) as ASCII art until Ctrl+C. |
| [`capture`](https://github.com/realsenseai/librealsense/tree/development/wrappers/csharp/tutorial/capture) (`cs-tutorial-capture`) | WPF app: stream depth and color with a `Pipeline` and `Config`, colorize depth with `Colorizer` and show both images in a window. |
| [`processing`](https://github.com/realsenseai/librealsense/tree/development/wrappers/csharp/tutorial/processing) (`cs-tutorial-processing`) | WPF app: apply the depth sensor's recommended processing blocks (`Sensor.ProcessingBlocks`), align depth to color with `Align`, colorize, and combine the results in a `CustomProcessingBlock`, using `FramesReleaser` to manage frame lifetime. |
| [`software-dev`](https://github.com/realsenseai/librealsense/tree/development/wrappers/csharp/tutorial/software-dev) (`cs-tutorial-software-dev`) | WPF app: copy live depth and color frames into a `SoftwareDevice` with two `SoftwareSensor`s, synchronize them with a `Syncer`, then colorize and display them. |
| [`d400-occ`](https://github.com/realsenseai/librealsense/tree/development/wrappers/csharp/tutorial/d400-occ) (`cs-tutorial-D400-on-chip-calibration`) | WPF app: on-chip calibration with `AutoCalibratedDevice`. Requires exactly one D400 series device; otherwise it exits with a message. See [[Calibration]]. |
| [`load-json`](https://github.com/realsenseai/librealsense/tree/development/wrappers/csharp/tutorial/load-json) (`cs-tutorial-load-json`) | WPF app: load a settings file into the camera through `SerializableDevice.JsonConfiguration`. It looks for `JsonConfigs/ADP_<model>_TEST_JSON_USB<usb type>.json`, built from the camera name and USB type; the folder ships files for a few models. See [[D400 Advanced Mode]]. |
| [`pose`](https://github.com/realsenseai/librealsense/tree/development/wrappers/csharp/tutorial/pose) (`cs-tutorial-pose`) | Console app: enable `Stream.Pose` with `Format.SixDOF` and print the device position from `PoseFrame.PoseData`. It needs a device with a pose stream. This folder has its own `CMakeLists.txt`, but `wrappers/csharp/CMakeLists.txt` does not add it, so it is not part of the generated solution. |

## Main classes

All types are in the `Intel.RealSense` namespace, with base classes in `Intel.RealSense.Base` and vector types in `Intel.RealSense.Math`. The source folders of [`wrappers/csharp/Intel.RealSense`](https://github.com/realsenseai/librealsense/tree/development/wrappers/csharp/Intel.RealSense) group them by area. Names follow the C++ API in `include/librealsense2/hpp`, converted to .NET style (`rs2::pipeline::wait_for_frames` becomes `Pipeline.WaitForFrames`).

| Folder | Main types |
|---|---|
| (root) | `Context` (`QueryDevices`, `Devices`, the `OnDevicesChanged` event, `AddDevice`/`RemoveDevice` for playback files, `ConvertBagToDb3`), `NativeMethods` (internal P/Invoke declarations) |
| `Pipeline` | `Pipeline`, `Config` (`EnableStream`, `EnableAllStreams`, `EnableDevice`, `EnableDeviceFromFile`, `EnableRecordToFile`, ...), `PipelineProfile` |
| `Devices` | `Device` (`Info`, `Sensors`, `FirstDepthSensor`, `FirstColorSensor`), `DeviceList`, `AdvancedDevice`, `SerializableDevice`, `CalibratedDevice`, `AutoCalibratedDevice`, `UpdatableDevice`, `UpdateDevice`, `DebugDevice`, `FirmwareLogsDevice`, `RecordDevice`, `PlaybackDevice`, `SoftwareDevice` |
| `Sensors` | `Sensor` (`StreamProfiles`, `Open`, `Start`, `Stop`, `Close`, `DepthScale`, `ProcessingBlocks`, options), `SoftwareSensor`, `PoseSensor`, `MaxUsableRangeSensor` |
| `StreamProfiles` | `StreamProfile`, `VideoStreamProfile`, `MotionStreamProfile`, `PoseStreamProfile` |
| `Frames` | `Frame`, `VideoFrame`, `DepthFrame`, `DisparityFrame`, `MotionFrame`, `PoseFrame`, `Points`, `FrameSet`, `FrameQueue`, `FramesReleaser` |
| `Processing` | `ProcessingBlock`, `CustomProcessingBlock`, `Align`, `Colorizer`, `PointCloud`, `Syncer`, `DecimationFilter`, `SpatialFilter`, `TemporalFilter`, `HoleFillingFilter`, `ThresholdFilter`, `DisparityTransform`, `UnitsTransform`, `ZeroOrderInvalidationFilter` (see [[Post Processing Filters]]) |
| `Options` | `IOption`, `IOptions`, `IOptionsContainer` |
| `Types` | Data types such as `Intrinsics`, `Extrinsics`, `Pose`, `SoftwareVideoStream`, `SoftwareVideoFrame`; enums in `Types/Enums` such as `Stream`, `Format`, `Option`, `CameraInfo`, `Extension`, `FrameMetadataValue`, `Rs400VisualPreset`, `Matchers` |
| `Helpers` | `Log` (`ToConsole`, `ToFile`, `LogMessage`), `ErrorMarshaler`, `ObjectPool`, `DisposableExtensions` (`DisposeWith`) |
| `Base` | `Object`, `PooledObject`, `RefCountedPooledObject` (disposable wrappers around native handles) |

Some patterns that recur in the API:

- **Extensions and casting.** `Frame.Is(Extension)` checks the native type and `Frame.As<T>()` converts to a more specific class, for example `f.As<VideoFrame>()` or `f.As<FrameSet>()`. Stream profiles work the same way (`p.As<VideoStreamProfile>()`), and device classes such as `AdvancedDevice` and `RecordDevice` provide `FromDevice(Device)`.
- **Processing.** `ProcessingBlock.Process(frame)` runs a block once; the extension method `frame.ApplyFilter(block)` does the same in a chain. `PointCloud.Calculate(frame)` returns `Points`.
- **Disposal.** `Context`, `Pipeline`, `Config`, frames, devices and sensors implement `IDisposable`. `DisposeWith(composite)` adds an object to a `FramesReleaser` or `FrameSet` so it is disposed together with it.
- **Errors.** Native errors are turned into .NET exceptions by `ErrorMarshaler`.

The folder [`wrappers/csharp/Documentation`](https://github.com/realsenseai/librealsense/tree/development/wrappers/csharp/Documentation) also contains notes on the P/Invoke design ([`pinvoke.md`](https://github.com/realsenseai/librealsense/blob/development/wrappers/csharp/Documentation/pinvoke.md)).

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`46cf1a6`](https://github.com/realsenseai/librealsense/commit/46cf1a67cc441ad9f55ce339a0de30f2a1c6fed0)._
<!-- /wikigen:footer -->
