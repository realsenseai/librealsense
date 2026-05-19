# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

# This test checks streaming y16 profile

import time
import pytest
import pyrealsense2 as rs
import logging
from rspy.timer import Timer
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D400*"),
    pytest.mark.device_each("D555"),
    pytest.mark.device_type_exclude("GMSL"), # Y16 is not streamed with metadata over GMSL causing all frames to drop
]


def test_y16_streaming(test_device):
    """Check that y16 is streaming."""
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()

    profile_y16 = next((p for p in depth_sensor.profiles if p.format() == rs.format.y16), None)
    assert profile_y16 is not None, "No Y16 profile found on depth sensor"
    log.debug(str(profile_y16))

    y16_streamed = False

    def frame_callback(frame):
        nonlocal y16_streamed
        y16_streamed = True

    depth_sensor.open(profile_y16)
    depth_sensor.start(frame_callback)
    try:
        timer = Timer(5)
        timer.start()
        while not timer.has_expired():
            if y16_streamed:
                break
            time.sleep(0.1)
        assert y16_streamed, "No Y16 frame received within 5 seconds"
    finally:
        if len(depth_sensor.get_active_streams()) > 0:
            depth_sensor.stop()
            depth_sensor.close()
