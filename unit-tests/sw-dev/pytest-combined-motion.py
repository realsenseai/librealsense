# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs


def test_combined_motion_orientation():
    motion = rs.combined_motion()
    orientation = (
        0.123456789012345,
        -0.234567890123456,
        0.345678901234567,
        0.987654321098765,
    )

    motion.orientation = orientation

    assert motion.orientation == orientation


def test_combined_motion_orientation_requires_four_values():
    motion = rs.combined_motion()

    with pytest.raises(ValueError, match="orientation must contain four values"):
        motion.orientation = (0.0, 0.0, 0.0)
