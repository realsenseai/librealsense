# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585"),  # matches D585 and D585S (prefix match)
    pytest.mark.context("nightly"),
]


# Constants
FRAMES_TO_COLLECT = 3


def _check_imu_streaming(test_context, stream, fps):
    cfg = rs.config()
    cfg.enable_stream(stream, 0, rs.format.motion_xyz32f, fps)
    pipe = rs.pipeline(test_context)
    pipe.start(cfg)
    iterations = 0
    while iterations < FRAMES_TO_COLLECT:
        iterations += 1
        f = pipe.wait_for_frames()
    assert iterations == FRAMES_TO_COLLECT
    pipe.stop()


@pytest.mark.parametrize("fps", [100, 200])
def test_accel_streaming(test_context, fps):
    _check_imu_streaming(test_context, rs.stream.accel, fps)


@pytest.mark.parametrize("fps", [200, 400])
def test_gyro_streaming(test_context, fps):
    _check_imu_streaming(test_context, rs.stream.gyro, fps)
