# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
D585 3C (dedicated color, factory rs5x5_dedicated_color_device) stream-profile conformance.

HAS D5X5 §6.5 (Table 19) requirement for the 3C SKU:
  Depth                  : Z16
  IR (Left+Right imager) : Y8I (L8R8, 16-bit interleaved)
  Color (3rd RGB camera) : NV12 / YUY2 / MJPEG   (D5xx-3C only, dedicated sensor)
  Calibration IR imager  : Y12I

Unlike the 2C SKU, the 3C variant has a single dedicated color sensor (no left/right
imager color streams).

Enumerates the FW-published profiles and compares them to the requirement (fails on any
missing capability). Streaming is covered by other tests.
See d585_profiles_helper.py for enumeration mode and format-family handling.

References: HAS D5X5_HAS_V0p65.docx §6.5 Table 19; RSDEV-9250 (D585 all-streams res/fps).
"""

import pytest
import pyrealsense2 as rs
import profiles_helper as req

pytestmark = [
    # rs5x5_dedicated_color_device, D585 3C only. "D585" matches every D585 variant; excluding
    # "D585S" (safety) and "Dual RGB" (2C) leaves the dedicated-color SKUs: "D585", "D585F",
    # "D585 Prototype".
    pytest.mark.device_each("D585"),
    pytest.mark.device_exclude("D585S"),
    pytest.mark.device_exclude("Dual RGB"),
]

REQUIRED = set()
REQUIRED |= req.video_reqs(rs.stream.depth,    "Z16",             req.DEPTH_FORMATS)             # Depth
REQUIRED |= req.video_reqs(rs.stream.infrared, "Y8I",             req.IR_FORMATS)                # IR L+R imager
REQUIRED |= req.video_reqs(rs.stream.color,    "NV12/YUY2/MJPEG", req.COLOR_FORMATS_WITH_MJPEG)  # 3rd RGB camera
REQUIRED |= req.calib_reqs()                                                                     # Calibration Y12I

# 3C has a single dedicated color sensor.
EXPECTED_COLOR_STREAMS = 1


def test_d585_3c_profiles_match_requirements(module_device_setup):
    """Enumerate FW profiles and compare to the requirement."""
    req.compare_and_assert(module_device_setup, REQUIRED, EXPECTED_COLOR_STREAMS)
