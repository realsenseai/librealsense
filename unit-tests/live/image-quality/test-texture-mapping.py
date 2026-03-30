# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# test:device each(D400*) !D401
# test:timeout 1500

# Texture Mapping Test
# Verifies that depth-to-color alignment (texture mapping) works correctly.
# Streams aligned depth+color, then checks two points on a target in the lab:
#   - Center (cube): expected to be black and closer to camera
#   - Left edge (background): expected to be white and further from camera
# Validates both color accuracy (per-pixel) and depth difference between the two points.

import pyrealsense2 as rs
from rspy import log, test
import numpy as np
import cv2
from iq_helper import find_roi_location, get_roi_from_frame, is_color_close, get_avg_depth_from_region, save_failure_snapshot, SAMPLE_REGION_SIZE, WIDTH, HEIGHT

NUM_FRAMES = 100  # Number of frames to check
COLOR_TOLERANCE = 60  # Acceptable per-channel deviation in RGB values
DEPTH_TOLERANCE = 90  # Acceptable deviation from expected depth in mm
FRAMES_PASS_THRESHOLD = 0.7  # Percentage of frames that needs to pass
DEBUG_MODE = False

EXPECTED_DEPTH_DIFF = 110  # Expected difference in mm between background and cube

# Expected colors for the two sampling points
EXPECTED_CUBE_COLOR = (35, 35, 35)  # blackish - center cube
EXPECTED_BG_COLOR = (150, 150, 150)  # whitish - background

# Sample points - center of cube and left edge for background
cube_x, cube_y = WIDTH // 2, HEIGHT // 2
bg_x, bg_y = int(WIDTH * 0.1), HEIGHT // 2


def draw_debug(depth_frame, color_roi, depth_cube, depth_bg, measured_diff):
    """
    Simple debug view: depth+color overlay with sampling points and depth values
    """
    colorizer = rs.colorizer()
    depth_image = get_roi_from_frame(colorizer.colorize(depth_frame))
    overlay = cv2.addWeighted(depth_image, 0.7, color_roi, 0.3, 0)

    # Draw points for cube and background
    cv2.circle(overlay, (cube_x, cube_y), 6, (0, 0, 255), -1)  # Red for cube
    cv2.circle(overlay, (bg_x, bg_y), 6, (0, 255, 0), -1)  # Green for background

    # Draw sampled region rectangles
    half = SAMPLE_REGION_SIZE // 2
    cv2.rectangle(overlay,
                  (cube_x - half, cube_y - half),
                  (cube_x + half, cube_y + half),
                  (0, 0, 255), 2)
    cv2.rectangle(overlay,
                  (bg_x - half, bg_y - half),
                  (bg_x + half, bg_y + half),
                  (0, 255, 0), 2)

    # Add labels for each point with their measured distance
    font = cv2.FONT_HERSHEY_SIMPLEX
    font_scale = 0.5
    thickness = 1
    cv2.putText(overlay, f"cube: {depth_cube:.2f}mm", (cube_x + 10, cube_y - 10),
                font, font_scale, (0, 0, 255), thickness, cv2.LINE_AA)
    cv2.putText(overlay, f"bg: {depth_bg:.2f}mm", (bg_x + 10, bg_y - 10),
                font, font_scale, (0, 255, 0), thickness, cv2.LINE_AA)
    cv2.putText(overlay, f"diff: {measured_diff:.2f}mm (exp: {EXPECTED_DEPTH_DIFF:.2f}mm)", (10, 30),
                font, font_scale, (255, 255, 255), thickness, cv2.LINE_AA)

    # resize
    height = 600
    width = int(overlay.shape[1] * (height / overlay.shape[0]))
    return cv2.resize(overlay, (width, height))


dev, ctx = test.find_first_device_or_exit()


