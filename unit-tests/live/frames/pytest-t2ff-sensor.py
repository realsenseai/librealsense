# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test time-to-first-frame for sensor API. Measures startup time from
sensor.open()/start() to first frame via callback for depth and color streams.

Note: Using Windows Media Foundation to handle power management between USB actions
can add ~27ms to the startup time.
"""

import pytest
import pyrealsense2 as rs
from rspy.stopwatch import Stopwatch
import logging
log = logging.getLogger(__name__)
import time
import platform
from rspy.snippets import is_dds_dev

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
]


_device_settled = False

@pytest.fixture
def sensor_device(test_device):
    """Return (dev, ctx), waiting once for device to reach idle state."""
    global _device_settled
    dev, ctx = test_device
    if not _device_settled:
        time.sleep(3)  # device starts at D0 (Operational), wait for idle
        _device_settled = True
    return dev, ctx


def time_to_first_frame(sensor, profile, max_delay):
    """
    Measure time from sensor.open() to first frame arrival via callback.
    Returns the delay in seconds, or -1 if no frame arrived within max_delay + 1s.
    """
    first_frame_time = -1
    stopwatch = Stopwatch()

    def frame_cb(frame):
        nonlocal first_frame_time, stopwatch
        if first_frame_time == -1:
            first_frame_time = stopwatch.get_elapsed()

    stopwatch.reset()
    sensor.open(profile)
    sensor.start(frame_cb)

    while first_frame_time == -1 and stopwatch.get_elapsed() < max_delay + 1:
        time.sleep(0.05)

    sensor.stop()
    sensor.close()

    return first_frame_time


def test_device_creation_time(module_device_setup):
    os_name = platform.system()
    log.info(f"Testing device creation time on {os_name} OS")

    stopwatch = Stopwatch()
    ctx = rs.context({"dds": {"enabled": False}})
    devs = ctx.devices
    if len(devs) == 0:
        ctx = rs.context({"dds": {"enabled": True}})
        stopwatch.reset()
        devs = ctx.devices
    creation_time = stopwatch.get_elapsed()

    assert len(devs) > 0, "No devices found"
    dev = devs[0]

    max_time = 5 if is_dds_dev(dev) else 1

    log.info(f"Device creation time is: {creation_time:.3f} [sec] max allowed is: {max_time:.1f} [sec]")
    assert creation_time < max_time, \
        f"Device creation time {creation_time:.3f}s exceeds maximum {max_time:.1f}s"


def test_first_depth_frame_delay(sensor_device):
    dev, ctx = sensor_device
    product_name = dev.get_info(rs.camera_info.name)
    max_delay = 1
    os_name = platform.system()

    log.info(f"Testing first depth frame delay on {product_name} device - {os_name} OS")

    ds = dev.first_depth_sensor()
    dp = next(p for p in ds.profiles
              if p.fps() == 30
              and p.stream_type() == rs.stream.depth
              and p.format() == rs.format.z16
              and p.is_default())

    delay = time_to_first_frame(ds, dp, max_delay)

    assert delay != -1, f"Depth frames did not arrive for {max_delay + 1} second(s)"
    log.info(f"Time until first depth frame is: {delay:.3f} [sec] max allowed is: {max_delay:.1f} [sec]")
    assert delay < max_delay, \
        f"Depth frame delay {delay:.3f}s exceeds maximum {max_delay:.1f}s"

    # Allow some time to close the depth pipe completely, stream stops when DDS reader closure is detected by device
    if is_dds_dev(dev):
        time.sleep(1)


# D421/D401/D405 do not have a color sensor support.
@pytest.mark.device_exclude("D421")
@pytest.mark.device_exclude("D401")
@pytest.mark.device_exclude("D405")
def test_first_color_frame_delay(sensor_device):
    dev, ctx = sensor_device
    product_name = dev.get_info(rs.camera_info.name)
    max_delay = 1
    os_name = platform.system()

    log.info(f"Testing first color frame delay on {product_name} device - {os_name} OS")

    cs = dev.first_color_sensor()
    cp = next(p for p in cs.profiles
              if p.fps() == 30
              and p.stream_type() == rs.stream.color
              and p.format() == rs.format.rgb8
              and p.is_default())

    delay = time_to_first_frame(cs, cp, max_delay)

    assert delay != -1, f"Color frames did not arrive for {max_delay + 1} second(s)"
    log.info(f"Time until first color frame is: {delay:.3f} [sec] max allowed is: {max_delay:.1f} [sec]")
    assert delay < max_delay, \
        f"Color frame delay {delay:.3f}s exceeds maximum {max_delay:.1f}s"
