# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test depth frame quality. Streams depth, checks that fill rate is above threshold
(>50% non-zero pixels with laser ON), across multiple resolutions.
"""

import pytest
import pyrealsense2 as rs
from pytest_check import check
import numpy as np
import time
import logging
log = logging.getLogger(__name__)

# Defines how far in cm do pixels have to be, to be considered in a different distance
DETAIL_LEVEL = 5
BLACK_PIXEL_THRESHOLD = 0.5  # Fail if more than 50% pixels are zero
FRAMES_TO_CHECK = 30

pytestmark = [
    pytest.mark.context("image-quality"),
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.device_exclude("D401"),
]

PREFERRED_FPS = 30


def get_available_depth_resolutions(dev, preferred_fps=PREFERRED_FPS):
    """
    Query the device for every distinct depth (width, height) it actually advertises
    for z16, each paired with preferred_fps if that resolution supports it, otherwise
    its own highest available fps. Sorted by pixel count, highest first.
    """
    depth_sensor = dev.first_depth_sensor()
    fps_by_resolution = {}
    for profile in depth_sensor.get_stream_profiles():
        if profile.stream_type() != rs.stream.depth or profile.format() != rs.format.z16:
            continue
        vp = profile.as_video_stream_profile()
        fps_by_resolution.setdefault((vp.width(), vp.height()), set()).add(vp.fps())

    resolutions = [
        (res, preferred_fps if preferred_fps in fps_set else max(fps_set))
        for res, fps_set in fps_by_resolution.items()
    ]
    resolutions.sort(key=lambda item: item[0][0] * item[0][1], reverse=True)
    return resolutions


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


def run_test(dev, ctx, resolution, fps):
    product_name = dev.get_info(rs.camera_info.name)

    cfg = rs.config()
    # On hubless multi-device rigs (e.g. Jetson with D457 + D436) the context sees every
    # connected device; without enable_device(sn) the pipeline picks the first match.
    cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    cfg.enable_stream(rs.stream.depth, resolution[0], resolution[1], rs.format.z16, fps)

    pipeline = rs.pipeline(ctx)
    if not cfg.can_resolve(pipeline):
        log.info(f"Configuration {resolution[0]}x{resolution[1]} @ {fps}fps is not supported by the device")
        return

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

        log.info(f"Testing depth frame - laser ON - {resolution[0]}x{resolution[1]}@{fps}fps - {product_name}")

        has_depth = False
        for frame_num in range(FRAMES_TO_CHECK):
            has_depth, blank_pixels = is_depth_fill_rate_enough(pipeline)
            if has_depth:
                break
    finally:
        pipeline.stop()

    check.is_true(has_depth,
                  f"Depth fill rate too low on {product_name} at {resolution[0]}x{resolution[1]}@{fps}fps "
                  f"after {FRAMES_TO_CHECK} frames")


def test_depth_fill_rate(test_device_wrapped, test_context_var):
    dev, ctx = test_device_wrapped

    all_resolutions = get_available_depth_resolutions(dev)

    # Sweeping every resolution the device advertises is expensive (each waits for up
    # to FRAMES_TO_CHECK frames), so regular CI only runs the highest-resolution mode;
    # the full sweep runs under nightly. If nightly still proves too slow, change
    # "nightly" to "weekly" below to push the extended sweep out further.
    configurations = all_resolutions if "nightly" in test_context_var else all_resolutions[:1]

    for resolution, fps in configurations:
        run_test(dev, ctx, resolution, fps)
