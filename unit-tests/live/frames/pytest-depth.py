# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test depth frame quality. Streams depth, checks that fill rate is above threshold
(>50% non-zero pixels with laser ON).
"""

import pytest
import pyrealsense2 as rs
from rspy import tests_wrapper as tw
import numpy as np
import time
import logging
log = logging.getLogger(__name__)

# Defines how far in cm do pixels have to be, to be considered in a different distance
DETAIL_LEVEL = 5
BLACK_PIXEL_THRESHOLD = 0.5  # Fail if more than 50% pixels are zero
FRAMES_TO_CHECK = 30

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.device_exclude("D401"),
]


def get_distances(depth_frame):
    MAX_METERS = 10

    depth_m = np.asanyarray(depth_frame.get_data()).astype(np.float32) * depth_frame.get_units()
    valid_mask = (depth_m < MAX_METERS)
    valid_depths = depth_m[valid_mask]

    rounded_depths = (np.floor(valid_depths * 100.0 / DETAIL_LEVEL) * DETAIL_LEVEL).astype(np.int32)
    unique_vals, counts = np.unique(rounded_depths, return_counts=True)

    dists = dict(zip(unique_vals.tolist(), counts.tolist()))
    total = valid_depths.size

    log.debug(f"Distances detected in frame are: {dists}")
    return dists, total


def is_depth_fill_rate_enough(pipeline):
    """Check if depth fill rate is above threshold. Returns (ok, num_blank_pixels)."""
    frames = pipeline.wait_for_frames()
    depth = frames.get_depth_frame()
    assert depth, "Error getting depth frame"

    dists, total = get_distances(depth)
    num_blank_pixels = dists.get(0, 0)

    if num_blank_pixels > total * BLACK_PIXEL_THRESHOLD:
        percent_blank = 100.0 * num_blank_pixels / total if total > 0 else 0
        log.warning(f"Too many blank pixels: {num_blank_pixels}/{total} ({percent_blank:.1f}%)")
        return False, num_blank_pixels

    fill_rate = 100.0 * (total - num_blank_pixels) / total if total > 0 else 0
    log.info(f"Depth fill rate: {fill_rate:.1f}% (blank pixels: {num_blank_pixels}/{total})")
    return fill_rate > (1 - BLACK_PIXEL_THRESHOLD) * 100.0, num_blank_pixels


def test_depth_laser_on(test_device):
    dev, ctx = test_device
    product_name = dev.get_info(rs.camera_info.name)
    
    tw.start_wrapper(dev)
    try:
        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth, rs.format.z16, 30)

        pipeline = rs.pipeline(ctx)
        pipeline.start(cfg)
        try:
            pipeline.wait_for_frames()
            time.sleep(2)

            # Enable laser when supported; otherwise continue with current emitter state.
            sensor = pipeline.get_active_profile().get_device().first_depth_sensor()
            if sensor.supports(rs.option.laser_power):
                sensor.set_option(rs.option.laser_power, sensor.get_option_range(rs.option.laser_power).max)
            else:
                log.info(f"Device {product_name} does not support laser power; running depth fill test without forcing laser power")

            if sensor.supports(rs.option.emitter_enabled):
                sensor.set_option(rs.option.emitter_enabled, 1)
            else:
                log.info(f"Device {product_name} does not support emitter control; running with default emitter state")

            log.info(f"Testing depth frame - laser ON - {product_name}")

            has_depth = False
            for frame_num in range(FRAMES_TO_CHECK):
                has_depth, blank_pixels = is_depth_fill_rate_enough(pipeline)
                if has_depth:
                    break
        finally:
            pipeline.stop()

        assert has_depth, f"Depth fill rate too low on {product_name} after {FRAMES_TO_CHECK} frames"
    finally:
        tw.stop_wrapper(dev)
