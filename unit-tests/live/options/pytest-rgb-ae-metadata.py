# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pyrealsense2 as rs
import pytest

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.context("weekly"),
]


def test_rgb_ae_metadata(test_device):
    dev, ctx = test_device
    sensor = next(s for s in dev.sensors
                  if any(p.stream_type() == rs.stream.color for p in s.profiles))

    cfg = rs.config()
    cfg.enable_stream(rs.stream.color)
    pipe = rs.pipeline(ctx)
    pipe.start(cfg)
    try:
        sensor.set_option(rs.option.enable_auto_exposure, True)
        for _ in range(5):
            pipe.wait_for_frames()
        frameset = pipe.wait_for_frames()
        frame = frameset.get_color_frame()
        assert frame.supports_frame_metadata(rs.frame_metadata_value.auto_exposure)
        assert frame.get_frame_metadata(rs.frame_metadata_value.auto_exposure) == 1

        sensor.set_option(rs.option.enable_auto_exposure, False)
        for _ in range(5):
            pipe.wait_for_frames()
        frameset = pipe.wait_for_frames()
        frame = frameset.get_color_frame()
        assert frame.supports_frame_metadata(rs.frame_metadata_value.auto_exposure)
        assert frame.get_frame_metadata(rs.frame_metadata_value.auto_exposure) == 0
    finally:
        pipe.stop()
