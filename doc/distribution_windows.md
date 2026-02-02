# Windows Distribution

**RealSense™ SDK 2.0** provides tools and binaries for the Windows platform using [GitHub Releases](https://github.com/realsenseai/librealsense/releases)

> To build from source, please follow the steps described [here](./installation_windows.md)

After plugging the camera into a USB3 port, you should be able to see the newly connected device in the Device Manager: 
![Windows Device Manager: Imaging Devices](./img/win_deploy_device_manager.PNG)

## RealSense Viewer

- Go to the [latest stable release](https://github.com/realsenseai/librealsense/releases/latest), navigate to the **Assets** section, download and run **Intel.RealSense.Viewer.exe**:
![GitHub Downloads](./img/win_deploy_downloads.PNG)

- Explore the depth data:

![Viewer](./img/windows_viewer_preview.PNG)

## Installing the SDK

* Go to the [latest stable release](https://github.com/realsenseai/librealsense/releases/latest), navigate to the **Assets** section, download and run **RealSense.SDK.exe**:

* Click through several simple steps of the installer:
1. RealSense™ SDK 2.0 is distributed under the [Apache 2.0](../LICENSE) permissive open-source license:

    ![Step 1](./img/win_step1.PNG)

2.  The SDK includes the RealSense Viewer, as well as development packages for various programming languages:

    ![Step 2](./img/win_step2.PNG)

3. Approve adding two shortcuts to your desktop:

    ![Step 3](./img/win_step3.PNG)

4. Review before installing:

    ![Step 4](./img/win_step4.PNG)
    
5. Open the `RealSense™ Samples` solution:

    ![Step 5](./img/win_shortcuts.PNG)

6. Press `F5` to compile and run the demos:

    ![Step 6](./img/win_samples.PNG)

7. Success!
