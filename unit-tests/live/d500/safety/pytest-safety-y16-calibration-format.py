# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# This test checks streaming y16 profile

import pytest
import time
import pyrealsense2 as rs
from rspy.timer import Timer
from rspy import tests_wrapper as tw
import logging
log = logging.getLogger(__name__)

pytestmark = [pytest.mark.device_each("D585S")]


def close_resources(sensor):
    """
    Stop and Close sensor.
    :sensor: sensor of device
    """
    if len(sensor.get_active_streams()) > 0:
        log.debug("Close_resources: Stopping active streams")
        sensor.stop()
        sensor.close()


@pytest.fixture(autouse=True)
def _start_stop_wrapper(test_device):
    dev, _ = test_device
    tw.start_wrapper(dev)
    yield
    tw.stop_wrapper(dev)


def test_y16_streaming(test_device):
    state = {"y16_streamed": False}

    def frame_callback(frame):
        frame_profile = frame.get_profile()
        if frame_profile.format() == rs.format.y16:
            state["y16_streamed"] = True

    timer = Timer(5)

    device, _ = test_device
    depth_sensor = device.first_depth_sensor()

    profile_y16 = next(p for p in depth_sensor.profiles if p.format() == rs.format.y16)
    assert profile_y16
    log.debug(str(profile_y16))

    depth_sensor.open(profile_y16)
    depth_sensor.start(frame_callback)

    timer.start()
    while not timer.has_expired():
        if state["y16_streamed"]:
            break
        time.sleep(0.1)

    assert not timer.has_expired()
    assert state["y16_streamed"]
    close_resources(depth_sensor)
