# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# Our commits can only seldom affect image quality (unless specifically processing the frames) no need to run for each commit
# test:donotrun:!nightly
# test:device each(D400*)

import pyrealsense2 as rs
from rspy import log, test
import numpy as np
import cv2
from iq_helper import find_roi_location, get_roi_from_frame, is_color_close, WIDTH, HEIGHT

NUM_FRAMES = 100 # Number of frames to check
COLOR_TOLERANCE = 60 # Acceptable per-channel deviation in RGB values
FRAMES_PASS_THRESHOLD =0.8 # Percentage of frames that needs to pass
DEBUG_MODE = False

# expected colors (insertion order -> mapped row-major to 3x3 grid)
expected_colors = {
    "red":   (132, 60, 60),
    "green": (40, 84, 72),
    "blue":  (20, 67, 103),
    "black": (35, 35, 35),
    "white": (150, 150, 150),
    "gray": (90, 90, 90),
    "purple": (56, 72, 98),
    "orange": (136, 86, 70),
    "yellow": (166, 142, 80),
}
# list of color names in insertion order -> used left->right, top->bottom
color_names = list(expected_colors.keys())

# we are given a 3x3 grid, we split it using 2 vertical and 2 horizontal separators
# we also calculate the center of each grid cell for sampling from it for the test
xs = [1.5 * WIDTH / 6.0, WIDTH / 2.0, 4.5 * WIDTH / 6.0]
ys = [1.5 * HEIGHT / 6.0, HEIGHT / 2.0, 4.5 * HEIGHT / 6.0]
centers = [(x, y) for y in ys for x in xs]

dev, ctx = test.find_first_device_or_exit()

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
        # white marker with black text for readability
        cv2.circle(a4_page_bgr, (cx_i, cy_i), 10, (255, 255, 255), -1)
        cv2.putText(a4_page_bgr, lbl, (cx_i + 12, cy_i + 6),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0,0,0), 2)

    # resize and display side by side
    height = 600
    frame_width = int(frame_bgr.shape[1] * (height / frame_bgr.shape[0]))
    a4_page_width = int(a4_page_bgr.shape[1] * (height / a4_page_bgr.shape[0]))
    left = cv2.resize(frame_bgr, (frame_width, height))
    right = cv2.resize(a4_page_bgr, (a4_page_width, height))
    return np.hstack([left, right])


def run_test(resolution, fps):
    color_match_count = {color: 0 for color in expected_colors.keys()}
    pipeline = rs.pipeline(ctx)
    cfg = rs.config()
    cfg.enable_stream(rs.stream.color, resolution[0], resolution[1], rs.format.bgr8, fps)
    if not cfg.can_resolve(pipeline):
        log.i(f"Configuration {resolution[0]}x{resolution[1]}@{fps}fps is not supported by the device")
        return
    pipeline_profile = pipeline.start(cfg)
    for i in range(60):  # skip initial frames
        pipeline.wait_for_frames()
    try:
        # find region of interest (page) and get the transformation matrix
        find_roi_location(pipeline, (0, 1, 2, 3), DEBUG_MODE) # markers in the lab are 0,1,2,3

        # sampling loop
        for i in range(NUM_FRAMES):
            frames = pipeline.wait_for_frames()
            color_frame = frames.get_color_frame()
            img_bgr = np.asanyarray(color_frame.get_data())

            color_frame_roi = get_roi_from_frame(color_frame)

            # sample each grid center and compare to expected color by row-major insertion order
            for idx, (x, y) in enumerate(centers):
                color = color_names[idx] if idx < len(color_names) else str(idx)
                expected_rgb = expected_colors[color]
                x = int(round(x))
                y = int(round(y))
                b, g, r = (int(v) for v in color_frame_roi[y, x])  # stream is BGR, convert to RGB
                pixel = (r, g, b)
                if is_color_close(pixel, expected_rgb, COLOR_TOLERANCE):
                    color_match_count[color] += 1
                else:
                    log.d(f"Frame {i} - {color} at ({x},{y}) sampled: {pixel} too far from expected {expected_rgb}")

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
            log.i(f"{name.title()} passed in {count}/{NUM_FRAMES} frames")
            test.check(count >= min_passes)

    except Exception as e:
        test.fail()
        raise e
    finally:
        cv2.destroyAllWindows()
        pipeline.stop()


log.d("context:", test.context)

configurations = [((1280, 720), 30)]
# on nightly we check additional arbitrary configurations
if "nightly" in test.context:
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
    test.start("Basic Color Image Quality Test:", f"{resolution[0]}x{resolution[1]} @ {fps}fps")
    run_test(resolution, fps)
    test.finish()

test.print_results_and_exit()
