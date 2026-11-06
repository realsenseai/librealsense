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
missing capability). Streaming is covered by other tests.
See d585_profiles_helper.py for enumeration mode and format-family handling.

References: HAS D5X5_HAS_V0p65.docx §6.5 Table 19; RSDEV-9250 (D585 all-streams res/fps).
"""

import pytest
import pyrealsense2 as rs
import profiles_helper as req

pytestmark = [
    # rs5x5_device, D585 2C dual-RGB only. Names are matched explicitly so other Dual-RGB SKUs
    # (e.g. D535 Dual RGB) are never selected: "D585 Dual RGB" (production) and
    # "D585 Proto Dual RGB" (prototype) - the latter is a distinct, non-overlapping substring.
    pytest.mark.device_each("D585 Dual RGB"),
    pytest.mark.device_each("D585 Proto Dual RGB"),
]

REQUIRED = set()
REQUIRED |= req.video_reqs(rs.stream.depth,    "Z16",       req.DEPTH_FORMATS)   # Depth
REQUIRED |= req.video_reqs(rs.stream.infrared, "Y8I",       req.IR_FORMATS)      # IR L+R imager
REQUIRED |= req.video_reqs(rs.stream.color,    "NV12/YUY2", req.COLOR_FORMATS)   # Color L+R imager
REQUIRED |= req.calib_reqs(include_selfcal=False)  # Calib Y12I; no 256x144 self-cal in 2C mode

# Dual-RGB exposes a color stream from each imager (left + right).
EXPECTED_COLOR_STREAMS = 2


def test_d585_2c_profiles_match_requirements(module_device_setup):
    """Enumerate FW profiles and compare to the requirement."""
    req.compare_and_assert(module_device_setup, REQUIRED, EXPECTED_COLOR_STREAMS)
