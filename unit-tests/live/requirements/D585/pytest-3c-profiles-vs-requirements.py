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

The enumeration test compares the FW-published profiles to the requirement (fails on any
missing capability); the streaming test (weekly) opens each profile and verifies frames.
See requirements_helper.py for enumeration mode and format-family handling.

References:
  HAS D5X5 V0p65, §6.5 Table 19:
    https://realsensecloud.sharepoint.com/:w:/r/sites/allcompany/_layouts/15/Doc.aspx?sourcedoc=%7B68C45603-CBDD-48E2-8A7F-F405018CF6DF%7D&file=D5X5_HAS_V0p65.docx
  RSDEV-9250 (D585 all-streams res/fps): https://rsjira.realsenseai.com/browse/RSDEV-9250
"""

import pytest
import pyrealsense2 as rs
import requirements_helper as req

pytestmark = [
    # rs5x5_dedicated_color_device, 3C. "D585" matches all D585 variants; excluding
    # "D585S" (safety) and "Dual RGB" (2C) leaves the 3C SKUs: "D585", "D585F",
    # "D585 Prototype".
    pytest.mark.device_each("D585"),
    pytest.mark.device_exclude("D585S"),
    pytest.mark.device_exclude("Dual RGB"),
]

REQUIRED = set()
REQUIRED |= req.video_reqs(rs.stream.depth,    "Z16",             req.FAM_DEPTH)             # Depth
REQUIRED |= req.video_reqs(rs.stream.infrared, "Y8I",             req.FAM_IR)                # IR L+R imager
REQUIRED |= req.video_reqs(rs.stream.color,    "NV12/YUY2/MJPEG", req.FAM_COLOR_COMPRESSED)  # 3rd RGB camera
REQUIRED |= req.calib_reqs()                                                                 # Calibration Y12I

# 3C has a single dedicated color sensor.
EXPECTED_COLOR_STREAMS = 1


def test_d585_3c_profiles_match_requirements(module_device_setup):
    """Normal + nightly: enumerate FW profiles and compare to the requirement."""
    req.compare_and_assert(module_device_setup, REQUIRED, EXPECTED_COLOR_STREAMS)


@pytest.mark.context("weekly")
@pytest.mark.timeout(900)
def test_d585_3c_profiles_stream(module_device_setup):
    """Weekly only (~6 min): open every depth/IR/color profile and verify frames arrive."""
    req.stream_each_profile(module_device_setup)
