# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test time-to-first-frame for pipeline API. Measures startup time from
pipeline.start() to first frame for depth and color streams.

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

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
]


_device_settled = False

@pytest.fixture
def pipeline_device(test_device):
    """Return (dev, ctx), waiting once for device to reach idle state."""
    global _device_settled
    dev, ctx = test_device
    if not _device_settled:
        time.sleep(3)  # device starts at D0 (Operational), wait for idle
        _device_settled = True
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


def test_pipeline_first_depth_frame_delay(pipeline_device):
    dev, ctx = pipeline_device
    product_name = dev.get_info(rs.camera_info.name)
    max_delay = 1
    os_name = platform.system()

    log.info(f"Testing pipeline first depth frame delay on {product_name} device - {os_name} OS")

    depth_cfg = rs.config()
    depth_cfg.enable_stream(rs.stream.depth, rs.format.z16, 30)

    frame_delay = time_to_first_frame(ctx, depth_cfg)

    log.info(f"Delay from pipeline.start() until first depth frame is: {frame_delay:.3f} [sec] "
          f"max allowed is: {max_delay:.1f} [sec]")

    assert frame_delay < max_delay, \
        f"Depth frame delay {frame_delay:.3f}s exceeds maximum {max_delay:.1f}s"


def test_pipeline_first_color_frame_delay(pipeline_device):
    dev, ctx = pipeline_device
    product_name = dev.get_info(rs.camera_info.name)
    max_delay = 1
    os_name = platform.system()

    if any(model in product_name for model in ['D421', 'D405', 'D430', 'D401']):
        pytest.skip(f"Device {product_name} has no color sensor")

    # Allow HKR some time to close the depth pipe completely (runs after depth test)
    if 'D555' in product_name:
        time.sleep(1)

    log.info(f"Testing pipeline first color frame delay on {product_name} device - {os_name} OS")

    color_cfg = rs.config()
    color_cfg.enable_stream(rs.stream.color, rs.format.rgb8, 30)

    frame_delay = time_to_first_frame(ctx, color_cfg)

    log.info(f"Delay from pipeline.start() until first color frame is: {frame_delay:.3f} [sec] "
          f"max allowed is: {max_delay:.1f} [sec]")

    assert frame_delay < max_delay, \
        f"Color frame delay {frame_delay:.3f}s exceeds maximum {max_delay:.1f}s"
