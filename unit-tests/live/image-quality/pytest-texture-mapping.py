# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Texture Mapping Test
# Verifies that depth-to-color alignment (texture mapping) works correctly.
# Streams aligned depth+color, then checks two points on a target in the lab:
#   - Center (cube): expected to be black and closer to camera
#   - Left edge (background): expected to be white and further from camera
# Validates both color accuracy (per-pixel) and depth difference between the two points.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import numpy as np
import cv2
import logging
from iq_helper import (find_roi_location, get_roi_from_frame, is_color_close,
                       get_median_depth_from_region, sample_bg_depth,
                       get_median_color_from_region, sample_bg_color,
                       make_depth_filter_chain, save_failure_snapshot,
                       SAMPLE_REGION_SIZE, BG_SAMPLE_POINTS, CUBE_CENTER, WIDTH, HEIGHT)

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.context("image-quality"),
    pytest.mark.device_each("D400*"),
    pytest.mark.device_exclude("D401"),
    pytest.mark.timeout(1500),
]

NUM_FRAMES = 100  # Number of frames to check
DEPTH_TOLERANCE = 90  # Acceptable deviation from expected depth in mm
FRAMES_PASS_THRESHOLD = 0.7  # Percentage of frames that needs to pass
DEBUG_MODE = False

EXPECTED_DEPTH_DIFF = 110  # Expected difference in mm between background and cube

# Expected colors for the two sampling points
EXPECTED_CUBE_COLOR = (35, 35, 35)  # blackish - center cube
EXPECTED_BG_COLOR = (150, 150, 150)  # whitish - background

# Cube sample is at image center. Bg sampling (both color and depth) uses
# the shared BG_SAMPLE_POINTS — 2 points on the left/right paper strips at
# the cube's vertical midline.
cube_x, cube_y = CUBE_CENTER


def draw_debug(depth_frame, color_roi, depth_cube, depth_bg, measured_diff):
    """
    Simple debug view: depth+color overlay with sampling points and depth values
    """
    colorizer = rs.colorizer()
    # INTER_NEAREST so the debug view reflects the exact pixels we sampled.
    depth_image = get_roi_from_frame(colorizer.colorize(depth_frame), interpolation=cv2.INTER_NEAREST)
    overlay = cv2.addWeighted(depth_image, 0.7, color_roi, 0.3, 0)

    half = SAMPLE_REGION_SIZE // 2

    # Cube (red) — color + depth sample
    cv2.circle(overlay, (cube_x, cube_y), 6, (0, 0, 255), -1)
    cv2.rectangle(overlay,
                  (cube_x - half, cube_y - half),
                  (cube_x + half, cube_y + half),
                  (0, 0, 255), 2)

    # Bg samples (green) — shared by color and depth
    for (bx, by) in BG_SAMPLE_POINTS:
        cv2.circle(overlay, (bx, by), 6, (0, 255, 0), -1)
        cv2.rectangle(overlay,
                      (bx - half, by - half),
                      (bx + half, by + half),
                      (0, 255, 0), 2)

    # Add labels for each point with their measured distance
    font = cv2.FONT_HERSHEY_SIMPLEX
    font_scale = 0.5
    thickness = 1
    cv2.putText(overlay, f"cube: {depth_cube:.2f}mm", (cube_x + 10, cube_y - 10),
                font, font_scale, (0, 0, 255), thickness, cv2.LINE_AA)
    cv2.putText(overlay, f"bg(med): {depth_bg:.2f}mm", (10, 50),
                font, font_scale, (255, 255, 0), thickness, cv2.LINE_AA)
    cv2.putText(overlay, f"diff: {measured_diff:.2f}mm (exp: {EXPECTED_DEPTH_DIFF:.2f}mm)", (10, 30),
                font, font_scale, (255, 255, 255), thickness, cv2.LINE_AA)

    # resize
    height = 600
    width = int(overlay.shape[1] * (height / overlay.shape[0]))
    return cv2.resize(overlay, (width, height))


