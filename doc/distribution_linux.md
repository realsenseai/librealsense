# Linux Distribution

#### Using pre-build packages
**RealSense™ SDK 2.0** provides installation packages for Intel X86/AMD64/ARM-based Debian distributions in [`dpkg`](https://en.wikipedia.org/wiki/Dpkg) format for Ubuntu 20/22/24 [LTS](https://wiki.ubuntu.com/LTS).

> Note: For EOL Ubuntu distributions please use the following versions:  
Ubuntu 16 -> [2.51.1](https://github.com/realsenseai/librealsense/releases/tag/v2.51.1).  
Ubuntu 18 -> [2.55.1](https://github.com/realsenseai/librealsense/releases/tag/v2.55.1).  
The Realsense [DKMS](https://en.wikipedia.org/wiki/Dynamic_Kernel_Module_Support) kernel drivers package (`librealsense2-dkms`) supports Ubuntu LTS HWE kernels 5.15, 5.19 and 6.5. Please refer to [Ubuntu Kernel Release Schedule](https://wiki.ubuntu.com/Kernel/Support) for further details.

#### Configuring and building from the source code
While we strongly recommend to use DKMS package whenever possible, there are certain cases where installing and patching the system manually is necessary:
 - Using SDK with non-LTS Ubuntu kernel versions
 - Integration of user-specific patches/modules with `librealsense` SDK.
 - Adjusting the patches for alternative kernels/distributions.

The steps are described in [Linux manual installation guide](./installation.md)


## Installing the packages:
- Register the server's public key:
```
# Ensure the directory exists
sudo mkdir -p /etc/apt/keyrings

# Download and dearmor
curl -sSf https://librealsense.realsenseai.com/Debian/librealsenseai.asc | \
gpg --dearmor | sudo tee /etc/apt/keyrings/librealsenseai.gpg > /dev/null
```
Note: The keyring contains both the new RS public key and the Intel public key for old repos, ensuring compatibility with both new and existing packages.

- Make sure apt HTTPS support is installed:
`sudo apt-get install apt-transport-https`

- Add the server to the list of repositories:
```
echo "deb [signed-by=/etc/apt/keyrings/librealsenseai.gpg] https://librealsense.realsenseai.com/Debian/apt-repo `lsb_release -cs` main" | \
sudo tee /etc/apt/sources.list.d/librealsense.list
sudo apt-get update
```

- Install the libraries (see section below if upgrading packages):  
  `sudo apt-get install librealsense2-dkms`  
  `sudo apt-get install librealsense2-utils`  
  The above two lines will deploy librealsense2 udev rules, build and activate kernel modules, runtime library and executable demos and tools.  

- Optionally install the developer and debug packages:  
  `sudo apt-get install librealsense2-dev`  
  `sudo apt-get install librealsense2-dbg`  
  With `dev` package installed, you can compile an application with **librealsense** using `g++ -std=c++11 filename.cpp -lrealsense2` or an IDE of your choice.

Reconnect the RealSense depth camera and run: `realsense-viewer` to verify the installation.

Verify that the kernel is updated :    
`modinfo uvcvideo | grep "version:"` should include `realsense` string

## Upgrading the Packages:
Refresh the local packages cache by invoking:  
  `sudo apt-get update`  

Upgrade all the installed packages, including `librealsense` with:  
  `sudo apt-get upgrade`

To upgrade selected packages only a more granular approach can be applied:  
  `sudo apt-get --only-upgrade install <package1 package2 ...>`  
  E.g:   
  `sudo apt-get --only-upgrade install  librealsense2-utils librealsense2-dkms`  

## Uninstalling the Packages:
**Important** Removing Debian package is allowed only when no other installed packages directly refer to it. For example removing `librealsense2-udev-rules` requires `librealsense2` to be removed first.

Remove a single package with:   
  `sudo apt-get purge <package-name>`  

Remove all RealSense™ SDK-related packages with:   
  `dpkg -l | grep "realsense" | cut -d " " -f 3 | xargs sudo dpkg --purge`  

## Package Details:
The packages and their respective content are listed below:  

Name    |      Content   | Depends on |
-------- | ------------ | ---------------- |
librealsense2-udev-rules | Configures RealSense device permissions on kernel level  | -
librealsense2-dkms | DKMS package for Depth cameras-specific kernel extensions | librealsense2-udev-rules
librealsense2 | RealSense™ SDK runtime (.so) and configuration files | librealsense2-udev-rules
librealsense2-utils | Demos and tools available as a part of RealSense™ SDK | librealsense2
librealsense2-dev | Header files and symbolic link for developers | librealsense2
librealsense2-dbg | Debug symbols for developers  | librealsense2
librealsense2-gl | GLSL extension module runtime and configuration file | librealsense2
librealsense2-gl-dev | GLSL development header files and symbolic link | librealsense2
librealsense2-gl-dbg | GLSL debug symbols required for debugging purposes | librealsense2

**Note** The packages include binaries and configuration files only.
Use the github repository to obtain the source code.
