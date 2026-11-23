# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test time-to-first-frame for pipeline API. Measures startup time from
pipeline.start() to first frame for depth and color streams,
on a cold start and on a restart right after stop.

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
from rspy.pytest.device_helpers import is_d585_on_gmsl

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
]


MAX_DELAY = 1.0  # [sec]
MAX_DELAY_D585_GMSL = 2.0  # [sec] first stream after a reset takes 1.1-1.6 s on D585 over GMSL; not on restart
MAX_DELAY_COLOR_FROM_DEPTH = 2.0  # [sec] D401/D405 color powers up the RGB ISP on start: ~0.4 s more than depth, 1.2-1.4 s on Jetson GMSL


def max_delay_for(dev, restart):
    return MAX_DELAY_D585_GMSL if is_d585_on_gmsl(dev) and not restart else MAX_DELAY


@pytest.fixture(scope="module")
def pipeline_device(test_device):
    """Return (dev, ctx), waiting once for device to reach idle state.

    Module-scoped so the settle-sleep fires once per module under normal runs
    AND re-fires after a --retries-triggered re-instantiation (pytest-retry
    tears down module fixtures between attempts, so the device may have been
    recycled and needs to settle again).
    """
    dev, ctx = test_device
    time.sleep(3)  # device starts at D0 (Operational), wait for idle
    return dev, ctx


def time_to_first_frame(ctx, config):
    """Measure time from pipeline.start() to first frame arrival."""
    pipe = rs.pipeline(ctx)
    start_call_stopwatch = Stopwatch()
    pipe.start(config)
    pipe.wait_for_frames()
    delay = start_call_stopwatch.get_elapsed()
    pipe.stop()
    return delay


def pipeline_first_depth_frame_delay(pipeline_device, restart):
    dev, ctx = pipeline_device
    product_name = dev.get_info(rs.camera_info.name)
    max_delay = max_delay_for(dev, restart)
    os_name = platform.system()

    log.info(f"Testing pipeline first depth frame delay on {product_name} device - {os_name} OS")

    depth_cfg = rs.config()
    # On hubless multi-device rigs (e.g. Jetson with D457 + D436) the context sees every
    # connected device; without enable_device(sn) the pipeline picks the first match.
    depth_cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    depth_cfg.enable_stream(rs.stream.depth, rs.format.z16, 30)

    if restart:  # unmeasured start/stop, then measure the restart right after it
        time_to_first_frame(ctx, depth_cfg)
        if is_dds_dev(dev):
            time.sleep(1)  # stream stops when DDS reader closure is detected by device

    frame_delay = time_to_first_frame(ctx, depth_cfg)

    log.info(f"Delay from pipeline.start() until first depth frame is: {frame_delay:.3f} [sec] "
          f"max allowed is: {max_delay:.1f} [sec]")

    assert frame_delay < max_delay, \
        f"Depth frame delay {frame_delay:.3f}s exceeds maximum {max_delay:.1f}s"

    # Allow some time to close the depth pipe completely, stream stops when DDS reader closure is detected by device
    if is_dds_dev(dev):
        time.sleep(1)


def test_pipeline_first_depth_frame_delay(pipeline_device):
    pipeline_first_depth_frame_delay(pipeline_device, restart=False)


def test_pipeline_restart_depth_frame_delay(pipeline_device):
    pipeline_first_depth_frame_delay(pipeline_device, restart=True)


def pipeline_first_color_frame_delay(pipeline_device, restart):
    dev, ctx = pipeline_device
    product_name = dev.get_info(rs.camera_info.name)
    max_delay = MAX_DELAY_COLOR_FROM_DEPTH if any(m in product_name for m in ('D401', 'D405')) else max_delay_for(dev, restart)
    os_name = platform.system()

    log.info(f"Testing pipeline first color frame delay on {product_name} device - {os_name} OS")

    color_cfg = rs.config()
    # On hubless multi-device rigs (e.g. Jetson with D457 + D436) the context sees every
    # connected device; without enable_device(sn) the pipeline picks the first match.
    color_cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    color_cfg.enable_stream(rs.stream.color, rs.format.rgb8, 30)

    if restart:  # unmeasured start/stop, then measure the restart right after it
        time_to_first_frame(ctx, color_cfg)
        if is_dds_dev(dev):
            time.sleep(1)  # stream stops when DDS reader closure is detected by device

    frame_delay = time_to_first_frame(ctx, color_cfg)

    log.info(f"Delay from pipeline.start() until first color frame is: {frame_delay:.3f} [sec] "
          f"max allowed is: {max_delay:.1f} [sec]")

    assert frame_delay < max_delay, \
        f"Color frame delay {frame_delay:.3f}s exceeds maximum {max_delay:.1f}s"


# D421/D430 have no color
@pytest.mark.device_exclude("D421")
@pytest.mark.device_exclude("D430")
def test_pipeline_first_color_frame_delay(pipeline_device):
    pipeline_first_color_frame_delay(pipeline_device, restart=False)


@pytest.mark.device_exclude("D421")
@pytest.mark.device_exclude("D430")
def test_pipeline_restart_color_frame_delay(pipeline_device):
    pipeline_first_color_frame_delay(pipeline_device, restart=True)
