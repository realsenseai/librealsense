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


@pytest.mark.parametrize("fps", [5, 15, 30])
def test_depth_and_ir_streaming(test_context, fps):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth, 0, 1280, 720, rs.format.z16, fps)
    cfg.enable_stream(rs.stream.infrared, 1, 1280, 720, rs.format.y8, fps)
    pipe = rs.pipeline(test_context)
    pipe.start(cfg)
    iterations = 0
    while iterations < FRAMES_TO_COLLECT:
        iterations += 1
        f = pipe.wait_for_frames()
    assert iterations == FRAMES_TO_COLLECT
    pipe.stop()
