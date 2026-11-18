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
    if stream == rs.stream.labeled_point_cloud:
        cfg.enable_stream(stream, 2880, 1040, rs.format.y8, 30)
    else:
        cfg.enable_stream(stream)
    pipe = rs.pipeline(test_context)
    pipe.start(cfg)
    try:
        for _ in range(FRAMES_TO_COLLECT):
            assert pipe.wait_for_frames().first_or_default(stream)
    finally:
        pipe.stop()
