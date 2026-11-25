## License: Apache 2.0. See LICENSE file in root directory.
## Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Read the occupancy grid and labeled point cloud params of a D5x5 camera.

Usage: python d5x5_get_depth_mapping_params.py
"""

import json
import pyrealsense2 as rs

devices = rs.context().query_devices()
if len(devices) == 0:
    raise SystemExit("No RealSense device found")

dev = devices[0]
print(dev.get_info(rs.camera_info.name),
      "| PID", dev.get_info(rs.camera_info.product_id),
      "| FW", dev.get_info(rs.camera_info.firmware_version))

# D585 legacy keeps its Depth Mapping Camera; the other D585s serve the params on the Perception sensor
cameras = [s for s in dev.query_sensors() if s.is_depth_mapping_sensor()]
depth_mapping_sensor = cameras[0].as_depth_mapping_sensor() if cameras else dev.first_perception_sensor()
params = depth_mapping_sensor.get_depth_mapping_params()
print(json.dumps(json.loads(params), indent=2))
