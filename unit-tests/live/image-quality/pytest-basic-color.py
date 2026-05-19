# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import numpy as np
import cv2
import logging
from iq_helper import find_roi_location, get_roi_from_frame, is_color_close, save_failure_snapshot, WIDTH, HEIGHT

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.context("image-quality"),
    pytest.mark.device_each("D400*"),
    pytest.mark.device_exclude("D401"),
]

NUM_FRAMES = 100 # Number of frames to check
FRAMES_PASS_THRESHOLD =0.8 # Percentage of frames that needs to pass
DEBUG_MODE = False

# expected colors (insertion order -> mapped row-major to 3x3 grid)
expected_colors = {
    # All expected values are empirical means across 10 camera instances
    "red":   (175, 77, 82),
    "green": (64, 123, 95),
    "blue":  (26, 104, 146),
    "black": (50, 50, 46),
    "white": (183, 184, 186),
    "gray":  (121, 129, 130),
    "purple": (69, 77, 109),
    "orange": (182, 94, 83),
    "yellow": (199, 178, 85),
}
# list of color names in insertion order -> used left->right, top->bottom
color_names = list(expected_colors.keys())

# we are given a 3x3 grid, we split it using 2 vertical and 2 horizontal separators
# we also calculate the center of each grid cell for sampling from it for the test
xs = [1.5 * WIDTH / 6.0, WIDTH / 2.0, 4.5 * WIDTH / 6.0]
ys = [1.5 * HEIGHT / 6.0, HEIGHT / 2.0, 4.5 * HEIGHT / 6.0]
centers = [(x, y) for y in ys for x in xs]


def draw_debug(frame_bgr, a4_page_bgr):
    """
    Simple debug view:
      - left: camera frame
      - right: focused view on the A4 page with grid and color names
    """
    vertical_lines = [WIDTH / 3.0, 2.0 * WIDTH / 3.0]
    horizontal_lines = [HEIGHT / 3.0, 2.0 * HEIGHT / 3.0]
    H, W = a4_page_bgr.shape[:2]

    # draw grid on a4 page image
    for x in vertical_lines:
        cv2.line(a4_page_bgr, (int(x), 0), (int(x), H - 1), (255, 255, 255), 2)
    for y in horizontal_lines:
        cv2.line(a4_page_bgr, (0, int(y)), (W - 1, int(y)), (255, 255, 255), 2)

    # label centers with color names
    for i, (cx, cy) in enumerate(centers):
        cx_i, cy_i = int(round(cx)), int(round(cy))
        lbl = color_names[i] if i < len(color_names) else str(i)
        # marker in the expected color for visual reference
        expected_rgb = expected_colors.get(lbl, (255, 255, 255))
        cv2.circle(a4_page_bgr, (cx_i, cy_i), 10, expected_rgb[::-1], -1)  # RGB -> BGR
        cv2.putText(a4_page_bgr, lbl, (cx_i + 12, cy_i + 6),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0,0,0), 2)

    # resize and display side by side
    height = 600
    frame_width = int(frame_bgr.shape[1] * (height / frame_bgr.shape[0]))
    a4_page_width = int(a4_page_bgr.shape[1] * (height / a4_page_bgr.shape[0]))
    left = cv2.resize(frame_bgr, (frame_width, height))
    right = cv2.resize(a4_page_bgr, (a4_page_width, height))
    return np.hstack([left, right])


def run_test(ctx, resolution, fps):
    log.info(f"Basic Color Image Quality Test: {resolution[0]}x{resolution[1]} @ {fps}fps")
    color_match_count = {color: 0 for color in expected_colors.keys()}
    color_sums = {color: np.zeros(3, dtype=int) for color in expected_colors.keys()}
    pipeline = rs.pipeline(ctx)
    cfg = rs.config()
    cfg.enable_stream(rs.stream.color, resolution[0], resolution[1], rs.format.bgr8, fps)
    if not cfg.can_resolve(pipeline):
        log.info(f"Configuration {resolution[0]}x{resolution[1]}@{fps}fps is not supported by the device")
        return
    pipeline_profile = pipeline.start(cfg)
    for i in range(60):  # skip initial frames
        pipeline.wait_for_frames()
    last_frame_bgr = None
    last_roi = None
    try:
        # find region of interest (page) and get the transformation matrix
        find_roi_location(pipeline, (0, 1, 2, 3), DEBUG_MODE) # markers in the lab are 0,1,2,3

        # sampling loop
        for i in range(NUM_FRAMES):
            frames = pipeline.wait_for_frames()
            color_frame = frames.get_color_frame()
            img_bgr = np.asanyarray(color_frame.get_data())

            color_frame_roi = get_roi_from_frame(color_frame)

            last_frame_bgr = img_bgr.copy()
            last_roi = color_frame_roi.copy()

            # sample each grid center and compare to expected color by row-major insertion order
            for idx, (x, y) in enumerate(centers):
                color = color_names[idx] if idx < len(color_names) else str(idx)
                expected_rgb = expected_colors[color]
                x = int(round(x))
                y = int(round(y))
                b, g, r = (int(v) for v in color_frame_roi[y, x])  # stream is BGR, convert to RGB
                pixel = (r, g, b)
                color_sums[color] += pixel
                if is_color_close(pixel, expected_rgb):
                    color_match_count[color] += 1
                else:
                    log.debug(f"Frame {i} - {color} at ({x},{y}) sampled: {pixel} too far from expected {expected_rgb}")

            if DEBUG_MODE:
                dbg = draw_debug(img_bgr, color_frame_roi)
                cv2.imshow("PageDetect - camera | A4", dbg)
                cv2.waitKey(1)

        # wait for close
        # if DEBUG_MODE:
        #     cv2.waitKey(0)

        # check colors sampled correctly
        min_passes = int(NUM_FRAMES * FRAMES_PASS_THRESHOLD)
        for name, count in color_match_count.items():
            avg = tuple(int(v) for v in color_sums[name] // NUM_FRAMES)
            log.info(f"{name.title()} passed in {count}/{NUM_FRAMES} frames  avg={avg} expected={expected_colors[name]}")
            check.is_true(count >= min_passes)

        if any(c < min_passes for c in color_match_count.values()) and last_frame_bgr is not None:
            save_failure_snapshot(__file__, pipeline, draw_debug(last_frame_bgr, last_roi))

    except Exception as e:
        save_failure_snapshot(__file__, pipeline)
        raise e
    finally:
        cv2.destroyAllWindows()
        pipeline.stop()


def test_basic_color(test_device, test_context_var):
    dev, ctx = test_device

    configurations = [((1280, 720), 30)]
    # on nightly we check additional arbitrary configurations
    if "nightly" in test_context_var:
        configurations += [
            ((640,480), 15),
            ((640,480), 30),
            ((640,480), 60),
            ((848,480), 15),
            ((848,480), 30),
            ((848,480), 60),
            ((1280,720), 5),
            ((1280,720), 10),
            ((1280,720), 15),
        ]

    for resolution, fps in configurations:
        run_test(ctx, resolution, fps)
