# rs-registration Sample

## Overview
This example performs depth to color frame registration using OpenCV. It captures a color and a depth frame and displays both of them plus the registered depth frame.
Depth registration is performed by first rectifying the images (using intrinsic parameters of the lenses).
In the second step we align the viewports (using extrinsic parameters for rotation and translation).
