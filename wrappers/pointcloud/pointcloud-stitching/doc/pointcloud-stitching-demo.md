# rs-pointcloud-stitching guide:

`rs-pointcloud-stitching` is a demo showing how to use depth projection to combine 2 devices into a single, wide FOV, **"virtual device"**.</br>
The application constructs a new, virtual, device based on the user specifications: FOV, resolution, orientation relative to the 2 actual devices, etc. Then it converts the original depth images into points, translates them into the virtual device's coordinate system and projects them as images.
The result is a virtual device, oriented according to the user's specification, containing the combined information from 2 devices.

The application assumes the calibration matrix between the devices is known and the following guide will demonstrate how to use MATLAB's calibration tool to retrieve that matrix and then feed that calibration to the `rs-pointcloud-stitching` tool.

## Agenda
-	Mounting the cameras together.
-	Calibrating the 2 cameras:
    - Setting configuration files
    - Gathering images using `rs-pointcloud-stitching`.
    - Calculating calibration matrix using MATLAB's® "Stereo Camera Calibrator"®
    - Converting calibration matrix to rs-pointcloud-stitching's units.
-	Running `rs-pointcloud-stitching`
    - Setting configuration files
    - Running the app.

</br>

## Mounting the cameras together.
### Mounting notes:
1.	The application uses a precalculated calibration matrix. There is no real-time calibration. Therefore, the cameras should be fixed firmly in relation to each other.
2.	Although the stitching process itself is invariant to the amount of overlapping between the cameras, the calibration process may be affected by it.
3.	You can use your own mount. For the sake of this manual, we use the demonstration mount available in 2 parts here:
[Part4-base](https://librealsense.realsenseai.com/rs-tests/TestData/pc-stitching-demo-guide/Part4-base.STL)
and here:
[Part1-D435stand](https://librealsense.realsenseai.com/rs-tests/TestData/pc-stitching-demo-guide/Part1-D435stand.STL)
</br>

For this demonstration we used 2 x D435i cameras. At VGA resolution it provides Depth FOV of 75x62 degrees and Color FOV of 69x42 degrees.</br>
Setting the cameras at 60 degrees apart gives around 15 degrees overlap in the depth field which should be enough for our calibration.
</br></br>

## Calibrating the 2 cameras
In this demo we'll be using MATLAB's® "Stereo Camera Calibrator"® for calibrating the 2 devices. It is available in the "vision" toolbox. The complete guide can be found here: https://www.mathworks.com/help/vision/ug/stereo-camera-calibrator-app.html </br>
The following sections demonstrate the above procedure and describe how to use `rs-pointcloud-stitching` for gathering the required images.
### __Preparing a checkerboard target.__
- In matlab: `open checkerboardPattern.pdf`</br>
I printed the checkerboard pattern on an A3 page and set Custom Scale: 160% to have the squares as large as possible because I wanted the calibration to be at the maximal distance possible. You might choose differently as the MATLAB's guide says that it's best to match the calibration distance and the expected working distance.</br>
- Attach the printed pattern on a flat, solid surface.

###	__Gathering images for inter-cam calibration using rs-pointcloud-stitching.__
`rs-pointcloud-stitching` expects a calibration matrix between the depth streams of the 2 devices. In the D400 series the depth stream is, by design, aligned with the infrared1 stream. Therefore, the calibration process will be done using the infrared1 images and we shall configure `rs-pointcloud-stitching` to gather images accordingly.

Create a working directory for `rs-pointcloud-stitching`. Use OS conventional limitations. i.e. no spaces in name, no wildcards, etc. For this example, we'll create `C:\pc_stitching_ws`

To configure each camera, a file with the following name format is used: <serial_number>.cfg.
Obtaining the serial number can be done using `rs-enumerate-devices -s`.</br>
Assuming that the serial numbers of the connected devices are 912112073098 and 831612073525, create the following files inside the designated working directory:</br>

*912112073098.cfg*:</br>
```
INFRARED,640,480,30,Y8,1
```

*831612073525.cfg:*</br>
```
INFRARED,640,480,30,Y8,1
```

Make sure the INFRARED resolution is the same as the DEPTH resolution you wish to use later.

 
Now, run `rs-pointcloud-stitching`:
```
rs-pointcloud-stitching C:\pc_stitching_ws
```
Since no depth or color streams were specified, `rs-pointcloud-stitching` opens with only the IR frames show:
 

![rs-pointcloud-stitching ir only](https://user-images.githubusercontent.com/40540281/127966989-2474f2ab-475b-47be-bc16-547f3d2f43c9.png)


Use the button marked **"Save Frames"** to save frames of the checkerboard target in different locations. For best results capture 10-20 pairs of images, covering as much of the overlapping area as possible at different distances. The target should always be visible in both images. Checkout [MATLAB's® "Stereo Camera Calibrator"® guide](https://www.mathworks.com/help/vision/ug/stereo-camera-calibrator-app.html), "capture images" section for complete review.</br>
The images are saved in 2 separate folders, 1 for each camera, under the given `<working directory>/calibration_input` directory.</br>
In our case, they are saved here: </br> `C:\pc_stitching_ws\calibration_input`</br>
In addition, the application also creates 2 files in the calibration_input directory, describing the intrinsics of the cameras.

### __Calculating calibration matrix using MATLAB's® "Stereo Camera Calibrator"®__
- Open MATLAB and set directory to your `librealsense\wrappers\pointcloud\pointcloud-stitching` directory.
- Load intrinsics parameters using the given script:
```
>> intrin_831 = load_intrinsics("C:\pc_stitching_ws\calibration_input\intrinsics_831612073525.txt");
>> intrin_912 = load_intrinsics("C:\pc_stitching_ws\calibration_input\intrinsics_912112073098.txt");
```
- Launch stereoCameraCalibrator:
```
>> stereoCameraCalibrator
```
- Inside stereoCameraCalibrator:
    1. Use “Add Images” button to load the saved images:
        - The images were saved in the working directory under "calibration_input/<serial_number>".
        - The app calculates calibration from camera1 to camera2. Remember which device you set for which index. In this example we load the images for camera1 from "912112073098" and for camera2 from "831612073525"
        - Measure the size of a square on target using a ruler, a caliper or the most accurate option in your possession. I used a caliper to measure 36.8mm. Set the value in the "Size of checkerboard square" text box.
    - Load Intrinsics using the "Use fixed intrinsics->Load Intrinsics” button.
    - Use “Calibrate” button to calculate the calibration values.
    - Evaluate the calibration: Follow the "Evaluate Calibration Results" section in [MATLAB's® "Stereo Camera Calibrator"® guide](https://www.mathworks.com/help/vision/ug/stereo-camera-calibrator-app.html)
    - Use “Export camera parameters” to export to workspace.

![stereoCameraCalibrator](https://user-images.githubusercontent.com/40540281/127968760-c0d9d5cd-2416-45cc-ab94-c5e698768f3d.png)
 
### __Converting calibration matrix to rs-pointcloud-stitching's units.__
- Use the provided MATLAB function to create an rs-pointcloud-stiching calibration file from the calibration results:
The name of the output calibration file is unimportant as you specify it later in `rs-pointcloud-stitching` command line.
```
export_calibratrion(912112073098, 831612073525, stereoParams, 'C:\pc_stitching_ws\calibration_60m.cfg')
```
**Notice:**
- The first given serial number belongs to camera1 and the second to camera2, as specified using the "Add Images" button in stereoCameraCalibrator. **The order is important.**
- stereoParams – the parameter exported by stereoCameraCalibrator
- 'C:\pc_stitching_ws\calibration_60m.cfg' – The output calibration file.

The function will create the calibration file needed for `rs-pointcloud-stitching` based on the calculated calibration. It will also place the "virtual device" in the middle between the 2 input devices. You can alter "virtual device"'s transformation manually later if you wish. Position it aligned with one of the actual devices, for instance.

## Running rs-pointcloud-stitching

### __Setting configuration files__
1. **Defining the input stream:**</br>
    For each connected device we need a configuration file to describe the input streams.</br>
    The name of the file will be <serial_number>.cfg</br>
    As mentioned in the calibration section, in this example I used 2 x D435i with the following serial numbers: 912112073098, 831612073525 using a resolution of 640x480 pixels for depth and 640x360 for color at 30 fps. Make sure you use the same resolution by which you calibrated the cameras.
    If you reuse the files created for the gathering of images process, you can just remark the INFRARED streams using the number sign (#).</br>
    The files should look as follows:

    *912112073098.cfg:*</br>
    ```
    DEPTH,640,480,30,Z16,0
    COLOR,640,360,30,RGB8,0
    #INFRARED,640,480,30,Y8,1
    ```

    *831612073525.cfg:*</br>
    ```
    DEPTH,640,480,30,Z16,0
    COLOR,640,360,30,RGB8,0
    #INFRARED,640,480,30,Y8,1
    ```

2.	**A calibration file.**</br>
    - The name of the file is unimportant as you specify it in `rs-pointcloud-stitching` command line.</br>
    - It should contain 2 lines. One contains the calibration matrix between camera A and camera B. The other, the calibration matrix between camera B and the virtual camera that the app creates.</br>
    - The transformation parameters are 9 rotation and then 3 translation parameters.

    </br>
    As an example, a calibration file describing 2 cameras with 15 degrees between them is given:</br>

    *calibration_15.cfg*:
    ```
    912112073098, 831612073525, 0.8660254, 0,  -0.5,  0,1,0,      0.5, 0., 0.8660254, 0,0,0
    831612073525, virtual_dev, 0.96592583, 0,  0.25881905,  0,1,0,      -0.25881905, 0., 0.96592583, 0,0,0
    ```
    **Notice:** "virtual_dev" is a key word.

    If you followed the suggested calibration process, a calibration file was already created by `export_calibratrion` provided function and was named : `C:\pc_stitching_ws\calibration_60m.cfg`:

    *calibration_60m.cfg:*
    ```
    912112073098, 831612073525, 0.488316, -0.007477, -0.872635, 0.004309, 0.999972, -0.006157, 0.872656, -0.000754, 0.488334, 0.199384, 0.001205, -0.090225
    831612073525, virtual_dev, 0.862643, 0.003806, 0.505799, -0.004022, 0.999992, -0.000666, -0.505797, -0.001460, 0.862651, -0.099692, -0.000602, 0.045112
    ```

    The first line describes the calibration from camera 912112073098 to 831612073525.</br>
    The second line describes the calibration from 831612073525 to the virtual device.
 

3.	**Defining the virtual device:**</br>
`rs-pointcloud-stitching` project the depth and color images from the 2 actual devices onto a virtual device. The definitions of this "virtual device" are given using a file named `virtual_dev.cfg`. This file should contain the desired FOV and resolution for the virtual device's color and depth streams.</br>
For example:

    *virtual_dev.cfg:*</br>
    ```
    depth_width=960
    depth_height=320
    depth_fov_x=120
    depth_fov_y=60

    color_width=720
    color_height=240
    color_fov_x=120
    color_fov_y=40
    ```
    **Limitations:**</br>
    The resulting pointcloud is projected using a pinhole camera model so FOV << 180 Deg is a hard limitation for the virtual device. </br>
    A Few additional notes to bear in mind for appearance:
    -	Areas in the virtual device's FOV that are not covered by actual devices create empty spaces in the image.
    -	Try to maintain the same width-height ratio for color and depth images. 

### __Running the app.__
```
rs-pointcloud-stitching.exe C:\pc_stitching_ws calibration_60m.cfg
```
![RealSensePointcloud-StitchingExample](https://librealsense.realsenseai.com/rs-tests/TestData/pc-stitching-demo-guide/rs-pointcloud-stitching.gif)
</br>
[Download Full Resolution video](https://librealsense.realsenseai.com/rs-tests/TestData/pc-stitching-demo-guide/rs-pointcloud-stitching.mp4)
</br>
You can now see live depth and color images as if taken from the virtual device.
The application project the original images onto the virtual device. You can record this device and play it back using the realsense-viewer app.
Use the "Record" button to start and stop a recording session. It starts recording when its caption is changed to "Stop Recording" suggesting that the next press on it will stop the recording process.
The file ["record.bag"](https://librealsense.realsenseai.com/rs-tests/TestData/pc-stitching-demo-guide/record.bag) is saved under the given working directory. In this example: `C:\pc_stitching_ws\record.bag`</br>
You can now open realsense-viewer, choose "Add source->Load recorded sequence" and choose `C:\pc_stitching_ws\record.bag`. Switch to 3D view and watch the pointcloud of the extended scene.
</br>
<img align="left" src="https://librealsense.realsenseai.com/rs-tests/TestData/pc-stitching-demo-guide/RealSense-Viewer-cf2.gif">
</br>
</br>
</br>
[Download Full Resolution video](https://librealsense.realsenseai.com/rs-tests/TestData/pc-stitching-demo-guide/RealSense-Viewer.mp4)
