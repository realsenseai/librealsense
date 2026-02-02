<a name="readme-top"></a>

# Linux Ubuntu Installation


## Table of contents

  * [Prerequisites](#prerequisites)
  * [Install dependencies](#install-dependencies)
  * [Install librealsense2](#install-librealsense2)
  * [Building librealsense2 SDK](#building-librealsense2-sdk)
  * [Troubleshooting Installation and Patch-related Issues](#troubleshooting-installation-and-patch-related-issues)

**Note:** Due to the USB 3.0 translation layer between native hardware and virtual machine, the *librealsense* team does not support installation in a VM. \
If you do choose to try it, we recommend using VMware Workstation Player, and not Oracle VirtualBox for proper emulation of the USB3 controller. \
Please ensure to work with the supported Kernel versions listed [here](https://github.com/realsenseai/librealsense/releases/) and verify that the kernel is updated properly according to the instructions.


## Prerequisites

Supported versions are:  **Ubuntu 20/22/24 LTS** versions. 
> **Note:** The scripts and commands below invoke `wget, git, add-apt-repository` which may be blocked by router settings or a firewall. \
Infrequently, apt-get mirrors or repositories may also cause timeout. For _librealsense_ users behind an enterprise firewall, \
configuring the system-wide Ubuntu proxy generally resolves most timeout issues.

**Important:** Running RealSense Depth Cameras on Linux requires patching and inserting modified kernel drivers. \
Some OEM/Vendors choose to lock the kernel for modifications. Unlocking this capability may require modification of BIOS settings


## Install dependencies

1. Make Ubuntu packages up-to-date:
   ```sh
   sudo apt-get update && sudo apt-get upgrade
   ```
   > Note - Optional - Update kenel to the latest stable kernel (Assuming already supported by RealSense, see supported kernel versions below)
   
   > `sudo apt-get dist-upgrade`
   
3. Install the core packages required to build _librealsense_ binaries and the affected kernel modules:
   ```sh
   sudo apt-get install libssl-dev libusb-1.0-0-dev libudev-dev pkg-config libgtk-3-dev
   ```
   **Cmake Note:** certain _librealsense_ [CMAKE](https://cmake.org/download/) flags (e.g. CUDA) require version 3.8+ which is currently not made available via apt manager for Ubuntu LTS.
4. Install build tools
   ```sh
   sudo apt-get install git wget cmake build-essential
   ```
5. Prepare Linux Backend and the Dev. Environment \
   Unplug any connected RealSense camera and run:
   ```sh
   sudo apt-get install libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev at
   ```
5. Install IDE (Optional): \
   We use [QtCreator](https://wiki.qt.io/Main) as an IDE for Linux development on Ubuntu.

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
     git clone https://github.com/realsenseai/librealsense.git
     ```
   * Download and unzip the latest stable _librealsense2_ version from `master` branch \
     [realsenseai.zip](https://github.com/realsenseai/librealsense/archive/master.zip)

2. Run Realsense permissions script from _librealsense2_ root directory:
   ```sh
   cd librealsense
   ./scripts/setup_udev_rules.sh
   ```
   Notice: You can always remove permissions by running: `./scripts/setup_udev_rules.sh --uninstall`

3. Build and apply patched kernel modules for:
    * Ubuntu 20/22/24 (focal/jammy/noble) with LTS kernel 5.15, 5.19, 6.5, 6.8, 6.11, 6.14 \
      `./scripts/patch-realsense-ubuntu-lts-hwe.sh`
    * Ubuntu 20 with LTS kernel (< 5.13) \
     `./scripts/patch-realsense-ubuntu-lts.sh`

    **Note:** What the *.sh script perform?
    The script above will download, patch and build realsense-affected kernel modules (drivers). \
    Then it will attempt to insert the patched module instead of the active one. If failed
    the original uvc modules will be restored.
    

   >  Check the patched modules installation by examining the generated log as well as inspecting the latest entries in kernel log: \
       `sudo dmesg | tail -n 50` \
       The log should indicate that a new _uvcvideo_ driver has been registered.  
       Refer to [Troubleshooting](#troubleshooting-installation-and-patch-related-issues) in case of errors/warning reports.


## Building librealsense2 SDK

  * Navigate to _librealsense2_ root directory and run:
    ```sh
    mkdir build && cd build
    ```
  * Run cmake configure step, here are some cmake configure examples:\
    The default build is set to produce the core shared object and unit-tests binaries in Debug mode.\
    Use `-DCMAKE_BUILD_TYPE=Release` to build with optimizations.
    ```sh
    cmake ../
    ```
    Builds _librealsense2_ along with the demos and tutorials:
    ```sh
    cmake ../ -DBUILD_EXAMPLES=true
    ```
    For systems without OpenGL or X11 build only textual examples:
    ```sh
    cmake ../ -DBUILD_EXAMPLES=true -DBUILD_GRAPHICAL_EXAMPLES=false
    ```
  * Recompile and install _librealsense2_ binaries:
    ```sh
    sudo make uninstall && make clean && make && sudo make install
    ```
    **Note:** Only relevant to CPUs with more than 1 core: use `make -j$(($(nproc)-1)) install` allow parallel compilation.

    **Note:** The shared object will be installed in `/usr/local/lib`, header files in `/usr/local/include`. \
    The binary demos, tutorials and test files will be copied into `/usr/local/bin`

    **Note:** Linux build configuration is presently configured to use the V4L2 backend by default. \
    **Note:** If you encounter the following error during compilation `gcc: internal compiler error` \
    it might indicate that you do not have enough memory or swap space on your machine. \
    Try closing memory consuming applications, and if you are running inside a VM, increase available RAM to at least 2 GB. \
    **Note:** You can find more information about the available configuration options on [this wiki page](https://github.com/realsenseai/librealsense/wiki/Build-Configuration).

  
## Troubleshooting Installation and Patch-related Issues

| Error                                                                                                     | Cause                                                                      | Correction Steps                                                                                                               |
|-----------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------|
| ` Multiple realsense udev-rules were found!`                                                              | The issue occurs when user install both Debians and manual from source     | Remove the unneeded installation (manual / Deb)                                                                                |
| `git.launchpad... access timeout`                                                                         | Behind Firewall                                                            | Configure Proxy Server                                                                                                         |
| `dmesg:... uvcvideo: module verification failed: signature and/or required key missing - tainting kernel` | A standard warning issued since Kernel 4.4-30+                             | Notification only - does not affect module's functionality                                                                     |
| `sudo modprobe uvcvideo` produces `dmesg: uvc kernel module is not loaded`                                | The patched module kernel version is incompatible with the resident kernel | Verify the actual kernel 
version with `uname -r`. Revert and proceed from [Make Ubuntu Up-to-date](#install-dependencies) step |
| execution of `./scripts/patch-realsense-ubuntu-lts-hwe.sh` fails with `fatal error: openssl/opensslv.h`   | Missing Dependency                                                         | Install _openssl_ package   |
| build failure due to `fastrtps` / `fastcdr` dependencies issue | SDK tries to integrate eProsima Fast-DDS library and cannot build/find it on the current machine | build with -DBUILD_WITH_DDS=OFF |
| build failure due to `curl`  dependencies issue | SDK tries to integrate libcurl library and cannot build/find it on the current machine | build with -DCHECK_FOR_UPDATES=OFF |

  <p align="right">(<a href="#readme-top">back to top</a>)</p>
