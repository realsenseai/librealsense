# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pyrealsense2 as rs

def is_dds_dev(dev):
    """Returns true if device is connected using DDS."""
    return dev and dev.supports(rs.camera_info.connection_type) and dev.get_info(rs.camera_info.connection_type) == "DDS"
