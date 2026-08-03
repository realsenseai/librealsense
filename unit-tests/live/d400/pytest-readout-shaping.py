# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
]


def test_readout_shaping_set_get(test_device_wrapped):
    # Readout shaping is applied while streaming, so stream first, set a small value,
    # verify it reads back, then return to 0.
    dev, _ = test_device_wrapped
    depth_sensor = dev.first_depth_sensor()
    if not depth_sensor.supports(rs.option.readout_shaping):
        pytest.skip("readout shaping not exposed on this device")

    r = depth_sensor.get_option_range(rs.option.readout_shaping)
    assert r.min == 0
    assert r.max == 100

    pipe = rs.pipeline()
    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth)
    def apply(value):
        # Set the value, stream a few frames so the FW applies it at a frame boundary, read it back.
        depth_sensor.set_option(rs.option.readout_shaping, value)
        for _ in range(5):
            pipe.wait_for_frames()
        return depth_sensor.get_option(rs.option.readout_shaping)

    pipe.start(cfg)
    try:
        assert apply(10) == 10     # small change
        assert apply(0) == 0       # return to 0
    finally:
        pipe.stop()


def test_readout_shaping_rejects_out_of_range(test_device_wrapped):
    # Values above the 0-100 range are rejected (no streaming needed).
    dev, _ = test_device_wrapped
    depth_sensor = dev.first_depth_sensor()
    if not depth_sensor.supports(rs.option.readout_shaping):
        pytest.skip("readout shaping not exposed on this device")

    for bad in (101, 200, 255):
        with pytest.raises(Exception):
            depth_sensor.set_option(rs.option.readout_shaping, bad)
