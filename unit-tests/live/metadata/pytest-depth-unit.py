# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D400*"),
    pytest.mark.device("D500*"),
    pytest.mark.device_exclude("D555"),
]


def test_depth_units_metadata(test_device):
    """Get metadata depth units value and make sure it's non zero and equal to the depth sensor matching option value"""
    dev, ctx = test_device

    pipeline = rs.pipeline(ctx)
    cfg = rs.config()

    try:
        pipeline_profile = pipeline.start(cfg)
        frame_set = pipeline.wait_for_frames()
        depth_frame = frame_set.get_depth_frame()
        depth_units_from_metadata = depth_frame.get_units()
        check.is_true(depth_units_from_metadata > 0, "Depth units from metadata should be non-zero")

        dev = pipeline_profile.get_device()
        ds = dev.first_depth_sensor()
        check.is_true(ds.supports(rs.option.depth_units), "Depth sensor should support depth_units option")
        check.equal(ds.get_option(rs.option.depth_units), depth_units_from_metadata,
            f"Depth units option ({ds.get_option(rs.option.depth_units)}) should match metadata ({depth_units_from_metadata})")
    except Exception as e:
        log.error(f"test failed: {e}")
        raise
    finally:
        pipeline.stop()