def run_test(dev, ctx, depth_resolution, depth_fps, color_resolution, color_fps):
    log.info(f"Texture Mapping Test: "
             f"Depth: {depth_resolution[0]}x{depth_resolution[1]} @ {depth_fps}fps | "
             f"Color: {color_resolution[0]}x{color_resolution[1]} @ {color_fps}fps")
    pipeline = None
    pipeline_profile = None
    last_color_roi = None
    last_depth_frame = None
    last_depth_cube = 0
    last_depth_bg = 0
    last_measured_diff = 0
    try:
        pipeline = rs.pipeline(ctx)
        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth, depth_resolution[0], depth_resolution[1], rs.format.z16, depth_fps)
        cfg.enable_stream(rs.stream.color, color_resolution[0], color_resolution[1], rs.format.bgr8, color_fps)
        if not cfg.can_resolve(pipeline):
            log.info(f"Config not supported! Depth: {depth_resolution[0]}x{depth_resolution[1]}@{depth_fps}fps, "
                     f"Color: {color_resolution[0]}x{color_resolution[1]}@{color_fps}fps")
            return

        depth_sensor = dev.first_depth_sensor()
        depth_sensor.set_option(rs.option.exposure, 10000) # on auto exposure we see more failures on sampling

        pipeline_profile = pipeline.start(cfg)

        depth_filters = make_depth_filter_chain()

        depth_stream = pipeline_profile.get_stream(rs.stream.depth)
        color_stream = pipeline_profile.get_stream(rs.stream.color)
        depth_to_color_extrinsics = depth_stream.get_extrinsics_to(color_stream)
        if (not np.any(np.array(depth_to_color_extrinsics.rotation))
                and not np.any(np.array(depth_to_color_extrinsics.translation))):
            check.fail("Extrinsics between depth and color streams are all zeros, aligned stream will show blank frames, failing test")
            return

        for i in range(60):  # skip initial frames
            pipeline.wait_for_frames()

        align = rs.align(rs.stream.color)

        # Track passes for color and depth difference
        cube_color_passes = 0
        bg_color_passes = 0
        depth_diff_passes = 0

        # find region of interest (page) and get the transformation matrix
        find_roi_location(pipeline, (4, 5, 6, 7), DEBUG_MODE)  # markers in the lab are 4,5,6,7

        for i in range(NUM_FRAMES):
            frames = pipeline.wait_for_frames()
            aligned_frames = align.process(frames)

            depth_frame = aligned_frames.get_depth_frame()
            color_frame = aligned_frames.get_color_frame()

            if not depth_frame or not color_frame:
                # if color is missing, skip
                log.debug(f"Frame {i}: Missing depth or color frame, skipping")
                continue

            # Filters are applied after rs.align for simplicity — the canonical
            # order is filter -> align, but that requires rebuilding the frameset.
            # Filtering aligned depth is a pragmatic compromise; acceptable here
            # since we only read region medians, not per-pixel geometry.
            depth_frame = depth_filters(depth_frame)

            color_frame_roi = get_roi_from_frame(color_frame)
            # Nearest-neighbor on depth — linear interpolation blends values
            # across cube/paper discontinuities.
            depth_frame_roi = get_roi_from_frame(depth_frame, interpolation=cv2.INTER_NEAREST)

            last_color_roi = color_frame_roi.copy()
            last_depth_frame = depth_frame

            # Check cube color (center - should be black) — region median instead
            # of a single pixel so one noisy pixel can't flake the test.
            cube_pixel = get_median_color_from_region(color_frame_roi, cube_x, cube_y)
            if is_color_close(cube_pixel, EXPECTED_CUBE_COLOR):
                cube_color_passes += 1
            else:
                log.debug(f"Frame {i} - Cube color at ({cube_x},{cube_y}) sampled: {cube_pixel} too far from expected {EXPECTED_CUBE_COLOR}")

            # Check background color — median of 2 regions on the left/right
            # paper strips at the cube's vertical midline (same BG_SAMPLE_POINTS
            # used for depth).
            bg_pixel, bg_color_readings = sample_bg_color(color_frame_roi, BG_SAMPLE_POINTS)
            if is_color_close(bg_pixel, EXPECTED_BG_COLOR):
                bg_color_passes += 1
            else:
                log.debug(f"Frame {i} - Background color sampled: {bg_pixel} too far from expected {EXPECTED_BG_COLOR} "
                          f"(per-region: {bg_color_readings})")

            # Cube depth: single region median at center.
            # Bg depth: median across BG_SAMPLE_POINTS.
            raw_cube = get_median_depth_from_region(depth_frame_roi, cube_x, cube_y)
            raw_bg, bg_readings = sample_bg_depth(depth_frame_roi, BG_SAMPLE_POINTS)

            if not raw_bg or not raw_cube:
                continue

            depth_cube = raw_cube  # in mm
            depth_bg = raw_bg  # in mm
            measured_diff = depth_bg - depth_cube  # background should be further than cube

            last_depth_cube = depth_cube
            last_depth_bg = depth_bg
            last_measured_diff = measured_diff

            if abs(measured_diff - EXPECTED_DEPTH_DIFF) <= DEPTH_TOLERANCE:
                depth_diff_passes += 1
            else:
                log.debug(f"Frame {i} - Depth diff: {measured_diff:.2f}mm too far from "
                          f"{EXPECTED_DEPTH_DIFF:.2f}mm (cube: {depth_cube:.2f}mm, bg: {depth_bg:.2f}mm, "
                          f"bg_samples: {[f'{v:.1f}' for v in bg_readings]})")

            if DEBUG_MODE:
                dbg = draw_debug(depth_frame, color_frame_roi, depth_cube, depth_bg, measured_diff)
                cv2.imshow('Overlay', dbg)
                cv2.waitKey(1)

        # if DEBUG_MODE:
        #     cv2.waitKey(0)

        min_passes = int(NUM_FRAMES * FRAMES_PASS_THRESHOLD)

        log.info("\n--- Color Results ---")
        log.info(f"Cube color passed in {cube_color_passes}/{NUM_FRAMES} frames")
        cube_ok = check.is_true(cube_color_passes >= min_passes, "Cube color failed in too many frames")

        log.info(f"Background color passed in {bg_color_passes}/{NUM_FRAMES} frames")
        bg_ok = check.is_true(bg_color_passes >= min_passes, "Background color failed in too many frames")

        log.info("\n--- Depth Results ---")
        log.info(f"Depth difference passed in {depth_diff_passes}/{NUM_FRAMES} frames")
        depth_ok = check.is_true(depth_diff_passes >= min_passes, "Depth difference failed in too many frames")

        if (not (cube_ok and bg_ok and depth_ok)) and last_color_roi is not None and last_depth_frame:
            save_failure_snapshot(__file__, pipeline,
                                 draw_debug(last_depth_frame, last_color_roi,
                                            last_depth_cube, last_depth_bg, last_measured_diff))

    except Exception as e:
        save_failure_snapshot(__file__, pipeline)
        log.exception("Unexpected exception")
        check.fail(f"Unexpected exception: {e}")
    finally:
        cv2.destroyAllWindows()
        if pipeline_profile:
            pipeline.stop()


def test_texture_mapping(test_device, test_context_var):
    dev, ctx = test_device

    configurations = [((1280, 720), 30)]
    # on nightly we check additional arbitrary configurations
    if "nightly" in test_context_var or "weekly" in test_context_var:
        configurations += [
            ((640, 480), 15),
            ((640, 480), 30),
            ((640, 480), 60),
            ((848, 480), 15),
            ((848, 480), 30),
            ((848, 480), 60),
            ((1280, 720), 5),
            ((1280, 720), 10),
            ((1280, 720), 15),
        ]

    for (depth_resolution, depth_fps) in configurations:
        for (color_resolution, color_fps) in configurations:
            if "weekly" not in test_context_var:
                # in nightly we test only matching resolutions and fps
                if depth_resolution != color_resolution or depth_fps != color_fps:
                    continue

            run_test(dev, ctx, depth_resolution, depth_fps, color_resolution, color_fps)

