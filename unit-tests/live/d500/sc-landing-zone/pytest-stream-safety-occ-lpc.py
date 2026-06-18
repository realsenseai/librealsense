# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [pytest.mark.device_each("D585S")]


# Constants
FRAMES_TO_COLLECT = 3


@pytest.mark.parametrize("stream", [rs.stream.safety, rs.stream.occupancy, rs.stream.labeled_point_cloud],
                         ids=lambda s: repr(s))
def test_sc_streaming(test_context, stream):
    cfg = rs.config()
    cfg.enable_stream(stream)
    pipe = rs.pipeline(test_context)
    pipe.start(cfg)
    iterations = 0
    while iterations < FRAMES_TO_COLLECT:
        iterations += 1
        f = pipe.wait_for_frames()
    assert iterations == FRAMES_TO_COLLECT
    pipe.stop()
