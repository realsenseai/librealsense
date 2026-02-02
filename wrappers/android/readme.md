#  Build RealSense SDK for Android OS
This document describes how to build the Intel® RealSense™ SDK 2.0 including headless tools and examples for Android devices.

> Read about Android support [here](../../doc/android.md).

## Instructions
### Build With Gradle
To build the AAR from command line, simply navigate to `<librealsense_root_dir>/wrappers/android`
and run `gradlew assembleRelease` on Windows host or `./gradlew assembleRelease` on Linux host.
If the build succeeded, the generated AAR will be located in `<librealsense_root_dir>/wrappers/android/librealsense/build/outputs/aar`

### Build With Android Studio
1. Download and install [Android Studio IDE](https://developer.android.com/studio).
2. Clone the latest [RealSense&trade; SDK 2.0](https://github.com/realsenseai/librealsense/releases) to your host machine.
3. Open Android Studio
4. Open the Android project: Menu Bar-->File-->Open-->[path to build.gradle](./build.gradle)
5. In the gradle tasks run :librealsense-->build-->assembleRelease:
![assemblerelease](https://user-images.githubusercontent.com/18511514/52563751-f7981280-2e0a-11e9-8612-65946e86f6fc.PNG)

The build will take few minutes since the library is built for 4 different ABI configurations.
If the build succeeded, the generated AAR will be located in `<librealsense_root_dir>/wrappers/android/librealsense/build/outputs/aar`

You can also build and run any of the example apps from this project by selecting it from the 'Configuration box' and run it while your target phone is connected:
![run_example](https://user-images.githubusercontent.com/18511514/52564272-69bd2700-2e0c-11e9-94dc-8f79c60a6b42.PNG)