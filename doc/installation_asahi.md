<a name="readme-top"></a>

# Asahi Ubuntu Installation


## Table of contents

  * [Prerequisites](#prerequisites)
  * [Install dependencies](#install-dependencies)
  * [Install librealsense2](#install-librealsense2)
  * [Building librealsense2 SDK](#building-librealsense2-sdk)
  * [Troubleshooting Installation and Patch-related Issues](#troubleshooting-installation-and-patch-related-issues)

**Note:** Ubuntu Aasahi LTS has been deprecated by the team working on it. Currently, only Ubuntu Desktop 23.10 works. Check the [repository](https://github.com/UbuntuAsahi/ubuntu-asahi) for the latest information. 

**Note:** Some Mac hardware features that might be used during development (like using external displays) might not be supported on Asahi. Check the [documentation](https://asahilinux.org/fedora/#device-support) for the latest information.

**Note:** The installation has been tested on a 16'' M1 Pro MacBook Pro with Ubuntu Asahi 23.10 with the Intel RealSense D455 depth camera.

**Note:** The scripts and commands below invoke `wget, git, add-apt-repository` which may be blocked by router settings or a firewall. \
Infrequently, apt-get mirrors or repositories may also cause timeout. For _librealsense_ users behind an enterprise firewall, \
configuring the system-wide Ubuntu proxy generally resolves most timeout issues.

## Prerequisites

Apple Silicon machine running Ubuntu Asahi.

USB-A to USB-C adapter. Avoid using [small adapters](https://www.amazon.com/Henrety-Adapter-Female-Compatible-MacBook/dp/B0B4S2SKNH/ref=sr_1_11?crid=7ZELTWN9XIZ5&dib=eyJ2IjoiMSJ9.CaJU0UhhA1-dAJiLYaVrjJwpmfcLIfDwNxIladP6dT7iRz3FCge_4Gfn1J2gAqMV3uF3-KIu35i7TR5Z-0-PnVQMJLrP_0Sgz64CQv9xuvRapH2fxTt0LHZfz76wSbVq25pST-q4oE4ehi0_i4gmDBGy9aMsAwG_IKMJvz5wHcX0dxDvcxu39VfCYX1Fsy0fbXxzv2kPUMv2mjgn-CoM39DLpxqdUUu__8HUYfBXOts._Mxa1MFaQEql-xXh-oK1lvVnR02J1zQAa6zcSZZIA4Y&dib_tag=se&keywords=usb-a%2Bto%2Busb-c%2Badapter&qid=1710749441&sprefix=usb-a%2Bto%2Busb-c%2B%2Caps%2C298&sr=8-11&th=1). Using more robust adapters like the TP-LINK UC400 or the adapter sold by Apple is recommended.

Install Toshy for [Mac](https://github.com/RedBearAK/toshy?tab=readme-ov-file#requirements) shortcuts which might make pasting commands a lot easier (Optional).


## Install dependencies

1. Make Ubuntu up-to-date including the latest stable kernel:
   ```sh
   sudo apt-get update && sudo apt-get upgrade && sudo apt-get dist-upgrade
   ```
2. Install the core packages required to build _librealsense_ binaries and the affected kernel modules:
   ```sh
   sudo apt-get install libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev at
   ```
3. Prepare Linux Backend and the Dev. Environment \
   Unplug any connected Intel RealSense camera and run:
   ```sh
   sudo apt-get install libssl-dev libusb-1.0-0-dev libudev-dev pkg-config libgtk-3-dev
   ```
4. Install CMAKE:
   ```sh
   sudo apt-get install cmake
   ```
   Optionally install `cmake-gui` which will make building things like [OpenCV](https://github.com/opencv/opencv_contrib/blob/master/README.md) & [rs-kinfu](https://github.com/opencv/opencv_contrib/blob/master/README.md) a lot easier.

**Note**:

* on graphic sub-system utilization: \
*glfw3*, *mesa* and *gtk* packages are required if you plan to build the SDK's OpenGL-enabled examples. \
The *librealsens2e* core library and a range of demos/tools are designed for headless environment deployment.

* `libudev-dev` installation is optional but recommended, \
when the `libudev-dev` is installed the SDK will use an event-driven approach for triggering USB detection and enumeration, \
if not the SDK will use a timer polling approach which is less sensitive for device detection.


## Install librealsense2

1. Clone/Download the latest stable version of _librealsense2_ in one of the following ways:
   * Clone the _librealsense2_ repo
     ```sh
     git clone https://github.com/IntelRealSense/librealsense.git
     ```
   * Download and unzip the latest stable _librealsense2_ version from `master` branch \
     [IntelRealSense.zip](https://github.com/IntelRealSense/librealsense/archive/master.zip)

2. Run Intel Realsense permissions script from _librealsense2_ root directory:
   ```sh
   cd librealsense
   sudo ./scripts/setup_udev_rules.sh
   ```
   Notice: You can always remove permissions by running: `./scripts/setup_udev_rules.sh --uninstall`

## Building librealsense2 SDK

  * Navigate to _librealsense2_ root directory and run:
    ```sh
    mkdir build && cd build
    ```
  * Run cmake configure step, here are some cmake configure examples: 
   
    **Note:** Always build with `-DFORCE_RSUSB_BACKEND=true`. 

    Builds _librealsense2_ along with the demos and tutorials:
    ```sh
    cmake .. -DBUILD_EXAMPLES=true -DFORCE_RSUSB_BACKEND=true
    ```
    Builds _librealsense2_ and _pyrealsense2_ along with the demos and tutorials:
    ```sh
    cmake .. -DBUILD_PYTHON_BINDINGS=bool:true -DPYTHON_EXECUTABLE=$(which python3) -DBUILD_EXAMPLES=bool:true -DFORCE_RSUSB_BACKEND=true
    ```
  * Recompile and install:
    ```sh
    sudo make uninstall && make clean && make && sudo make -j<number of jobs> install
    ```
    **Note:** The number of jobs is limited by the amout of unified memory (RAM) and cores your Mac has. The higher the job count, the faster the process is.
    It's recommended to use at least 2. 

    For example:
    ```sh
    sudo make uninstall && make clean && make && sudo make -j2 install
    ```
    **Note:** You can find more information about the available configuration options on [this wiki page](https://github.com/IntelRealSense/librealsense/wiki/Build-Configuration). Some things might not work due to Asahi kernel limitations.

    **Note:** The shared object will be installed in `/usr/local/lib`, header files in `/usr/local/include`. 
    The binary demos, tutorials and test files will be copied into `/usr/local/bin`

    **Note:** Linux build configuration is presently configured to use the V4L2 backend by default. 
    
    **Note:** If you encounter the following error during compilation `gcc: internal compiler error` 
    it might indicate that you do not have enough memory or swap space on your machine. 



  
## Troubleshooting Installation and Patch-related Issues

| Error                                                                                                     | Cause                                                                      | Correction Steps                                                                                                               |
|-----------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------|
| ` Multiple realsense udev-rules were found!`                                                              | The issue occurs when user install both Debians and manual from source     | Remove the unneeded installation (manual / Deb)                                                                                |
| `git.launchpad... access timeout`                                                                         | Behind Firewall                                                            | Configure Proxy Server                                                                                                         |
| `sudo modprobe uvcvideo` produces `dmesg: uvc kernel module is not loaded`                                | The patched module kernel version is incompatible with the resident kernel | Verify the actual kernel version with `uname -r`. Revert and proceed from [Make Ubuntu Up-to-date](#install-dependencies) step |
| Execution of `./scripts/patch-realsense-ubuntu-lts-hwe.sh` fails with `fatal error: openssl/opensslv.h`   | Missing Dependency                                                         | Install _openssl_ package                                                                                                      |

  <p align="right">(<a href="#readme-top">back to top</a>)</p>