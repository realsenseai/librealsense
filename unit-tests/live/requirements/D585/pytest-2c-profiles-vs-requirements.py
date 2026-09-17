# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
D585 2C (Dual-RGB, factory rs5x5_device) stream-profile conformance.

HAS D5X5 §6.5 (Table 19) requirement for the 2C SKU:
  Depth                  : Z16
  IR (Left+Right imager) : Y8I (L8R8, 16-bit interleaved)
  Color (Left imager)    : NV12 / YUY2
  Color (Right imager)   : NV12 / YUY2
  Calibration IR imager  : Y12I

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
    # rs5x5_device, 2C dual-RGB. "Dual RGB" substring matches both production
    # ("D585 Dual RGB") and prototype ("D585 Proto Dual RGB"); excludes D585S / 3C.
    pytest.mark.device_each("Dual RGB"),
]

REQUIRED = set()
REQUIRED |= req.video_reqs(rs.stream.depth,    "Z16",       req.FAM_DEPTH)   # Depth
REQUIRED |= req.video_reqs(rs.stream.infrared, "Y8I",       req.FAM_IR)      # IR L+R imager
REQUIRED |= req.video_reqs(rs.stream.color,    "NV12/YUY2", req.FAM_COLOR)   # Color L+R imager
REQUIRED |= req.calib_reqs(include_selfcal=False)  # Calib Y12I; no 256x144 self-cal in 2C mode

# Dual-RGB exposes a color stream from each imager (left + right).
EXPECTED_COLOR_STREAMS = 2


def test_d585_2c_profiles_match_requirements(module_device_setup):
    """Normal + nightly: enumerate FW profiles and compare to the requirement."""
    req.compare_and_assert(module_device_setup, REQUIRED, EXPECTED_COLOR_STREAMS)


@pytest.mark.context("weekly")
@pytest.mark.timeout(900)
def test_d585_2c_profiles_stream(module_device_setup):
    """Weekly only (~6 min): open every depth/IR/color profile and verify frames arrive."""
    req.stream_each_profile(module_device_setup)
