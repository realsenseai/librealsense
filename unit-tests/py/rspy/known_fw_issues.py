# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Tests that cannot pass on a specific device + FW because of a known FW issue."""

import pytest
import pyrealsense2 as rs

# The LibCI golden FW for the D585 Prototype. It is frozen, so it will not get a fix.
GOLDEN_FW_D585_PROTOTYPE = "7.58.46322.15891"


_NO_FRAMES = ("depth sometimes gives no frames on the first stream start; suspected FW issue (RSDEV-16272 family), "
              "fixed only in newer FW")


def skip_on_golden_d585_prototype(dev, reason=_NO_FRAMES):
    """Skip the running test when `dev` is a D585 Prototype with the golden FW."""
    if dev and dev.supports(rs.camera_info.name) and "D585 Prototype" in dev.get_info(rs.camera_info.name) \
            and dev.supports(rs.camera_info.firmware_version) \
            and dev.get_info(rs.camera_info.firmware_version) == GOLDEN_FW_D585_PROTOTYPE:
        pytest.skip(f"{reason} (D585 Prototype, golden FW {GOLDEN_FW_D585_PROTOTYPE})")
