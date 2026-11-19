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
import pyrsutils as rsutils
from rspy.stopwatch import Stopwatch
import logging
from rspy.pytest.device_helpers import require_min_fw_version, is_d585_on_gmsl
log = logging.getLogger(__name__)
import time
import platform
from rspy.snippets import is_dds_dev
from pytest_check import check

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
]


@pytest.fixture(scope="module")
def sensor_device(test_device):
    """Return (dev, ctx), waiting once for device to reach idle state.

    Module-scoped so the settle-sleep fires once per module under normal runs
    AND re-fires after a --retries-triggered re-instantiation (pytest-retry
    tears down module fixtures between attempts, so the device may have been
    recycled and needs to settle again).
    """
    dev, ctx = test_device
    time.sleep(3)  # device starts at D0 (Operational), wait for idle
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
    max_delay = 1.5 if is_d585_on_gmsl(dev) else 1  # slow first start after reset on D585 GMSL
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


# D421/D430 have no color; D401/D405 stream color from the depth sensor.
@pytest.mark.device_exclude("D421")
@pytest.mark.device_exclude("D430")
def test_first_color_frame_delay(sensor_device):
    dev, ctx = sensor_device
    if "D585" in dev.get_info( rs.camera_info.name ) and dev.get_info( rs.camera_info.connection_type ) == "GMSL":
        require_min_fw_version( dev, rsutils.version( "7.62.0.0" ), "first color frame delay" )
    product_name = dev.get_info(rs.camera_info.name)
    color_from_depth = any(m in product_name for m in ('D401', 'D405'))
    max_delay = 2 if color_from_depth else 1  # D401/D405 color powers up the RGB ISP on start
    os_name = platform.system()

    log.info(f"Testing first color frame delay on {product_name} device - {os_name} OS")

    cs = dev.first_depth_sensor() if color_from_depth else dev.first_color_sensor()
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


@pytest.mark.context("nightly")
@pytest.mark.device_exclude("D500*")
@pytest.mark.device_exclude("D421")
@pytest.mark.device_exclude("D401")
@pytest.mark.device_exclude("D405")
def test_stop_to_start_depth_color_hd_delay(sensor_device):
    """Stream depth + color HD for 5s, stop, restart and measure time from stop until each first frame."""
    dev, ctx = sensor_device
    max_delay = 1  # KPI is 0.5s, measured up to 0.8s on D435/Windows

    sensors = [(s, next(p for p in s.profiles
                        if p.stream_type() == stream and p.format() == fmt and p.fps() == 30
                        and p.as_video_stream_profile().width() == 1280
                        and p.as_video_stream_profile().height() == 720))
               for s, stream, fmt in [(dev.first_depth_sensor(), rs.stream.depth, rs.format.z16),
                                      (dev.first_color_sensor(), rs.stream.color, rs.format.yuyv)]]

    def start(cb):
        for s, p in sensors:
            s.open(p)
            s.start(cb)

    def stop():
        for s, _ in sensors:
            s.stop()
            s.close()

    start(lambda f: None)
    time.sleep(5)
    stop()
    stopwatch = Stopwatch()
    first = {}
    start(lambda f: first.setdefault(f.get_profile().stream_type(), stopwatch.get_elapsed()))
    while len(first) < len(sensors) and stopwatch.get_elapsed() < max_delay + 1:
        time.sleep(0.05)
    stop()

    for _, p in sensors:
        stream = p.stream_type()
        check.is_in(stream, first, f"{stream} frames did not arrive for {max_delay + 1} second(s) after restart")
        if stream in first:
            log.info(f"Stop to first {stream} frame: {first[stream]:.3f} [sec] max allowed is: {max_delay:.1f} [sec]")
            check.less(first[stream], max_delay, f"{stream} stop-to-start delay exceeds maximum")
