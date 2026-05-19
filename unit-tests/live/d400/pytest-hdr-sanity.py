# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import logging
log = logging.getLogger(__name__)

pytestmark = [pytest.mark.device("D400*")]


@pytest.fixture(autouse=True)
def _disable_hdr(test_device):
    yield
    dev, _ = test_device
    depth_sensor = dev.first_depth_sensor()
    if depth_sensor and depth_sensor.supports(rs.option.hdr_enabled):
        depth_sensor.set_option(rs.option.hdr_enabled, 0)


def test_hdr_streaming_custom_config(test_device):
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()

    if not (depth_sensor and depth_sensor.supports(rs.option.hdr_enabled)):
        pytest.skip("HDR not supported on this device")

    depth_sensor.set_option(rs.option.sequence_size, 2)
    check.is_true(depth_sensor.get_option(rs.option.sequence_size) == 2)
    first_exposure = 120
    first_gain = 90
    depth_sensor.set_option(rs.option.sequence_id, 1)
    check.is_true(depth_sensor.get_option(rs.option.sequence_id) == 1)
    depth_sensor.set_option(rs.option.exposure, first_exposure)
    check.is_true(depth_sensor.get_option(rs.option.exposure) == first_exposure)
    depth_sensor.set_option(rs.option.gain, first_gain)
    check.is_true(depth_sensor.get_option(rs.option.gain) == first_gain)

    second_exposure = 1200
    second_gain = 20
    depth_sensor.set_option(rs.option.sequence_id, 2)
    check.is_true(depth_sensor.get_option(rs.option.sequence_id) == 2)
    depth_sensor.set_option(rs.option.exposure, second_exposure)
    check.is_true(depth_sensor.get_option(rs.option.exposure) == second_exposure)
    depth_sensor.set_option(rs.option.gain, second_gain)
    check.is_true(depth_sensor.get_option(rs.option.gain) == second_gain)

    depth_sensor.set_option(rs.option.hdr_enabled, 1)
    check.is_true(depth_sensor.get_option(rs.option.hdr_enabled) == 1)

    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth)
    cfg.enable_stream(rs.stream.infrared, 1)
    pipe = rs.pipeline(ctx)
    pipe.start(cfg)
    for iteration in range(1, 100):
        data = pipe.wait_for_frames()

        out_depth_frame = data.get_depth_frame()
        if iteration < 3:
            continue

        if out_depth_frame.supports_frame_metadata(rs.frame_metadata_value.sequence_id):
            frame_exposure = out_depth_frame.get_frame_metadata(rs.frame_metadata_value.actual_exposure)
            frame_gain = out_depth_frame.get_frame_metadata(rs.frame_metadata_value.gain_level)
            seq_id = out_depth_frame.get_frame_metadata(rs.frame_metadata_value.sequence_id)

            if seq_id == 0:
                check.is_true(frame_exposure == first_exposure)
                check.is_true(frame_gain == first_gain)
            else:
                check.is_true(frame_exposure == second_exposure)
                check.is_true(frame_gain == second_gain)
    pipe.stop()
