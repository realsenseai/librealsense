# rs-pcl Sample

## Overview
This example is a "hello-world" code snippet for RealSense cameras integration with PCL. The demo will capture a single depth frame from the camera, convert it to `pcl::PointCloud` object and perform basic `PassThrough` filter. All points that passed the filter (with Z less than 1 meter) will be marked in green while the rest will be marked in red. 

