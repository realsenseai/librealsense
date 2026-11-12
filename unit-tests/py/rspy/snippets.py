# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pyrealsense2 as rs

def is_dds_dev(dev):
    """Returns true if device is connected using DDS."""
    return dev and dev.supports(rs.camera_info.connection_type) and dev.get_info(rs.camera_info.connection_type) == "DDS"


# The D585 Prototype on this FW (the LibCI golden) is slower to deliver its first color frame: 1.1 - 1.25 s measured.
_GOLDEN_FW_D585_PROTOTYPE = "7.58.46322.15891"
_FIRST_COLOR_FRAME_MAX_DELAY = 1.0              # [sec]
_FIRST_COLOR_FRAME_MAX_DELAY_GOLDEN_D585_PROTOTYPE = 1.5

def first_color_frame_max_delay(dev):
    """Max allowed time [sec] from start to the first color frame. Longer for the D585 Prototype on its golden FW."""
    if "D585 Prototype" in dev.get_info(rs.camera_info.name) \
            and dev.get_info(rs.camera_info.firmware_version) == _GOLDEN_FW_D585_PROTOTYPE:
        return _FIRST_COLOR_FRAME_MAX_DELAY_GOLDEN_D585_PROTOTYPE
    return _FIRST_COLOR_FRAME_MAX_DELAY