def run_test(depth_resolution, depth_fps, color_resolution, color_fps):
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
            log.i(f"Config not supported! Depth: {depth_resolution[0]}x{depth_resolution[1]}@{depth_fps}fps, "
                  f"Color: {color_resolution[0]}x{color_resolution[1]}@{color_fps}fps")
            return

        depth_sensor = dev.first_depth_sensor()
        depth_sensor.set_option(rs.option.exposure, 10000) # on auto exposure we see more failures on sampling

        pipeline_profile = pipeline.start(cfg)

        depth_stream = pipeline_profile.get_stream(rs.stream.depth)
        color_stream = pipeline_profile.get_stream(rs.stream.color)
        depth_to_color_extrinsics = depth_stream.get_extrinsics_to(color_stream)
        if not np.any(np.array(depth_to_color_extrinsics.rotation)) and not np.any(np.array(depth_to_color_extrinsics.translation)):
            log.f("Extrinsics between depth and color streams are all zeros, aligned stream will show blank frames, failing test")

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
                log.d(f"Frame {i}: Missing depth or color frame, skipping")
                continue

            color_frame_roi = get_roi_from_frame(color_frame)
            depth_frame_roi = get_roi_from_frame(depth_frame)

            last_color_roi = color_frame_roi.copy()
            last_depth_frame = depth_frame

            # Check cube color (center - should be black)
            cube_b, cube_g, cube_r = (int(v) for v in color_frame_roi[cube_y, cube_x])
            cube_pixel = (cube_r, cube_g, cube_b)
            if is_color_close(cube_pixel, EXPECTED_CUBE_COLOR, COLOR_TOLERANCE):
                cube_color_passes += 1
            else:
                log.d(f"Frame {i} - Cube color at ({cube_x},{cube_y}) sampled: {cube_pixel} too far from expected {EXPECTED_CUBE_COLOR}")

            # Check background color (left side - should be white)
            bg_b, bg_g, bg_r = (int(v) for v in color_frame_roi[bg_y, bg_x])
            bg_pixel = (bg_r, bg_g, bg_b)
            if is_color_close(bg_pixel, EXPECTED_BG_COLOR, COLOR_TOLERANCE):
                bg_color_passes += 1
            else:
                log.d(f"Frame {i} - Background color at ({bg_x},{bg_y}) sampled: {bg_pixel} too far from expected {EXPECTED_BG_COLOR}")

            # Sample depths using region averaging
            raw_cube = get_avg_depth_from_region(depth_frame_roi, cube_x, cube_y)
            raw_bg = get_avg_depth_from_region(depth_frame_roi, bg_x, bg_y)

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
                log.d(f"Frame {i} - Depth diff: {measured_diff:.2f}mm too far from "
                      f"{EXPECTED_DEPTH_DIFF:.2f}mm (cube: {depth_cube:.2f}mm, bg: {depth_bg:.2f}mm)")

            if DEBUG_MODE:
                dbg = draw_debug(depth_frame, color_frame_roi, depth_cube, depth_bg, measured_diff)
                cv2.imshow('Overlay', dbg)
                cv2.waitKey(1)

        # if DEBUG_MODE:
        #     cv2.waitKey(0)

        min_passes = int(NUM_FRAMES * FRAMES_PASS_THRESHOLD)

        log.i("\n--- Color Results ---")
        log.i(f"Cube color passed in {cube_color_passes}/{NUM_FRAMES} frames")
        test.check(cube_color_passes >= min_passes, "Cube color failed in too many frames")

        log.i(f"Background color passed in {bg_color_passes}/{NUM_FRAMES} frames")
        test.check(bg_color_passes >= min_passes, "Background color failed in too many frames")

        log.i("\n--- Depth Results ---")
        log.i(f"Depth difference passed in {depth_diff_passes}/{NUM_FRAMES} frames")
        test.check(depth_diff_passes >= min_passes, "Depth difference failed in too many frames")

        if test.test_failed and last_color_roi is not None and last_depth_frame:
            save_failure_snapshot(__file__, pipeline,
                                 draw_debug(last_depth_frame, last_color_roi,
                                            last_depth_cube, last_depth_bg, last_measured_diff))

    except Exception as e:
        save_failure_snapshot(__file__, pipeline)
        test.unexpected_exception()
    finally:
        cv2.destroyAllWindows()
        if pipeline_profile:
            pipeline.stop()

log.d("context:", test.context)

configurations = [((1280, 720), 30)]
# on nightly we check additional arbitrary configurations
if "nightly" in test.context or "weekly" in test.context:
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
        if "weekly" not in test.context:
            # in nightly we test only matching resolutions and fps
            if depth_resolution != color_resolution or depth_fps != color_fps:
                continue

        test.start("Texture Mapping Test",
                    f"Depth: {depth_resolution[0]}x{depth_resolution[1]} @ {depth_fps}fps | "
                    f"Color: {color_resolution[0]}x{color_resolution[1]} @ {color_fps}fps")
        run_test(depth_resolution, depth_fps, color_resolution, color_fps)
        test.finish()


test.print_results_and_exit()
