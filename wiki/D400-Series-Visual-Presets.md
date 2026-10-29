## Overview
The [RealSense Viewer](https://github.com/realsenseai/librealsense/tree/development/tools/realsense-viewer ) tool supports several predefined depth presets that can be selected according to the user's usage.
The predefined presets are listed in the table below. The table explains some of the recommended use cases for each predefined preset. There are also examples of images with each preset and a corresponding JSON file.<br>
The user can also modify the settings via the Advanced Mode menu and save a customized user preset. All the settings can be saved and loaded via the tool's menu. We use machine learning algorithms and capture of ground truth datasets to optimize preset for certain conditions, and we may add more presets in the future. The developers can do this too, or just start adjusting the parameters by hand, but we do not provide guidance on this.
The key to good depth is to start from our recommended defaults:
* 	D435 / D435i / D455 / D456: Use 848x480 resolution @30fps, with auto-exposure.  Use post processing with downsample 2.   
* 	D415: Use 1280x720 resolution @30fps, with auto-exposure. Use post processing with downsample 3.

If you have bad depth, please first try to use manual exposure and adjust exposure. Keep gain as low as possible (preferably 16).

For further information, please refer to [Tuning depth cameras for best performance](https://dev.realsenseai.com/docs/tuning-depth-cameras-for-best-performance).

## Supported Devices
Please refer to [latest release](https://github.com/realsenseai/librealsense/releases/latest) for the supported devices and platforms.

## Preset Table
<!-- This is commented out. 
Note: Some of the presets are different based on the resolution and some are the same for all resolutions.
The resolution range is:
* High Res  - 1280x720 or 848x480
* Med Res - 640x480 or 640x360
* Low Res  - 480x270 or 424x240
-->

| Resolution \ Preset| Use Cases recommended for usage| JSON files |
| ------------------------- | ------------------------------------------------ |------- |
| High Density | Higher Fill factor, sees more objects. (Ex. BGS and 3D Enhanced Photography,  Object recognition) |[HighDensityPreset.json](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/d400_presets/HighDensityPreset.json)|
| Medium Density | Balance between Fill factor and accuracy. |[MedDensityPreset.json](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/d400_presets/MidDensityPreset.json)|
| High Accuracy | High confidence threshold value of depth, lower fill factor.  (Ex. Object Scanning,  Collision Avoidance, Robots) |[HighAccuracyPreset.json](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/d400_presets/HighAccuracyPreset.json)|
| Hand | Good for Hand Tracking,  Gesture recognition, good edges |[HandGesturePreset.json](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/d400_presets/HandGesturePreset.json)|	 
| Left Imager Color w/o IR Pattern | Removes the Projector-generated IR pattern from left imager when streaming synthetic RGB.<br> Applies to D400, D410, D415 and D460 Devices.<br> Supported since FW ver 5.9.10+|[D415_RemoveIR.json](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/d400_presets/D415_RemoveIR.json), [D460_RemoveIR.json](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/d400_presets/D460_RemoveIR.json) |
| Default | Best Visual appeal, Clean edges,  Reduced PointCloud Spraying | [DefaultPreset_D415.json](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/d400_presets/DefaultPreset_D415.json), [DefaultPreset_D435.json](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/d400_presets/DefaultPreset_D435.json), [DefaultPreset_D455.json](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/d400_presets/DefaultPreset_D455.json)  |
| Edge Enhancement | Stronger edge preservation (same depth-control settings as the Edge Map preset below) | [EdgeMapD435.json](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/d400_presets/EdgeMapD435.json) |


<br>Additional Presets not in the Viewer (the Edge Map depth-control settings are available in the Viewer as the Edge Enhancement preset):

| Preset Use case | JSON file | Notes
| ------------------------- | --------------- | -- |
| Vacuum Cleaner | [ShortRangePreset.json](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/d400_presets/ShortRangePreset.json) |
| Body Scan | [BodyScanPreset.json](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/d400_presets/BodyScanPreset.json)|
| Edge Map| [EdgeMapD435.json](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/d400_presets/EdgeMapD435.json) | <p align="center"><b>Edges</b><br><img width=80% src="https://raw.githubusercontent.com/wiki/realsenseai/librealsense/res/edgemap.jpg" /></p>|

## Example Pictures
Please refer to the example pictures below captured with the various presets. The depth presets are evaluated and compared based on:
* Depth quality based on a 3D scene configured to include a variety of object types, textures, and distances.
* A flat target depth metric test


All pictures were taken with these settings/conditions:
* Resolution: 1280 x 720
* Frame Rate: 30
* Auto Exposure: On
* Ambient fluorescent lighting (~100 - 200 lx)

## RGB Images
The following RGB images captured as a test scene with objects ranging from ~0.6m (foreground cube and hand) to ~2m (back wall) from camera.
<p align="center"><b>D415 Camera</b><br><img width=90% src="https://user-images.githubusercontent.com/17433152/35091542-d20b510c-fc34-11e7-8f7d-9d319d39fa54.png" /></p>

<p align="center"><br><br><b>D435 Camera</b><br><img width=90% src="https://user-images.githubusercontent.com/17433152/35097687-12dea03a-fc49-11e7-9b32-7bca0731d81b.png" /></p>

## Corresponding Depth images using Visual Presets
* ### Default Preset
Best visual appeal, clean edges and reduced Point Cloud spraying.<br>

<p align="center"><b>D415 Camera</b><br><img width=90% src="https://user-images.githubusercontent.com/17433152/35091885-c6d1c6da-fc35-11e7-9484-a3b2b0214032.png" /></p>

<p align="center"><br><br><b>D435 Camera</b><br><img width=90% src="https://user-images.githubusercontent.com/17433152/35097826-be6e8898-fc49-11e7-9e2a-41e9b7bc288b.png" /></p>

* ### High Accuracy Preset
High confidence threshold value of depth, lower fill factor.<br>

<p align="center"><b>D415 Camera</b><br><img width=90% src="https://user-images.githubusercontent.com/17433152/35094329-dc3e70fc-fc3c-11e7-84be-fd220aa9fd27.png" /></p>

<p align="center"><br><br><b>D435 Camera</b><br><img width=90% src="https://user-images.githubusercontent.com/17433152/35097726-32fd30ac-fc49-11e7-8840-64b9fabaaf80.png" /></p>

* ### High Density Preset
Higher fill factor which sees more objects.<br>

<p align="center"><b>D415 Camera</b><br><img width=90% src="https://user-images.githubusercontent.com/17433152/35094430-1eb2217c-fc3d-11e7-8608-9cec38c1ba79.png" /></p>

<p align="center"><br><br><b>D435 Camera</b><br><img width=90% src="https://user-images.githubusercontent.com/17433152/35097758-5a9ccd20-fc49-11e7-96a6-7b08deac1ace.png" /></p>

* ### Medium Density Preset
Balance between fill factor and accuracy.<br>

<p align="center"><b>D415 Camera</b><br><img width=90% src="https://user-images.githubusercontent.com/17433152/35094547-7636f7c4-fc3d-11e7-9558-7a1fa68875bd.png" /></p>

<p align="center"><br><br><b>D435 Camera</b><br><img width=90% src="https://user-images.githubusercontent.com/17433152/35097774-6e200f4c-fc49-11e7-8c08-48a6af30d200.png" /></p>

* ### Hand Gesture Preset
Hand tracking and gesture recognition.<br>

<p align="center"><b>D415 Camera</b><br><img width=90% src="https://user-images.githubusercontent.com/17433152/35094642-c247fdfc-fc3d-11e7-93e5-231e85f73fef.png" /></p>

<p align="center"><img width=90% src="https://user-images.githubusercontent.com/17433152/35094669-d88cb4d6-fc3d-11e7-8d5f-5d928c59c415.png" /></p>

<p align="center"><br><br><b>D435 Camera</b><br><img width=90% src="https://user-images.githubusercontent.com/17433152/35097797-91f03a0a-fc49-11e7-8b30-4701ba9134dc.png" /></p>
<p align="center"><img width=90% src="https://user-images.githubusercontent.com/17433152/35097813-ab783496-fc49-11e7-8b73-3ac8ea08d1c1.png" /></p>

## Related Discussion

1. Best parameters for **short-range**:
* [Finding the best acquisition parameters](https://github.com/realsenseai/librealsense/issues/1171)
* [D435 Recommended configuration for near-range (< 1m) accuracy](https://github.com/realsenseai/librealsense/issues/1021)

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`65ed974`](https://github.com/realsenseai/librealsense/commit/65ed97469471f9d104969d2b7d029506058819bd)._
<!-- /wikigen:footer -->
