The Android wrapper lets you use the RealSense SDK 2.0 (librealsense) in Android apps, from Java or from native (C/C++) code. Apps include the `librealsense` Android Archive (AAR), which builds the native library, provides a JNI-based Java API and handles USB camera access. It is for Android developers who want to stream from a RealSense camera connected to a phone, tablet or board over USB OTG.

## When you need it

- You are writing an Android app that streams depth, color or other data from a RealSense camera.
- You want to build the `librealsense` AAR from source, or try the example apps and the camera app on a device.
- You want to call librealsense from native Android code through the AAR.

## Key facts

- The wrapper is one Gradle project in [`wrappers/android`](https://github.com/realsenseai/librealsense/blob/master/wrappers/android/settings.gradle) with the library module `:librealsense`, the camera app `:tools:camera` and the examples `capture`, `playback`, `recording`, `processing`, `multicam`, `sensor` and `stream_pipeline_callback`.
- The library module builds the native code with CMake using `-DFORCE_RSUSB_BACKEND=TRUE` for the ABIs `arm64-v8a` and `x86_64` ([`librealsense/build.gradle`](https://github.com/realsenseai/librealsense/blob/master/wrappers/android/librealsense/build.gradle)).
- The project sets `minSdk` 24 and `compileSdk`/`targetSdk` 35 ([`build.gradle`](https://github.com/realsenseai/librealsense/blob/master/wrappers/android/build.gradle)).
- On Android, CMake turns off examples, tools, unit tests, OpenMP and usage statistics, links the C++ STL statically (`c++_static`) and uses the Android USB backend when `FORCE_RSUSB_BACKEND` is set ([`CMake/android_config.cmake`](https://github.com/realsenseai/librealsense/blob/master/CMake/android_config.cmake)).
- The Java API is in package `com.intel.realsense.librealsense`, with classes such as `RsContext`, `Pipeline`, `Config`, `DeviceWatcher` and the processing filters.
- The AAR declares `DeviceWatcherActivity`, which starts on `android.hardware.usb.action.USB_DEVICE_ATTACHED` for the USB vendor IDs listed in [`usb_filter.xml`](https://github.com/realsenseai/librealsense/blob/master/wrappers/android/librealsense/src/main/res/xml/usb_filter.xml).
- The standalone [`java_example`](https://github.com/realsenseai/librealsense/blob/master/wrappers/android/examples/java_example/readme.md) and [`native_example`](https://github.com/realsenseai/librealsense/blob/master/wrappers/android/examples/native_example/readme.md) projects take the AAR from a Maven repository as `com.intel.realsense:librealsense` instead of building it.
- RealSense has not explicitly validated SDK 2.0 on Android, and the calibration and firmware update tools used in production and manufacturing are not available on Android (according to doc/android.md).

## Full guide

See [doc/android.md](https://github.com/realsenseai/librealsense/blob/master/doc/android.md), and [wrappers/android/readme.md](https://github.com/realsenseai/librealsense/blob/master/wrappers/android/readme.md) for building the AAR and the apps from source.

## Related pages

- [[Build Configuration]]
- [[Supported Devices]]
- [[Code Samples]]
- [[Post Processing Filters]]

<!-- wikigen:footer -->
_Maintained from librealsense `master`; last checked at commit [`c84fe64`](https://github.com/realsenseai/librealsense/commit/c84fe6438bd14a4cc69f46deae92c993268232c6)._
<!-- /wikigen:footer -->
