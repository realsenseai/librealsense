# macOS Installation  

**Note:** macOS support for the full range of functionality offered by the SDK is not yet complete. If you need support for R200 or the ZR300, [legacy librealsense](https://github.com/realsenseai/librealsense/tree/legacy) offers a subset of SDK functionality.

## macOS 12+ (Monterey) and newer

 **sudo** required for USB access. On macOS 12+ most librealsense tools that use libusb must be run with elevated privileges. This is due to macOS USB security changes and overriding the default UVC driver.
```bash
# examples
sudo examples/rs-multicam
sudo examples/rs-enumerate-devices
sudo examples/rs-hello-realsense
sudo examples/rs-depth
 ```

**Current Limitations:**
- **RealSense Viewer is not supported** on macOS in the current release
- **Motion sensors (IMU) are disabled** on macOS in the current release

## Building from Source

1. Install CommandLineTools `sudo xcode-select --install` or download XCode 6.0+ via the AppStore
2. Install the Homebrew package manager via terminal - [link](http://brew.sh/)
3. Install the following packages via brew:
  * `brew install cmake libusb pkg-config openssl`

**Note** *librealsense* requires CMake version 3.10 that can also be obtained via the [official CMake site](https://cmake.org/download/).  

4. Clone the repo
  * `git clone https://github.com/realsenseai/librealsense.git`
5. Generate XCode project:
  * `mkdir build && cd build`
  * `sudo xcode-select --reset`
  * `cmake .. -DBUILD_EXAMPLES=true -DBUILD_GRAPHICAL_EXAMPLES=true -DFORCE_RSUSB_BACKEND=ON`
6. Build the Project
  * `make -j2`

> **Note:** On some Mac systems you might encounter `ld: library not found for -lusb-1.0` error (either in the terminal during make or in XCode) This can be worked-around by setting environment variable: `/bin/launchctl setenv LIBRARY_PATH /usr/local/lib`

> **Note:**  On some Mac systems you might encounter `Could NOT find OpenSSL` error  (Usually when setting `-DCHECK_FOR_UPDATES=ON`), this can be worked-around by setting a global variable ``export OPENSSL_ROOT_DIR=`brew --prefix openssl` ``

  **Note:** You can find more information about the available configuration options on [this wiki page](https://github.com/realsenseai/librealsense/wiki/Build-Configuration).

## Packaging your application
1. librealsense requires libusb to be bundled in the application. To fix the real-time linking, use `install_name_tool`
```
install_name_tool -change /usr/local/opt/libusb/lib/libusb-1.0.0.dylib @rpath/libusb-1.0.0.dylib librealsense2.dylib
```
2. Copy `libusb-1.0.0.dylib` and `librealsense2.dylib` to your application's `Frameworks` folder
