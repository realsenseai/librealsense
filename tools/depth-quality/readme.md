# Depth Quality Tool

<p align="center"><img src="https://raw.githubusercontent.com/wiki/realsenseai/librealsense/res/depth-quality-glimpse.gif" /></p>


## Overview

This application allows you to test the camera’s depth quality, including: Z-Accuracy, Sub-Pixel, Z RMS errors (spatial noise), Fill Rate and Temporal Noise.
You should be able to easily get and interpret several of the depth quality metrics, or record and save the data for offline analysis.
<br>Please refer to [RealSense DepthQualityTesting White Paper](https://dev.realsenseai.com/docs/camera-depth-testing-methodology/) for further information.


## Quick Start
* Position the depth camera within a range of 0.3 - 2 meter from a flat non-reflective surface.
* Aim the camera to the target and hold it steady for several seconds till the tool is able to recognize the surface (the yellow grid in 3D view).
* Adjust the camera using the "Angle" metric to minimize the skew and make it as  perpendicular to the surface as possible.
* Inspect the calculated depth quality metrics, expand the metric properties to get more in-depth info.
* In order to get Z-Accuracy, enable the "Ground Truth" field in UI and modify it with the value obtained/measured with external tools
* Click on "Start_record" to collect both the metrics and the raw data for offline analysis.  

## Features
* 2D/3D Depth View
* Plane Fitting - using reconstructed surface
* User-defined Region of Interest
* Depth Quality metrics:
  * Z-Accuracy
  * Spatial Noise:
    * Plane Fit RMS Error
    * Sub-Pixel RMS Error
  * Fill-Rate
  * Distance to target
  * Temporal Noise
* Export metrics and device configuration
* Depth Sensor controls

## Depth Metrics elaborated
![](./res/Zi_ZPi.png)

### Fill Rate
the percentage of valid (non-zero) pixels within the user-defined Region of Interest (ROI) area.

### Distance To Target
estimated distance to an average within the ROI of the target (wall) in mm.  
_N_ - Normal of the fitted plane  
_P_ - Center of ROI(centroid)  

![](./res/distance.gif)

### Plane Fit RMS Error
the metrics represents depth pixels deviation from the calculated Plane Fit.
_Dist<sub>i</sub>_ - Distance from depth pixel coordinates to the Plane Fit (mm).  
![](./res/z_error_rms.gif)

### Sub-Pixel RMS Error
_Z<sub>i</sub>_ - Depth value of i-th pixel in the ROI (mm)  
_ZP<sub>i</sub>_ - Depth value of the i-th pixel projected onto the plane fit (mm)  
_BL_ - Stereoscopic Baseline (mm)  
_FL_ -Focal Length as a multiple of pixel width (pixels)  
_D<sub>i</sub>_ - Disparity value of i-th pixel in the ROI (pixel)  
 _DP<sub>i</sub>_ - Disparity value of i-th plane-projected pixel (pixel)

![](./res/Di.gif)  ![](./res/DPi.gif)  
![](./res/subpixel_rms.gif)

### Z Accuracy
![](./res/z_accuracy.png)  
_RotationPivot<sub>x,y,z</sub>_ - Intersection point between the center of the depth censor's ROI  and the Fitted Plane  
_PlanesOffset<sub>mm</sub>_ - Distance (signed) from the Fitted to the Ground Truth planes (mm)  
_D<sub>i</sub>_ - Distance (signed) from a depth vertex to the Fitted Plane (mm)  
_D'<sub>i</sub>_ - Z-error: Distance (signed) from the rotated _D<sub>i</sub>_ coordinate to the Ground Truth Plane (mm)  
_GT_ - Ground Truth distance to the wall (mm)  
![](./res/z_accuracy_d_rotated.gif)  
![](./res/z_accuracy_percentage.gif)

### Temporal Noise
_STD<sub>n</sub>_ - Matrix of Standard deviation calculated per pixel over N images (N is set to 40 images).  
_Median<sub>n</sub>_ - Matrix of Median calculated per pixel over N images.  
Division - Matrix consist of Division of _STD<sub>n</sub>_ by _Median<sub>n</sub>_ per pixel.  
Temporal Noise (%)  is calculated as the median of the Division matrix, multiplied by 100 to get the percentage.


<!---
Math expressions generated with
http://www.numberempire.com/texequationeditor/equationeditor.php
{D}_{i}=\frac{BL\times FL}{{Z}_{i}}  
{DP}_{i}=\frac{BL\times FL}{{ZP}_{i}}  
RMS = \sqrt{\frac{\sum_{1}^{n}{\left({D}_{i} -{DP}_{i}\right)}}{n}^{2}}
AVG = \frac{\sum_{1}^{n}{\left({Dist}_{i}\right)}}{n}
STD = \sqrt{\frac{\sum_{1}^{n}{\left({Dist}_{i}\right)}}{n}^{2}}  
ACC = 100 \times median(\frac{\sum_{1}^{n}{\left({Z}_{i}\right - GT)}}{GT})
{D'}_{mm}={D}_{i} -{Planes Offset}_{mm}
Z-Accuracy = 100 \times median(\frac{\sum_{1}^{n}{\left({D'}_{i}\right - GT)}}{GT})
--->
