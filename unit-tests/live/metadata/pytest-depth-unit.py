# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import time
import pytest
import numpy as np
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy.pytest.device_helpers import require_min_fw_version
from pytest_check import check
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D400*"),
    pytest.mark.device_each("D500*"),
]


def require_dds_depth_units(dev):
    # Older DDS FW reports neither a depth-units option nor depth-units metadata
    if dev.get_info(rs.camera_info.connection_type) == "DDS":
        require_min_fw_version(dev, rsutils.version(7, 59, 46941, 17304), "DDS depth units")


def start_depth(dev, ctx):
    pipeline = rs.pipeline(ctx)
    cfg = rs.config()
    # On hubless multi-device rigs (e.g. Jetson with D457 + D436) the context sees every
    # connected device; without enable_device(sn) the pipeline picks the first match.
    cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    # Depth-only: the test validates depth units, and a default config would also wait on
    # color/IMU streams, failing the test if any unrelated stream doesn't deliver.
    cfg.enable_stream(rs.stream.depth)
    pipeline.start(cfg)
    return pipeline


def test_depth_units_metadata(test_device):
    """Get metadata depth units value and make sure it's non zero and equal to the depth sensor matching option value"""
    dev, ctx = test_device
    require_dds_depth_units(dev)

    pipeline = start_depth(dev, ctx)
    try:
        depth_units_from_metadata = pipeline.wait_for_frames().get_depth_frame().get_units()
        ds = pipeline.get_active_profile().get_device().first_depth_sensor()
    finally:
        pipeline.stop()
    check.is_true(depth_units_from_metadata > 0, "Depth units from metadata should be non-zero")

    assert ds.supports(rs.option.depth_units), "Depth sensor should support depth_units option"
    check.equal(ds.get_option(rs.option.depth_units), depth_units_from_metadata,
        f"Depth units option ({ds.get_option(rs.option.depth_units)}) should match metadata ({depth_units_from_metadata})")


@pytest.fixture
def restore_depth_table(test_device_wrapped):
    am = rs.rs400_advanced_mode(test_device_wrapped[0])
    original = am.get_depth_table()
    yield
    am.set_depth_table(original)


def test_depth_units_from_advanced_mode(test_device_wrapped, restore_depth_table):
    """Depth units changed through the advanced depth table while streaming must reach the frames and the depth sensor"""
    dev, ctx = test_device_wrapped
    require_dds_depth_units(dev)

    def median_distance():
        # Let the FW apply the change and older frames drain; the stream restarts with a few empty frames.
        # Saturated pixels follow the units, so they are not distances; a too-sparse scene gives None.
        for i in range(90):
            frame = pipeline.wait_for_frames().get_depth_frame()
            raw = np.asanyarray(frame.get_data())
            valid = raw[(raw > 0) & (raw < 0xFFFF)]
            if i >= 30 and valid.size >= raw.size // 20:
                return frame.get_units(), float(np.median(valid)) * frame.get_units()
        log.warning(f"only {valid.size} of {raw.size} depth pixels are valid, not checking distance")
        return frame.get_units(), None

    pipeline = start_depth(dev, ctx)
    try:
        # Write through the streaming device: each device handle keeps its own cached depth scale
        streaming_dev = pipeline.get_active_profile().get_device()
        am = rs.rs400_advanced_mode(streaming_dev)
        ds = streaming_dev.first_depth_sensor()
        _, baseline = median_distance()
        for units in (2000, 500):  # micrometers; 500 still covers 32m, so no far pixel saturates
            dt = am.get_depth_table()
            dt.depthUnits = units
            am.set_depth_table(dt)
            expected = units * 1e-6
            frame_units, distance = median_distance()
            # Over DDS the option value follows the frames by about a second
            deadline = time.time() + 3
            while ds.get_depth_scale() != pytest.approx(expected, rel=1e-3) and time.time() < deadline:
                time.sleep(0.1)
            check.almost_equal(frame_units, expected, rel=1e-3, msg=f"frame units after setting {units}um")
            check.almost_equal(ds.get_depth_scale(), expected, rel=1e-3, msg=f"depth scale after setting {units}um")
            if ds.supports(rs.option.depth_units):
                check.almost_equal(ds.get_option(rs.option.depth_units), expected, rel=1e-3)
            # The scene did not move, so the measured distance must not follow the units
            if baseline is not None and distance is not None:
                check.almost_equal(distance, baseline, rel=0.15, msg=f"median distance after setting {units}um")
    finally:
        pipeline.stop()
