This page explains how to use the RealSense SDK 2.0 (librealsense) on Android: building the `librealsense` Android Archive (AAR) and the example apps from source, adding the AAR to your own Java or native (C/C++) app, and how the SDK gets access to the camera over USB. It is for Android developers who want to stream from a RealSense camera connected to a phone, tablet or board through a USB OTG port, without rooting the device.

## Overview

The Android support lives in [`wrappers/android`](https://github.com/realsenseai/librealsense/tree/development/wrappers/android). It is one Gradle project with three kinds of modules ([`settings.gradle`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/settings.gradle)):

| Module | What it is |
|---|---|
| `:librealsense` | The library module. It builds the native `librealsense2.so` with the Android NDK and packs it with a Java API (package `com.intel.realsense.librealsense`) into an AAR. |
| `:tools:camera` | A full camera app (package `com.intel.realsense.camera`) with preview, settings, recording, playback, device info and terminal screens. |
| `:examples:*` | Small graphical Java examples: `capture`, `playback`, `recording`, `processing`, `multicam`, `sensor`, `stream_pipeline_callback`. |

How it works:

- **Backend.** The library module passes `-DFORCE_RSUSB_BACKEND=TRUE` to CMake ([`librealsense/build.gradle`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/librealsense/build.gradle)). On Android this selects `RS2_USE_ANDROID_BACKEND` ([`CMake/android_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/android_config.cmake)), which compiles the user-space RS USB backend ([`src/rsusb-backend/`](https://github.com/realsenseai/librealsense/tree/development/src/rsusb-backend), [`src/uvc/`](https://github.com/realsenseai/librealsense/tree/development/src/uvc), [`src/hid/`](https://github.com/realsenseai/librealsense/tree/development/src/hid)) on top of the Android USB host layer ([`src/usbhost/`](https://github.com/realsenseai/librealsense/tree/development/src/usbhost)) and the JNI bindings ([`src/android/`](https://github.com/realsenseai/librealsense/tree/development/src/android)). No kernel driver or root access is needed.
- **Device access.** The native library does not open USB devices by itself. The Java side asks Android for USB permission, opens the device with `UsbManager.openDevice()` and hands the file descriptor to native code (see [USB permissions and device discovery](#usb-permissions-and-device-discovery)). Because of this, every app, including a purely native one, must call `RsContext.init()` from Java.
- **Java API.** The Java classes wrap the C API through JNI and load the native library with `System.loadLibrary("realsense2")` ([`LrsClass.java`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/librealsense/src/main/java/com/intel/realsense/librealsense/LrsClass.java)).

The repository guide ([`doc/android.md`](https://github.com/realsenseai/librealsense/blob/development/doc/android.md)) says the SDK has not been explicitly validated on Android but is expected to work, and that calibration and firmware update tools for production use are not available on Android. You need an Android device that supports USB OTG and a USB 3 OTG cable (according to doc/android.md).

![RealSense app running on Android](https://raw.githubusercontent.com/realsenseai/librealsense/development/doc/img/realsense-android.png)

## Build the AAR and the example apps

### Build settings

These values come from the Gradle files in `wrappers/android`:

| Setting | Value | Where |
|---|---|---|
| Android Gradle Plugin | `8.9.1` (`com.android.application`, `com.android.library`) | [`build.gradle`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/build.gradle) |
| `compileSdk` / `targetSdk` | 35 / 35 (`defaultCompileSdkVersion`, `defaultTargetSdkVersion`) | `build.gradle` |
| `minSdk` | 24, that is Android 7.0 (`defaultMinSdkVersion`) | `build.gradle` |
| NDK | `28.0.13004108` (`ndkVersion`) | [`librealsense/build.gradle`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/librealsense/build.gradle) |
| ABIs | `arm64-v8a`, `x86_64` (`abiFilters`) | `librealsense/build.gradle` |
| CMake arguments | `-DFORCE_RSUSB_BACKEND=TRUE` | `librealsense/build.gradle` |
| Minimum CMake | 3.10 (`cmake_minimum_required`) | [`librealsense/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/librealsense/CMakeLists.txt) |
| Java level (camera tool) | `JavaVersion.VERSION_17` | [`tools/camera/build.gradle`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/tools/camera/build.gradle) |
| AndroidX | `android.useAndroidX=true`, `android.enableJetifier=true` | [`gradle.properties`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/gradle.properties) |

Only two ABIs are built. The [`wrappers/android/readme.md`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/readme.md) still mentions four ABI configurations; `abiFilters` is what counts. If you need another ABI (for example `armeabi-v7a`), add it to `abiFilters` in `librealsense/build.gradle`.

Gradle downloads its plugins and AndroidX dependencies from `google()`, `mavenCentral()` and `gradlePluginPortal()` (`settings.gradle`), so the first build needs network access. The native build also fetches third-party sources the same way a desktop build does (see [[Build Configuration]]).

### With Android Studio

1. Install [Android Studio](https://developer.android.com/studio) with the SDK platform for API 35, the NDK version listed above and CMake (all from the SDK Manager).
2. Clone the repository:

   ```
   git clone https://github.com/realsenseai/librealsense.git
   ```

3. In Android Studio, choose **File > Open** and open the `wrappers/android` folder (the one that holds `settings.gradle`). Let Gradle sync.
4. Run the `:librealsense` > `build` > `assembleRelease` Gradle task. The first build takes a while, because the whole library is compiled once per ABI.
5. To run an example or the camera app, pick it (for example `examples.capture` or `tools.camera`) in the run configuration box and run it with your phone connected.

### From the command line

The project has `gradlew` and `gradlew.bat` scripts, but the checkout has no `gradle/wrapper/` folder (`gradle-wrapper.jar` and `gradle-wrapper.properties`), so the scripts do not work in a fresh clone. Either open the project once in Android Studio, or use a locally installed Gradle that works with Android Gradle Plugin 8.9.1. Set `sdk.dir` in `wrappers/android/local.properties`, or set `ANDROID_HOME`, so Gradle can find the SDK. Then run:

```
cd wrappers/android
gradle :librealsense:assembleRelease
```

### Where the outputs land

| Output | Location |
|---|---|
| AAR | `wrappers/android/librealsense/build/outputs/aar/` |
| Per-ABI CMake build tree of the library | `wrappers/android/build-<ABI>/` |
| Native `.so` files from CMake | `wrappers/android/build/jniLibs/<ABI>/` |
| Example and tool APKs | each module's `build/outputs/apk/` |

The two CMake paths come from [`librealsense/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/librealsense/CMakeLists.txt), which includes [`wrappers/android/config.cmake`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/config.cmake) to locate the repository root and then adds the top-level librealsense CMake project.

### What the Android build turns off

When CMake runs with the NDK toolchain, [`CMake/include_os.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/include_os.cmake) includes `CMake/android_config.cmake`. That file forces these settings, whatever you pass:

- `BUILD_UNIT_TESTS`, `BUILD_EXAMPLES`, `BUILD_TOOLS`, `BUILD_GRAPHICAL_EXAMPLES`, `BUILD_WITH_OPENMP` and `ENABLE_STATS` are `OFF`.
- `HWM_OVER_XU` is `ON`.
- `ANDROID_STL` is `c++_static`.
- The library links the Android `log` library when `BUILD_SHARED_LIBS` is `ON` (the default).

[`CMake/global_config.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/global_config.cmake) also turns off `CHECK_FOR_UPDATES` and `ENABLE_AI_ASSISTANT` and prints `Android build do not support CHECK_FOR_UPDATES/ENABLE_AI_ASSISTANT flags, turning them off..`. `ENABLE_EASYLOGGINGPP_ASYNC` defaults to `OFF` on Android ([`CMake/lrs_options.cmake`](https://github.com/realsenseai/librealsense/blob/development/CMake/lrs_options.cmake)). The SSE and NEON optimised image paths are compiled out when `ANDROID` is defined (for example in [`src/proc/color-formats-converter.cpp`](https://github.com/realsenseai/librealsense/blob/development/src/proc/color-formats-converter.cpp)).

## Use the SDK in your own app

### Add the library

The examples inside the Gradle project use the library module directly:

```groovy
dependencies {
    implementation project(':librealsense')
}
```

(from [`examples/capture/build.gradle`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/examples/capture/build.gradle)). In a separate project, add the AAR you built as a local library dependency, as described in the Android guide [Add your library as a dependency](https://developer.android.com/studio/projects/android-library#psd-add-library-dependency). The AAR declares `androidx.appcompat:appcompat` as its only runtime dependency, and its Java code uses `androidx.core`, so your app needs AndroidX.

The two standalone examples, [`examples/java_example`](https://github.com/realsenseai/librealsense/tree/development/wrappers/android/examples/java_example) and [`examples/native_example`](https://github.com/realsenseai/librealsense/tree/development/wrappers/android/examples/native_example), show a separate project instead. They pull `com.intel.realsense:librealsense:2.+@aar` from a remote Maven repository set in their `app/build.gradle`. They are older than the main project (Android Gradle Plugin 4.2.1 and 3.2.1, `minSdkVersion 19`, and the native example still uses the `com.android.support` libraries and `jcenter()`). Use them as a reference for the project layout, and point the dependency at the AAR you built yourself.

### Manifest

You do not need to declare the USB device filter yourself: the AAR's manifest, which is merged into your app, already has it (see below). Request the `CAMERA` permission, as all the camera examples do:

```xml
<uses-feature android:name="android.hardware.camera" android:required="false" />
<uses-permission android:name="android.permission.CAMERA"/>
```

The examples also ask for `CAMERA` at runtime when `Build.VERSION.SDK_INT > Build.VERSION_CODES.O`, before they touch the camera. Apps that read or write recordings also need storage or media permissions. See the manifests of [`recording`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/examples/recording/src/main/AndroidManifest.xml), [`playback`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/examples/playback/src/main/AndroidManifest.xml) and the [camera tool](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/tools/camera/src/main/AndroidManifest.xml).

### Minimal Java code

This is trimmed from [`java_example/MainActivity.java`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/examples/java_example/app/src/main/java/com/example/realsense_java_example/MainActivity.java). It starts a pipeline and reads the depth at the image centre:

```java
import com.intel.realsense.librealsense.*;

// In onCreate(): once per application lifetime, before any device access.
// Use the application context in apps with several activities.
RsContext.init(getApplicationContext());

mRsContext = new RsContext();
mRsContext.setDevicesChangedCallback(new DeviceListener() {
    @Override
    public void onDeviceAttach() { mStreamingThread.start(); }

    @Override
    public void onDeviceDetach() { mStreamingThread.interrupt(); }
});

// On a worker thread:
Pipeline pipe = new Pipeline();
try (PipelineProfile pp = pipe.start()) {}
while (!mStreamingThread.isInterrupted()) {
    try (FrameSet frames = pipe.waitForFrames()) {
        try (Frame f = frames.first(StreamType.DEPTH)) {
            DepthFrame depth = f.as(Extension.DEPTH_FRAME);
            float meters = depth.getDistance(depth.getWidth() / 2, depth.getHeight() / 2);
        }
    }
}
pipe.stop();
```

The Java objects hold native resources, so close them on time. `Frame`, `FrameSet`, `PipelineProfile`, `Config`, `DeviceList`, `RsContext` and the processing blocks all extend `LrsClass`, which implements `AutoCloseable`. Use try-with-resources for every frame: the garbage collector does not keep up with the frame rate.

To choose streams, pass a `Config` to `Pipeline.start()`, as in [`capture/MainActivity.java`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/examples/capture/src/main/java/com/intel/realsense/capture/MainActivity.java):

```java
try (Config config = new Config()) {
    config.enableStream(StreamType.DEPTH, 640, 480);
    config.enableStream(StreamType.COLOR, 640, 480);
    try (PipelineProfile pp = mPipeline.start(config)) {}
}
```

To draw frames, put a `GLRsSurfaceView` in your layout and call `upload()` with a frame set. The capture example colourizes depth first with `frames.applyFilter(mColorizer)`.

### Example apps

All of these are in [`wrappers/android/examples`](https://github.com/realsenseai/librealsense/tree/development/wrappers/android/examples):

| Example | Shows |
|---|---|
| `capture` | Depth and colour streaming with `Pipeline`, `Config`, `Colorizer` and `GLRsSurfaceView` |
| `multicam` | One `Pipeline` per connected camera, chosen with `Config.enableDevice()` |
| `sensor` | Streaming from a `DepthSensor` directly with `openSensor()` and a `FrameCallback` |
| `stream_pipeline_callback` | `Pipeline.start()` with a `FrameCallback` instead of polling, including motion frames |
| `processing` | `Align`, `DecimationFilter`, `ThresholdFilter`, `SpatialFilter`, `TemporalFilter`, `HoleFillingFilter` and `Pointcloud` |
| `recording` | Recording to a `.bag` file with `Config.enableRecordToFile()` |
| `playback` | Playing a `.bag` file with `Config.enableDeviceFromFile()` |
| `java_example` | Standalone project: minimal Java streaming (see above) |
| `native_example` | Standalone project: calling the C++ API through JNI |

[`wrappers/android/code_fragments`](https://github.com/realsenseai/librealsense/tree/development/wrappers/android/code_fragments) holds more snippets. Its readme warns that they are fragments that may not compile.

## USB permissions and device discovery

The AAR handles USB permissions for you. The parts are:

1. **Attach intent filter.** The AAR manifest ([`librealsense/src/main/AndroidManifest.xml`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/librealsense/src/main/AndroidManifest.xml)) declares an invisible `DeviceWatcherActivity` (`Theme.NoDisplay`, finishes at once) with an `android.hardware.usb.action.USB_DEVICE_ATTACHED` intent filter. Its metadata points to [`res/xml/usb_filter.xml`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/librealsense/src/main/res/xml/usb_filter.xml), which matches all products of three USB vendor IDs:

   | `vendor-id` in `usb_filter.xml` |
   |---|
   | `0x08086` |
   | `0x08087` |
   | `0x003E7` |

   There are no product ID entries, so Android offers your app for any device of these vendors.
2. **Runtime request.** When `RsContext.init()` runs, and again on each `USB_DEVICE_ATTACHED` or `USB_CONTROL_PERMISSION` broadcast, the internal `Enumerator` calls `UsbUtilities.grantUsbPermissionIfNeeded()` ([`UsbUtilities.java`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/librealsense/src/main/java/com/intel/realsense/librealsense/UsbUtilities.java)). This finds every attached USB device with vendor ID `0x8086` that the app has no permission for yet, and calls `UsbManager.requestPermission()` with a broadcast intent named `USB_CONTROL_PERMISSION`. Android then shows its USB permission dialog.
3. **Opening the device.** When `RsContext.init()` runs, when the user answers the permission dialog, or when a device is attached or detached, the internal `DeviceWatcher` ([`DeviceWatcher.java`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/librealsense/src/main/java/com/intel/realsense/librealsense/DeviceWatcher.java)) lists the attached devices with vendor ID `0x8086`. It opens new ones with `UsbManager.openDevice()` and passes the device name and file descriptor to native code (`nAddUsbDevice`). It removes devices that are gone. For each device added or removed, it calls `onDeviceAttach()` or `onDeviceDetach()` on every `DeviceListener` registered through `RsContext.setDevicesChangedCallback()`.

Only devices with vendor ID `0x8086` are asked for permission and handed to the library. The other two IDs in `usb_filter.xml` only affect the attach intent.

On Android 13 (`TIRAMISU`) and later, all three receivers are registered with `Context.RECEIVER_NOT_EXPORTED`. On older versions, only the `USB_CONTROL_PERMISSION` receiver is registered as not exported (through `ContextCompat`) ([`Enumerator.java`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/librealsense/src/main/java/com/intel/realsense/librealsense/Enumerator.java)).

## Native (C/C++) apps

### Calling the C++ API from an Android app

A native app links the same `librealsense2.so` that is inside the AAR, and uses the normal headers from [`include/librealsense2`](https://github.com/realsenseai/librealsense/tree/development/include/librealsense2). Java code is still required, because only the Java side opens USB devices. Call `RsContext.init()` and register a `DeviceListener` exactly as in a Java app, then call into your JNI code. [`native_example`](https://github.com/realsenseai/librealsense/tree/development/wrappers/android/examples/native_example) does this. Its C++ side ([`native-lib.cpp`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/examples/native_example/app/src/main/cpp/native-lib.cpp)) counts the cameras like this (it also returns `RS2_API_VERSION_STR` from a second function):

```cpp
#include <jni.h>
#include "librealsense2/rs.hpp"

extern "C" JNIEXPORT jint JNICALL
Java_com_example_realsense_1native_1example_MainActivity_nGetCamerasCountFromJNI(JNIEnv *env, jclass type) {
    rs2::context ctx;
    return ctx.query_devices().size();
}
```

Its [`app/CMakeLists.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/android/examples/native_example/app/CMakeLists.txt) imports the prebuilt library per ABI and links it:

```cmake
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/include")
add_library(realsense2 SHARED IMPORTED)
set_target_properties(
    realsense2
    PROPERTIES
    IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/build/jni/${ANDROID_ABI}/librealsense2.so
)

add_library(native-lib SHARED src/main/cpp/native-lib.cpp)
find_library(log-lib log)
target_link_libraries(native-lib ${log-lib} realsense2)
```

In that example, Gradle tasks (`extractSo`, `extractHeaders`) unpack `jni/**` from the AAR into `app/build/` and the headers from a separate zip into `src/main/cpp/include`. If you use an AAR you built yourself, do the same by hand. An AAR is a zip file, so copy `jni/<ABI>/librealsense2.so` out of it, and copy `include/` from the repository. The example compiles with `cppFlags "-fexceptions"`, because the C++ API reports errors as exceptions.

### Building only the native library with CMake

You can also configure the top-level librealsense CMake project with the NDK toolchain file, without Gradle. This is what the `Android_cpp` job in [`.github/workflows/buildsCI.yaml`](https://github.com/realsenseai/librealsense/blob/development/.github/workflows/buildsCI.yaml) does:

```
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=<path-to-ndk>/build/cmake/android.toolchain.cmake -DFORCE_RSUSB_BACKEND=true
cmake --build . -- -j4
```

Add `-DANDROID_ABI=<ABI>` and `-DANDROID_PLATFORM=<level>` to pick the target. These are standard NDK toolchain variables. CI builds Debug with NDK r20b. Using the toolchain file defines `ANDROID_NDK_TOOLCHAIN_INCLUDED`, and that is what makes CMake include `android_config.cmake`.

The backend depends on `FORCE_RSUSB_BACKEND`:

| `FORCE_RSUSB_BACKEND` | Backend | Notes |
|---|---|---|
| `ON` | `RS2_USE_ANDROID_BACKEND` | What the AAR uses. Devices come only from the Java `DeviceWatcher`, so the library must be loaded inside an app that calls `RsContext.init()`. |
| `OFF` (default) | `RS2_USE_V4L2_BACKEND` | Compiles the Linux V4L2 backend ([`src/linux/`](https://github.com/realsenseai/librealsense/tree/development/src/linux)) and libusb. It needs direct access to the video and USB device nodes, which a normal, unrooted Android app does not have. |

Android builds no SDK tools or examples (see [What the Android build turns off](#what-the-android-build-turns-off)). The `ANDROID_USB_HOST_UVC` option is deprecated. `CMakeLists.txt` tells you to use `FORCE_RSUSB_BACKEND` instead.

## Java API reference

All public classes are in [`com.intel.realsense.librealsense`](https://github.com/realsenseai/librealsense/tree/development/wrappers/android/librealsense/src/main/java/com/intel/realsense/librealsense). They follow the C++ API (see [[API How To]]):

| Area | Classes |
|---|---|
| Context and devices | `RsContext`, `DeviceListener`, `DeviceCallback`, `DeviceList`, `Device`, `UpdateDevice`, `Updatable`, `ProgressListener`, `ProductLine`, `CameraInfo`, `DebugProtocol` |
| Geometry | `Utils` (projection, deprojection, point transform, field of view), `Pixel`, `Point_3D` |
| USB plumbing | `UsbUtilities`, `DeviceWatcherActivity` |
| Sensors and options | `Sensor`, `DepthSensor`, `ColorSensor`, `RoiSensor`, `RegionOfInterest`, `Options`, `OptionsInterface`, `Option` |
| Streams | `StreamProfile`, `VideoStreamProfile`, `MotionStreamProfile`, `StreamType`, `StreamFormat`, `Intrinsic`, `MotionIntrinsic`, `Extrinsic`, `DistortionType` |
| Pipeline | `Pipeline`, `PipelineProfile`, `Config` |
| Frames | `Frame`, `FrameSet`, `VideoFrame`, `DepthFrame`, `MotionFrame`, `Points`, `FrameMetadata`, `FrameCallback`, `FrameQueue`, `FrameReleaser`, `TimestampDomain`, `Extension` |
| Processing | `Align`, `Colorizer`, `Pointcloud`, `DecimationFilter`, `ThresholdFilter`, `SpatialFilter`, `TemporalFilter`, `HoleFillingFilter`, `DisparityTransformFilter`, `HdrMerge`, `SequenceIdFilter`, `ZeroOrderInvalidationFilter`, `YuyDecoder`, `ProcessingBlock`, `Filter`, `ProcessingBlockInterface`, `FilterInterface` |
| Rendering | `GLRsSurfaceView`, `GLRenderer`, `GLFrame`, `GLVideoFrame`, `GLPointsFrame`, `GLMotionFrame` |
| Firmware logs | `FwLogger`, `FwLogMsg`, `FwLogParsedMsg` |

`RsContext.queryDevices()` uses `ProductLine.ANY_INTEL` by default. The `queryDevices(ProductLine)` overload takes another mask. `RsContext.getDeviceCount()` is deprecated in favour of `queryDevices()`. `RsContext.getVersion()` returns the library version string.

The AAR also ships the D400 visual preset JSON files under [`assets/presets`](https://github.com/realsenseai/librealsense/tree/development/wrappers/android/librealsense/src/main/assets/presets) (see [[D400 Series Visual Presets]]).

## Troubleshooting

- **No device is found, or `onDeviceAttach()` is never called.** Check that `RsContext.init()` ran before you created a `Pipeline` or called `queryDevices()`, and that the user accepted the Android USB permission dialog. If the dialog was dismissed, unplug and replug the camera. That triggers `USB_DEVICE_ATTACHED` and a new permission request. Also check that your phone supports USB OTG host mode.
- **Find out what the SDK did.** The Java classes log to logcat with tags that start with `librs`, such as `librs Enumerator`, `librs DeviceWatcher` and `librs UsbUtilities`. For example, `UsbUtilities` logs `getUsbDevice: failed to locate USB device, VID: 0x8086, PID: 0x0000` when no device with vendor ID `0x8086` is attached.
- **`UnsatisfiedLinkError` for `realsense2`.** The AAR has no native library for your device's ABI. Only `arm64-v8a` and `x86_64` are built by default. Add the ABI you need to `abiFilters` and rebuild.
- **The app runs out of memory or frames stop arriving.** Close every `Frame` and `FrameSet` with try-with-resources, and close `PipelineProfile` objects returned by `Pipeline.start()`. Native frames are not released until `close()` runs.
- **Android Studio cannot resolve the librealsense classes after you add the dependency.** The standalone example readmes suggest **File > Invalidate Caches** and a restart.
- **`./gradlew` fails in a fresh clone.** The `gradle/wrapper/` folder is not in the repository. Use Android Studio or an installed Gradle (see [From the command line](#from-the-command-line)).
- **Streams or camera behaviour.** These work the same as on other platforms. See [[Troubleshooting Q&A]] and [[Supported Devices]].

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`de7fa8c`](https://github.com/realsenseai/librealsense/commit/de7fa8cf3e05bac6f3ce6b5eeec2eb766c57259e)._
<!-- /wikigen:footer -->
