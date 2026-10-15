## License: Apache 2.0. See LICENSE file in root directory.
## Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Write occupancy grid and labeled point cloud params to a D5x5 camera.

Usage: python d5x5_set_depth_mapping_params.py [params.json]
Defaults to d5x5_depth_mapping_params.json next to this script. Only the keys in the
file change; the camera keeps the rest. The first write to a never-configured camera
must include camera_position.
"""

import json
import os
import sys
import pyrealsense2 as rs

path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "d5x5_depth_mapping_params.json")
with open(path) as f:
    params = json.load(f)

# Compact: the whole JSON document has to fit in 1016 bytes
params_str = json.dumps(params, separators=(",", ":"))
print("Setting", len(params_str), "bytes from", path)

devices = rs.context().query_devices()
if len(devices) == 0:
    raise SystemExit("No RealSense device found")

dev = devices[0]
print(dev.get_info(rs.camera_info.name),
      "| PID", dev.get_info(rs.camera_info.product_id),
      "| FW", dev.get_info(rs.camera_info.firmware_version))

depth_mapping_sensor = dev.first_depth_mapping_sensor()
try:
    # Blocks until the camera has applied the params or rejected them
    depth_mapping_sensor.set_depth_mapping_params(params_str)
except RuntimeError as e:
    raise SystemExit(f"Set failed: {e}")

print("Set OK; camera params now:")
print(json.dumps(json.loads(depth_mapping_sensor.get_depth_mapping_params()), indent=2))
