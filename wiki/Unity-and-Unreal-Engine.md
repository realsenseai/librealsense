librealsense ships two game-engine integrations: a Unity project in [`wrappers/unity`](https://github.com/realsenseai/librealsense/tree/development/wrappers/unity), built on top of the C# wrapper, and an Unreal Engine 4 project with a RealSense plugin in [`wrappers/unrealengine4`](https://github.com/realsenseai/librealsense/tree/development/wrappers/unrealengine4), built on the native C/C++ API. They are for developers who want live depth, color, infrared and point-cloud data from a RealSense camera inside a Unity scene or an Unreal level. This page covers how each integration is set up, what scenes, prefabs, components and Blueprint classes it contains, and the limitations stated in the code and readmes.

Both integrations target Windows: CMake refuses `BUILD_UNITY_BINDINGS` on other operating systems, and the Unreal plugin's module is whitelisted for `Win64` only.

## Unity: setting up the project

The Unity project depends on the .NET wrapper (`Intel.RealSense.dll`, see [[CSharp Wrapper]]) and the native library (`realsense2.dll`, loaded by the wrapper through `DllImport`). Both are expected in `Assets/RealSenseSDK2.0/Plugins/`. That folder is not in the repository (it is listed in [`wrappers/unity/.gitignore`](https://github.com/realsenseai/librealsense/blob/development/wrappers/unity/.gitignore)), so you have to put the two DLLs there yourself or let the CMake build do it.

### Option A: build the DLLs with CMake

The CMake option is `BUILD_UNITY_BINDINGS` (default `OFF`, defined in [`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake) with the description "Copy the unity project to the build folder with the required dependencies"). When it is on, [`wrappers/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/CMakeLists.txt):

- stops with `BUILD_UNITY_BINDINGS is supported only for Windows OS` on any OS other than Windows;
- turns on `BUILD_CSHARP_BINDINGS` and sets `DOTNET_VERSION_LIBRARY` to `3.5`. If you had not enabled C# bindings yourself, it prints the warning `Enabling BUILD_CSHARP_BINDINGS and set DOTNET_VERSION_LIBRARY to 3.5 since BUILD_UNITY_BINDINGS is enabled`;
- requires `BUILD_SHARED_LIBS` to be on for the C# bindings. Otherwise it fails with `BUILD_SHARED_LIBS flag must be enabled when generating CSharp bindings`.

From the librealsense root directory, configure a 64-bit Visual Studio build:

```
mkdir build
cd build
cmake .. -DBUILD_UNITY_BINDINGS=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_GENERATOR_PLATFORM=x64
```

Open the generated `realsense2.sln` in Visual Studio and build the `Intel.RealSense` project (in the `Wrappers/csharp` solution folder) in the Release configuration. A Debug build names the native library `realsense2d.dll`, and the copy step below does not pick that file up. `Intel.RealSense` depends on `realsense2`, so both libraries get built. The Unity-specific steps live in [`wrappers/csharp/Intel.RealSense/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/csharp/Intel.RealSense/CMakeLists.txt):

| Step | What happens |
|---|---|
| Configure | Creates `<build>\wrappers` and, if `<build>\wrappers\unity` does not exist yet, runs `mklink /D` to create a directory link at that path. |
| Post-build | `XCOPY` copies `Intel.RealSense.dll` and `realsense2.dll` from the build output directory into `<build>\wrappers\unity\Assets\RealSenseSDK2.0\Plugins\`. |
| Post-build (only if Unity is found) | Runs `Unity.exe -quit -batchmode -projectPath <build>\wrappers\unity -exportPackage "Assets" <OutDir>realsense.unitypackage` and writes the log to `<OutDir>Unity.log`. Failures in this step are ignored (the command ends with `EXIT 0`). |

CMake finds Unity with `find_program(UNITY_PATH Unity\\Editor\\Unity.exe)`. To use a specific editor, pass `-DUNITY_PATH=<path to>\Unity.exe`. If CMake does not find Unity, it prints the warning `Couldn't locate Unity.exe` and skips the package step. The DLLs are still copied.

The link target is computed from the `Intel.RealSense` project's source directory, so it points to `wrappers\csharp\Intel.RealSense\wrappers\unity` in the source tree. That folder is not in the repository, so `<build>\wrappers\unity` does not contain the Unity project. Use Option B and copy the DLLs by hand.

The .NET target framework of `Intel.RealSense` is `v${DOTNET_VERSION_LIBRARY}`. On Windows that variable is a cache entry that defaults to `3.5`, described in [`CMake/windows_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/windows_config.cmake) as ".Net Version, defaulting to '3.5', the Unity wrapper currently supports only .NET 3.5".

### Option B: copy the DLLs into the project by hand

1. Build or obtain `Intel.RealSense.dll` (targeting .NET 3.5) and the 64-bit `realsense2.dll`. See [[CSharp Wrapper]] and [[Installation Windows]].
2. Copy both files into `wrappers/unity/Assets/RealSenseSDK2.0/Plugins/`. Create the folder if it does not exist.
3. Open the `wrappers/unity` folder as a project in the Unity editor.

The project's [`ProjectSettings/ProjectVersion.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/unity/ProjectSettings/ProjectVersion.txt) records editor version `2017.4.13f1`. The repository guide ([`wrappers/unity/readme.md`](https://github.com/realsenseai/librealsense/blob/development/wrappers/unity/readme.md)) says the examples were designed and tested with Unity 2017-2020 versions no higher than 2020.1.8f1, and that newer versions may require code changes.

### Option C: import a Unity package

If you have a `realsense.unitypackage` (built by Option A when Unity is found), import it into a Unity project with the Unity editor. The repository guide points to the GitHub releases page for a prebuilt package.

### Running the samples

Open `Assets/RealSenseSDK2.0/Scenes/StartHere.unity`. It is the only scene listed in the project's build settings ([`ProjectSettings/EditorBuildSettings.asset`](https://github.com/realsenseai/librealsense/blob/development/wrappers/unity/ProjectSettings/EditorBuildSettings.asset)). Its menu loads the sample scenes through [`SceneLoader.cs`](https://github.com/realsenseai/librealsense/blob/development/wrappers/unity/Assets/RealSenseSDK2.0/Misc/Utils/SceneLoader.cs). `SceneLoader` loads `SampleSceneUI` and then loads the chosen sample additively. On its first `Awake` it also loads the `sample_assets` and `sample_scenes` asset bundles from `Assets/StreamingAssets`.

You can also open any scene under `Assets/RealSenseSDK2.0/Scenes/Samples/` directly.

## Unity: scenes

All scenes are in [`Assets/RealSenseSDK2.0/Scenes`](https://github.com/realsenseai/librealsense/tree/development/wrappers/unity/Assets/RealSenseSDK2.0/Scenes):

| Scene | What it shows |
|---|---|
| `StartHere.unity` | Menu that opens all of the samples below. |
| `Samples/TexturesDepthAndInfrared.unity` | 2D scene that binds depth and infrared streams to textures. |
| `Samples/TexturesDepthAndColor.unity` | 2D scene that binds depth and color streams to textures. Needs a camera with an RGB sensor. |
| `Samples/BackgroundSegmentation.unity` | Color with background segmentation (uses an `RsProcessingPipe` with profile `ProcessingPipe/BackgroundSegmentation.asset`, and the `BGSeg` and `Grayscale` shaders/materials). |
| `Samples/AlignmentSample.unity` | Alignment through an `RsProcessingPipe` (profile `ProcessingPipe/AlignmentSample.asset`). |
| `Samples/PointCloudProcessingBlocks.unity` | 3D point cloud with a processing-block chain whose blocks can be toggled at run time (profile `ProcessingPipe/PointCloudProcessingBlocks.asset`). |
| `Samples/PointCloudDepthAndColor.unity` | 3D point cloud textured with the color stream (profile `ProcessingPipe/PointCloudDepthAndColor.asset`). |
| `Samples/ARBackground.unity` | Camera image as an augmented-reality background (`RsARBackgroundRenderer`, `ARBackground` shader). |
| `Samples/SLAM.unity` | Drives a transform from the pose stream with `RsPoseStreamTransformer`. It needs a device that produces a 6-DoF pose stream and includes the `Models/T265.fbx` model. |

`Misc/SampleSceneUI.unity` is the shared UI scene (it contains the `SceneLoader`, a back button and an error display) that `SceneLoader.LoadScene` loads before each sample.

## Unity: prefabs and components

Prefabs in [`Assets/RealSenseSDK2.0/Prefabs`](https://github.com/realsenseai/librealsense/tree/development/wrappers/unity/Assets/RealSenseSDK2.0/Prefabs):

| Prefab | Contents |
|---|---|
| `RsDevice.prefab` | An `RsDevice` with child objects that use `RsStreamTextureRenderer` to feed textures. |
| `RsProcessingPipe.prefab` | An `RsProcessingPipe` that applies an `RsProcessingProfile` to frames from another frame provider. |
| `PointCloud.prefab` | A mesh object with `RsPointCloudRenderer`. |

Scripts in [`Assets/RealSenseSDK2.0/Scripts`](https://github.com/realsenseai/librealsense/tree/development/wrappers/unity/Assets/RealSenseSDK2.0/Scripts) (assembly definition `RealSense`):

| Component / type | Role |
|---|---|
| `RsFrameProvider` | Abstract base `MonoBehaviour` with `Streaming`, `ActiveProfile` and the events `OnStart`, `OnStop` and `OnNewSample`. |
| `RsDevice` | Frame provider that owns a `Pipeline` and starts it from its `DeviceConfiguration` in `OnEnable`. |
| `RsProcessingPipe` | Frame provider that takes frames from a `Source` provider and runs them through the blocks of its `profile`. |
| `RsStreamTextureRenderer` | Creates a `Texture2D` for frames that match the selected stream, format and index, and passes it to the `textureBinding` event. |
| `RsPointCloudRenderer` | Builds a point mesh from `Points` frames and sets a `_UVMap` texture on the material. |
| `RsARBackgroundRenderer` | Renders a frame provider's image behind the scene with a material. |
| `RsPoseStreamTransformer` | Applies `Pose`/`SixDOF` frames to the GameObject's transform. |
| `RsDeviceInspector` | Lists the running device's sensors and options in the Inspector (custom editor `RsDeviceInspectorEditor`). It looks up `RsDevice` with `GetComponent`, so add it to the same GameObject as `RsDevice`. |
| `RsConfiguration`, `RsVideoStreamRequest` | Serializable configuration used by `RsDevice`. |

### Configuring RsDevice

`RsDevice` has two settings:

- **`processMode`** (`ProcessMode` enum):
  - `Multithread`: a background thread calls `Pipeline.WaitForFrames()` and raises `OnNewSample` from that thread.
  - `UnityThread`: `Update()` calls `Pipeline.PollForFrames()` and raises `OnNewSample` on the Unity main thread.
- **`DeviceConfiguration`** (`RsConfiguration`): `mode` is one of `Live`, `Playback` or `Record`. The other fields are `Profiles` (array of `RsVideoStreamRequest`), `RequestedSerialNumber`, `PlaybackFile` and `RecordPath`. `ToPipelineConfig()` turns the configuration into a `Config`:
  - `Live` calls `EnableDevice(RequestedSerialNumber)` and `EnableStream(...)` for each profile.
  - `Playback` calls `EnableDeviceFromFile(PlaybackFile)`. If `PlaybackFile` is empty, it falls back to `Live` mode with no streams enabled.
  - `Record` enables the profiles and calls `EnableRecordToFile(RecordPath)` when the path is set.

With an empty `Profiles` array no stream is enabled explicitly, so the pipeline uses the device's default configuration. The default `DeviceConfiguration` requests these streams:

| Stream | Width x Height | Format | FPS |
|---|---|---|---|
| Depth | 640 x 480 | `Z16` | 30 |
| Infrared | 640 x 480 | `Y8` | 30 |
| Color | 640 x 480 | `Rgb8` | 30 |

After the pipeline starts, `Profiles` is overwritten with the streams that are actually active. The custom Inspector (`Editor/RsDeviceEditor.cs`) greys out the mode and process-mode controls while the device is streaming.

Recorded files are librealsense `.bag` recordings; see [[Record and Playback]].

### Processing profiles and blocks

`RsProcessingProfile` is a `ScriptableObject` marked `[CreateAssetMenu]`, so you can create one from the Unity *Create* menu. The custom editor (`Editor/RsProcessingPipeEditor.cs`) shows the blocks as a reorderable list. Its add menu lists every type that has a `ProcessingBlockData` attribute, and each new block is stored inside the profile asset. Assign the profile to an `RsProcessingPipe`, and point its `Source` at an `RsDevice` or another `RsProcessingPipe`.

The blocks in [`Scripts/ProcessingBlocks`](https://github.com/realsenseai/librealsense/tree/development/wrappers/unity/Assets/RealSenseSDK2.0/Scripts/ProcessingBlocks):

| Unity block | Wraps | Fields with defaults (from the script) |
|---|---|---|
| `RsDecimationFilter` | `DecimationFilter` | `_filterMagnitude = 2` |
| `RsThresholdFilter` | `ThresholdFilter` | `MinDistance = 0.1`, `MaxDistance = 4` |
| `RsDisparityTransform` | `DisparityTransform` | `Mode = DisparityMode.DepthToDisparity` |
| `RsSpatialFilter` | `SpatialFilter` | `_filterMagnitude = 2`, `_filterSmoothAlpha = 0.5`, `_filterSmoothDelta = 20`, `_holeFillingMode = 0` (`HoleFillingMode.Disabled`) |
| `RsTemporalFilter` | `TemporalFilter` | `_filterSmoothAlpha = 0.4`, `_filterSmoothDelta = 20`, `_temporalPersistence = 3` |
| `RsHoleFillingFilter` | `HoleFillingFilter` | `_holesFill = HoleFillingMode.FillFromLeft` |
| `RsColorizer` | `Colorizer` | `histogramEqualization = true`, `minDist = 0`, `maxDist = 6` |
| `RsAlign` | `Align` | `_alignTo = Stream.Depth` |
| `RsPointCloud` | `PointCloud` | `TextureStream = Stream.Color`, `TextureFormat = Format.Any`, `_occlusionRemoval = OcclusionRemoval.Off` |
| `DepthCutoff` | custom block | `Distance = 1000` |

Every block derives from `RsProcessingBlock`, which has an `enabled` flag (default `true`). For what each filter does, see [[Post Processing Filters]].

### Shaders, materials and helpers

- `Shaders/` contains `Depth`, `RGB`, `Grayscale`, `BW`, `BGSeg`, `ARBackground`, `PointCloud`, `PointCloudGeom`, `UnlitTransparentShader` and `Custom Skybox`. The matching materials are in `Materials/` (for example `depthMat`, `RGBMat`, `IRmat`, `BGSegMat`, `PointCloudMat`, `ARBackgroundMat`).
- `Misc/Utils/` (assembly definition `Utils`, which references `RealSense`) holds sample UI helpers such as `SceneLoader`, `ErrorPopup`, `OrbitCameraControl`, `MinMaxSlider` and `RsStreamAvailability`.
- `Misc/Editor/CreateAssetBundles.cs` adds an *Assets > Build AssetBundles* menu item. It writes the bundles to `Assets/StreamingAssets` for `StandaloneLinux64` when run in the Linux editor (`UNITY_EDITOR_LINUX`) and for `StandaloneWindows64` otherwise.

### Using RsDevice from a script

The following uses only members defined in the Unity scripts and the C# wrapper:

```csharp
using UnityEngine;
using Intel.RealSense;

public class FrameCounter : MonoBehaviour
{
    public RsFrameProvider Source;   // drag an RsDevice or RsProcessingPipe here
    int count;

    void Start()
    {
        Source.OnStart += profile => Debug.Log("Streaming started");
        Source.OnNewSample += OnFrame;
    }

    void OnFrame(Frame frame)
    {
        // With RsDevice.ProcessMode.Multithread this runs on a worker thread:
        // do not touch Unity objects here.
        count++;
    }
}
```

The bundled renderers (`RsStreamTextureRenderer`, `RsPointCloudRenderer`) follow the same pattern. They subscribe to `Source.OnStart`, then to `OnNewSample`, push frames into a `FrameQueue` of size 1, and upload them to Unity in `LateUpdate()`.

## Unreal Engine 4: setting up the project

[`wrappers/unrealengine4`](https://github.com/realsenseai/librealsense/tree/development/wrappers/unrealengine4) is a complete UE4 C++ project (`RealSenseUE.uproject`) with two project plugins:

- `Plugins/RealSense`: the RealSense plugin. Its [`RealSense.uplugin`](https://github.com/realsenseai/librealsense/blob/development/wrappers/unrealengine4/Plugins/RealSense/RealSense.uplugin) has the friendly name "RealSense Plugin (alpha)", version name `0.0.1` and category "Input Devices". Its runtime module `RealSense` uses loading phase `PostConfigInit` and is whitelisted for `Win64`.
- `Plugins/RuntimeMeshComponent`: the third-party Runtime Mesh Component plugin (version name `3.2`). The RealSense plugin depends on it to build the point-cloud mesh.

[`RealSenseUE.uproject`](https://github.com/realsenseai/librealsense/blob/development/wrappers/unrealengine4/RealSenseUE.uproject) sets `"EngineAssociation": "4.26"` and declares one runtime module, `RealSenseUE`. The repository guide ([`wrappers/unrealengine4/README.md`](https://github.com/realsenseai/librealsense/blob/development/wrappers/unrealengine4/README.md)) describes the wrapper as working with UE4 4.19 and later.

### Building

1. Install the RealSense SDK 2.0 on Windows (see [[Installation Windows]]). The plugin needs the librealsense headers, the `realsense2.lib` import library and `realsense2.dll`.
2. Check the SDK path in [`Plugins/RealSense/Source/RealSense/RealSense.Build.cs`](https://github.com/realsenseai/librealsense/blob/development/wrappers/unrealengine4/Plugins/RealSense/Source/RealSense/RealSense.Build.cs). For `Win64` it hard-codes:
   ```csharp
   string RealSenseDirectory = "C:/Program Files (x86)/Intel RealSense SDK 2.0";
   ```
   It adds `<RealSenseDirectory>/include` to the include path, adds `lib/x64` and `bin/x64` to the library paths, and links `realsense2.lib`. If your SDK is somewhere else, edit that line.
3. Generate Visual Studio project files for `RealSenseUE.uproject` with your Unreal Engine 4 installation.
4. Open the generated `RealSenseUE.sln` and build the `DevelopmentEditor` configuration.
5. Make `realsense2.dll` loadable at run time. `RealSense.Build.cs` does not copy it. The repository guide suggests copying it either to the engine's `Engine\Binaries\Win64` folder or to the plugin's build output folder `Plugins\RealSense\Binaries\Win64`.
6. Open `RealSenseUE.uproject` and press *Play*. The `Content/TestLevel.umap` level shows depth, color, infrared and point-cloud examples.

The first call to `IRealSensePlugin::Get().GetContext()` creates the librealsense context with `rs2_create_context(RS2_API_VERSION, ...)`, so the headers the plugin is compiled against must be compatible with the `realsense2.dll` loaded at run time. If context creation fails, the plugin (`RealSensePluginImpl.cpp`) logs `rs2_create_context failed: ` and `ARealSenseInspector::Start` reports `GetContext failed`.

`RealSense.Build.cs` also contains a Linux branch (headers in `/usr/local/include`, library `/usr/local/lib64/librealsense2.so`), but `RealSense.uplugin` whitelists only `Win64`.

### Using the plugin in your own project

Copy `Plugins/RealSense` and `Plugins/RuntimeMeshComponent` into your project's `Plugins` folder. For C++ use, add `"RealSense"` to your module's dependencies, the same way [`Source/RealSenseUE/RealSenseUE.Build.cs`](https://github.com/realsenseai/librealsense/blob/development/wrappers/unrealengine4/Source/RealSenseUE/RealSenseUE.Build.cs) does (`PublicDependencyModuleNames` and `PublicIncludePathModuleNames`). If you use the Blueprint helpers, migrate `BP_RealSenseAPI` and `BP_RealSenseInspector` from `Content/` as well. The repository guide says to place `BP_RealSenseAPI` in the level whenever you create a new one with the inspector.

## Unreal Engine 4: content and Blueprint classes

### Content

| Asset in `Content/` | Purpose |
|---|---|
| `TestLevel.umap` | Demo level. |
| `BP_RealSenseAPI.uasset` | Blueprint with the API examples `ExEnumerateHardware`, `ExModifyOption`, `ExPresetLoadSave`, `ExStreamConfiguration` and `ExRecordPlayback` (names from the repository guide). |
| `BP_RealSenseInspector.uasset` | Blueprint built on `ARealSenseInspector` that configures streams and binds the live textures to scene objects. |
| `M_Depth`, `M_Color`, `M_Infrared`, `M_BGSeg` | Materials for the stream textures. |
| `T_Depth.uasset` | Depth texture asset. |

![Unreal Engine 4 demo level](https://raw.githubusercontent.com/realsenseai/librealsense/development/wrappers/unrealengine4/doc/img/demo.jpg)

### Blueprint classes

All of these are in [`Plugins/RealSense/Source/RealSense/Public`](https://github.com/realsenseai/librealsense/tree/development/wrappers/unrealengine4/Plugins/RealSense/Source/RealSense/Public) and are `BlueprintType` in class group "RealSense":

| Class | Blueprint-callable functions | Blueprint-readable properties |
|---|---|---|
| `URealSenseContext` | `GetRealSense()` (static), `QueryDevices()`, `GetDeviceById(int)`, `FindDeviceBySerial(FString)` | `Devices` |
| `URealSenseDevice` | `GetSensor(ERealSenseStreamType)`, `GetStreamProfiles(ERealSenseStreamType)`, `SupportsProfile(FRealSenseStreamProfile)`, `LoadPreset(FileName)`, `SavePreset(FileName)` | `Sensors`, plus name and serial |
| `URealSenseSensor` | `SupportsOption(ERealSenseOptionType)`, `SupportsProfile(FRealSenseStreamProfile)`, `GetOption(ERealSenseOptionType)` | `Options`, `StreamProfiles`, plus name |
| `URealSenseOption` | `QueryValue()`, `SetValue(float)` | option type, name, description, range |
| `ARealSenseInspector` (actor) | `Start()`, `Stop()`, `SetPointCloudMaterial(int SectionId, UMaterialInterface*)` | `Context`, `ActiveDevice`, `DepthScale`, the textures, `PclMesh`, `PclMaterial` |

`LoadPreset` and `SavePreset` read and write JSON presets through `rs2_load_json` and `rs2_serialize_json` (see [[D400 Series Visual Presets]] and [[D400 Advanced Mode]]). The types in `RealSenseTypes.h` include:

- `ERealSenseStreamType`: `STREAM_ANY`, `STREAM_DEPTH`, `STREAM_COLOR`, `STREAM_INFRARED`
- `ERealSensePipelineMode`: `CaptureOnly`, `RecordFile`, `PlaybackFile`
- `ERealSenseDepthColormap`: `Jet`, `Classic`, `WhiteToBlack`, `BlackToWhite`, `Bio`, `Cold`, `Warm`, `Quantized`, `Pattern`
- the structs `FRealSenseStreamProfile`, `FRealSenseStreamMode` and `FRealSenseOptionRange`, plus the enums `ERealSenseFormatType` and `ERealSenseOptionType`

### ARealSenseInspector properties

These are editable in the Details panel and from Blueprints. Defaults are taken from `RealSenseInspector.h`:

| Category | Property | Default | Notes |
|---|---|---|---|
| Device | `DeviceSerial` | empty | Selects a device by serial number. |
| Device | `PipelineMode` | `CaptureOnly` | `RecordFile` records to `CaptureFile`; `PlaybackFile` plays `CaptureFile`. |
| Device | `CaptureFile` | empty | Recording or playback file. |
| Device | `bEnablePolling` | `false` | Poll at `PollFrameRate` instead of waiting for frames. |
| Device | `PollFrameRate` | `1/60` | Range 0.001-1. |
| Device | `WaitFrameTimeout` | `1.0` | Range 0.001-10. |
| DepthStream | `DepthConfig` | 640 x 480 @ 30 | `FRealSenseStreamMode` (`Width`, `Height`, `Rate`). Depth is requested as `Z16`. |
| DepthStream | `bEnableRawDepth`, `bEnableColorizedDepth` | `true`, `true` | Produce `DepthRawTexture` and `DepthColorizedTexture`. |
| DepthStream | `bAlignDepthToColor` | `true` | |
| DepthStream | `bEqualizeHistogram`, `DepthMin`, `DepthMax`, `DepthColormap` | `true`, `0`, `10`, first enum value | Settings for the colorized depth texture. |
| ColorStream | `ColorConfig`, `bEnableColor` | 640 x 480 @ 30, `true` | Color is requested as `RGBA8` and output as `ColorTexture`. |
| InfraredStream | `InfraredConfig`, `bEnableInfrared` | 640 x 480 @ 30, `true` | Infrared is requested as `Y8` and output as `InfraredTexture`. |
| PointCloud | `bEnablePcl` | `false` | Builds the point cloud into `PclMesh` (a `URuntimeMeshComponent`). |
| PointCloud | `PclScale`, `PclVoxelSize`, `PclDensity`, `PclRenderRate` | `1.0`, `1.0`, `0.2`, `5` | Cloud scale, point size, fraction of points drawn, time in seconds between mesh updates. |

The plugin also adds a details-panel customization ([`RealSenseDetailCustomization.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/unrealengine4/Plugins/RealSense/Source/RealSense/Private/RealSenseDetailCustomization.cpp)) that acts as a hardware inspector. It has a *Refresh* button, shows each device's serial number, has *Load preset* and *Save preset* buttons (file filter `*.json`), and lists the sensor options.

![Unreal Engine 4 hardware inspector](https://raw.githubusercontent.com/realsenseai/librealsense/development/wrappers/unrealengine4/doc/img/hw_inspector.jpg)

### Native C++ access

`IRealSensePlugin::Get().GetContext()` returns the plugin's `URealSenseContext`. [`Source/RealSenseUE/RealSenseTestActor.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/unrealengine4/Source/RealSenseUE/RealSenseTestActor.cpp) shows how to use it:

```cpp
#include "RealSensePlugin.h"
#include "RealSenseContext.h"
#include "RealSenseDevice.h"

auto Context = IRealSensePlugin::Get().GetContext();
Context->QueryDevices();
for (auto Device : Context->Devices)
{
    UE_LOG(LogRealSenseDemo, Display, TEXT("Wrapper device %s"), *(Device->Name));
}
```

`RealSenseNative.h` exposes the librealsense C++ headers if you want to call the native API directly.

## Limitations

### Unity

- Windows only through CMake (`BUILD_UNITY_BINDINGS is supported only for Windows OS`). The managed assembly targets .NET 3.5.
- On Unity 2020.1 or newer (`UNITY_2020_1_OR_NEWER`), [`RsARBackgroundRestrictions.cs`](https://github.com/realsenseai/librealsense/blob/development/wrappers/unity/Assets/RealSenseSDK2.0/Scripts/RsARBackgroundRestrictions.cs) hides the "AR" entry in the `StartHere` menu.
- When the streaming device's name starts with "Intel RealSense L5", `RsProcessingPipe` disables every processing-block toggle in the PointCloudProcessingBlocks UI except the temporal filter.
- `RsStreamTextureRenderer` converts only some formats to Unity textures (`Z16`, `Disparity16`, `Y16`, `Raw16` to `R16`; `Rgb8` to `RGB24`; `Rgba8` to `RGBA32`; `Bgra8` to `BGRA32`; `Y8`, `Raw8` to `Alpha8`; `Disparity32` to `RFloat`). Other formats, such as `Yuyv` and `Bgr8`, are not converted.
- In `Multithread` mode, `OnNewSample` handlers run on a worker thread.
- The repository guide says the samples were tested only up to Unity 2020.1.8f1.

### Unreal Engine 4

- The plugin is labelled "alpha" and whitelisted for `Win64` only. The SDK path in `RealSense.Build.cs` is hard-coded.
- The `ARealSenseInspector` constructor calls `GetDeviceDefaultProfileConfig()`, which overwrites `DepthConfig`, `ColorConfig` and `InfraredConfig` with the last profile that the device at index 0 of the context's device list reports for each stream (it ignores `DeviceSerial`). If depth and color then differ in size, it sets `bAlignDepthToColor` to `false`.
- Stream modes are read in `Start()`. Calling `Start()` while already running makes the plugin log `ARealSenseInspector::Start exception: Already started`, stop the running pipeline and return `false`. To change a video mode, call `Stop()` and then `Start()`.
- The repository guide says depth and infrared should use the same video mode when both are running.
- The repository guide lists these known issues:
  - point-cloud support is experimental;
  - high CPU usage;
  - colorized depth is CPU-bound and should be avoided at high resolutions and frame rates;
  - point-cloud update and rendering can be slow;
  - there can be frame drops or timeouts at high resolutions;
  - point-cloud edges can show texture corruption;
  - rebuilding project files can fail after switching engine versions (cleaning the solution or deleting the `Intermediate` directory fixes it).

## Related pages

- [[CSharp Wrapper]]: the .NET API the Unity project is built on
- [[Build Configuration]]: all CMake options
- [[Record and Playback]]: the file format used by the record and playback modes
- [[Post Processing Filters]]: the filters behind the Unity processing blocks
- [[Code Samples]]

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`46cf1a6`](https://github.com/realsenseai/librealsense/commit/46cf1a67cc441ad9f55ce339a0de30f2a1c6fed0)._
<!-- /wikigen:footer -->
