# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585S"),
    pytest.mark.context("nightly"),
]


# Constants
FRAMES_TO_COLLECT = 3


@pytest.mark.parametrize("res", [(1280, 720), (640, 360)], ids=lambda r: f"{r[0]}x{r[1]}")
@pytest.mark.parametrize("fps", [5, 15, 30])
def test_depth_dpp_streaming(test_context, res, fps):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth, 0, res[0], res[1], rs.format.z16, fps)
    pipe = rs.pipeline(test_context)
    pipe.start(cfg)
    iterations = 0
    while iterations < FRAMES_TO_COLLECT:
        iterations += 1
        f = pipe.wait_for_frames()
    assert iterations == FRAMES_TO_COLLECT
    pipe.stop()
