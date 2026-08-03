# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Verifies the laser projector is actually emitting a structured-light dot pattern.

Captures an averaged IR image with the emitter OFF and another with it ON (same
static scene, fixed exposure), then diffs them directly: the laser only adds
light where its dots land, so real dots show up in the difference image as many
small, bright, scattered blobs -- the same thing a human would see comparing the
two frames side by side. A uniform brightness shift (e.g. ambient light changing
between captures) would instead show up as one large blob, which is filtered out.
"""

import pytest
import pyrealsense2 as rs
import numpy as np
import cv2
import time
import logging
from iq_helper import save_failure_snapshot

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.context("image-quality"),
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.timeout(120),
]

NUM_FRAMES = 15  # frames averaged per measurement, to average out sensor read noise
SETTLE_FRAMES_TO_DISCARD = 5  # frames dropped after toggling the emitter, to let the new state take effect
MIN_DOT_AREA_PX = 1  # smallest connected component counted as a dot
MAX_DOT_AREA_PX = 50  # above this, treat it as a brightness blob, not a laser dot
MIN_DOT_COUNT = 30  # need at least this many dot-sized blobs in the diff image
GRID_SIZE = 4  # frame divided into GRID_SIZE x GRID_SIZE cells to check spread
MIN_GRID_CELLS_COVERED = 6  # dots must be spread across at least this many cells (of GRID_SIZE**2)
MIN_QUADRANTS_COVERED = 3  # covered cells must span at least this many of the 4 image quadrants,
                           # so the dots aren't all clustered in one corner
EXPOSURE_FRACTION = 6.0  # manual exposure = 1/EXPOSURE_FRACTION of frame time, short enough to avoid saturation


def capture_avg_ir(pipeline):
    """Discard a few frames to let the emitter state settle, then return the pixel-wise mean IR image."""
    for _ in range(SETTLE_FRAMES_TO_DISCARD):
        pipeline.wait_for_frames()

    frames = []
    for _ in range(NUM_FRAMES):
        ir_frame = pipeline.wait_for_frames().get_infrared_frame(1)
        if ir_frame:
            frames.append(np.asanyarray(ir_frame.get_data()).astype(np.float32))

    if not frames:
        pytest.fail("No IR frames captured — pipeline returned no valid infrared frames")
    return np.mean(frames, axis=0)


def find_dot_blobs(diff_image):
    """
    Threshold the ON-minus-OFF difference image (Otsu, like a human picking out
    "the bright bits") and return the dot-sized connected components plus the
    binary mask used, for debugging.
    """
    diff_u8 = cv2.normalize(np.clip(diff_image, 0, None), None, 0, 255, cv2.NORM_MINMAX, dtype=cv2.CV_8U)
    _, mask = cv2.threshold(diff_u8, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    _, _, stats, centroids = cv2.connectedComponentsWithStats(mask, connectivity=8)

    dots = [(centroids[i], stats[i, cv2.CC_STAT_AREA]) for i in range(1, len(stats))  # skip background label 0
            if MIN_DOT_AREA_PX <= stats[i, cv2.CC_STAT_AREA] <= MAX_DOT_AREA_PX]
    return dots, mask


def draw_debug(off_img, on_img, diff_mask, dots):
    off_bgr = cv2.cvtColor(cv2.normalize(off_img, None, 0, 255, cv2.NORM_MINMAX, dtype=cv2.CV_8U), cv2.COLOR_GRAY2BGR)
    on_bgr = cv2.cvtColor(cv2.normalize(on_img, None, 0, 255, cv2.NORM_MINMAX, dtype=cv2.CV_8U), cv2.COLOR_GRAY2BGR)
    mask_bgr = cv2.cvtColor(diff_mask, cv2.COLOR_GRAY2BGR)
    for (cx, cy), _ in dots:
        cv2.circle(mask_bgr, (int(cx), int(cy)), 4, (0, 0, 255), 1)
    cv2.putText(off_bgr, "OFF", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
    cv2.putText(on_bgr, "ON", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
    cv2.putText(mask_bgr, f"dots={len(dots)}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
    return np.hstack([off_bgr, on_bgr, mask_bgr])


def test_laser_pattern_visible(test_device_wrapped):
    dev, ctx = test_device_wrapped
    product_name = dev.get_info(rs.camera_info.name)
    pre_sensor = dev.first_depth_sensor()

    if not pre_sensor.supports(rs.option.emitter_enabled):
        pytest.skip(f"{product_name} does not support emitter_enabled")

    cfg = rs.config()
    # On hubless multi-device rigs (e.g. Jetson with D457 + D436) the context sees every
    # connected device; without enable_device(sn) the pipeline picks the first match.
    cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    cfg.enable_stream(rs.stream.infrared, 1, rs.format.y8, 30)

    pipeline = rs.pipeline(ctx)
    if not cfg.can_resolve(pipeline):
        pytest.skip(f"{product_name} does not support an IR y8 stream")

    pattern_visible = False
    dots = []
    covered_cells = set()
    covered_quadrants = set()
    profile = pipeline.start(cfg)
    
    try:
        sensor = profile.get_device().first_depth_sensor()
        if sensor.supports(rs.option.laser_power):
            sensor.set_option(rs.option.laser_power, sensor.get_option_range(rs.option.laser_power).max)
        if sensor.supports(rs.option.enable_auto_exposure):
            sensor.set_option(rs.option.enable_auto_exposure, 0)

        pipeline.wait_for_frames()
        time.sleep(1)  # let the stream stabilize before touching exposure/emitter

        if sensor.supports(rs.option.exposure):
            # Fix exposure so the OFF/ON images differ only by the laser's own light,
            # not by auto-exposure compensating for it -- otherwise the diff isn't a clean A/B.
            fps = profile.get_stream(rs.stream.infrared, 1).fps()
            sensor.set_option(rs.option.exposure, (1_000_000.0 / fps) / EXPOSURE_FRACTION)

        sensor.set_option(rs.option.emitter_enabled, 0)
        off_img = capture_avg_ir(pipeline)

        sensor.set_option(rs.option.emitter_enabled, 1)
        on_img = capture_avg_ir(pipeline)

        dots, mask = find_dot_blobs(on_img - off_img)
        h, w = mask.shape
        covered_cells = {
            (
                min(int(cx * GRID_SIZE / w), GRID_SIZE - 1),
                min(int(cy * GRID_SIZE / h), GRID_SIZE - 1)
            )
            for (cx, cy), _ in dots
        }

        # Map each covered cell to its image quadrant (2x2 blocks of the grid) to confirm
        # the dots aren't all clustered in one corner.
        covered_quadrants = {(col // (GRID_SIZE // 2), row // (GRID_SIZE // 2)) for col, row in covered_cells}

        log.info(f"{product_name}: {len(dots)} dot-sized blobs in ON-OFF diff, "
                 f"spread across {len(covered_cells)}/{GRID_SIZE * GRID_SIZE} grid cells "
                 f"in {len(covered_quadrants)}/4 quadrants")

        pattern_visible = (len(dots) >= MIN_DOT_COUNT
                           and len(covered_cells) >= MIN_GRID_CELLS_COVERED
                           and len(covered_quadrants) >= MIN_QUADRANTS_COVERED)

        if not pattern_visible:
            dbg = draw_debug(off_img, on_img, mask, dots)
            save_failure_snapshot(__file__, pipeline, dbg)
    finally:
        pipeline.stop()

    assert pattern_visible, (
        f"Laser dot pattern not detected on {product_name}: found {len(dots)} dot-sized blobs "
        f"(need >={MIN_DOT_COUNT}) across {len(covered_cells)} grid cells (need >={MIN_GRID_CELLS_COVERED}) "
        f"in {len(covered_quadrants)} quadrants (need >={MIN_QUADRANTS_COVERED})"
    )
