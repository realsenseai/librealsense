# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D555"),
    pytest.mark.device_each("D585"),
    pytest.mark.device_each("D535"),
]


def test_emitter_on_off_set_get(test_device):
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()

    if not depth_sensor.supports(rs.option.emitter_on_off):
        pytest.skip("Device does not support emitter_on_off option")

    emitter_mode = rs.frame_metadata_value.frame_emitter_mode
    # emitter on/off is a per-frame streaming control; toggle it while streaming
    pipe = rs.pipeline(ctx)
    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth)
    pipe.start(cfg)
    try:
        time.sleep(2)
        pipe.wait_for_frames()

        # ON: the emitter alternates on/off per frame
        depth_sensor.set_option(rs.option.emitter_on_off, 1)
        check.equal(depth_sensor.get_option(rs.option.emitter_on_off), 1.0)
        modes = []
        for _ in range(16):
            depth = pipe.wait_for_frames().get_depth_frame()
            if depth.supports_frame_metadata(emitter_mode):
                modes.append(depth.get_frame_metadata(emitter_mode))
        if len(modes) >= 8:
            # check rate of alterations between laser on and off
            transitions = sum(1 for a, b in zip(modes, modes[1:]) if a != b)
            rate = transitions / (len(modes) - 1)
            check.is_true(rate > 0.6, f"emitter should alternate most frames, got rate {rate:.2f}: {modes}")

        depth_sensor.set_option(rs.option.emitter_on_off, 0)
        check.equal(depth_sensor.get_option(rs.option.emitter_on_off), 0.0)
    finally:
        pipe.stop()
