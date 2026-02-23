# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# test:device each(D400*)
# test:timeout 400  # extra time for page detection

import pyrealsense2 as rs
from rspy import log, test
import numpy as np
import cv2
import time
from iq_helper import find_roi_location, get_roi_from_frame, sample_depth_region, SAMPLE_REGION_SIZE, WIDTH, HEIGHT

NUM_FRAMES = 100  # Number of frames to check
DEPTH_TOLERANCE = 100  # Acceptable deviation from expected depth in mm
FRAMES_PASS_THRESHOLD = 0.75  # Percentage of frames that needs to pass
DEBUG_MODE = False

EXPECTED_DEPTH_DIFF = 120  # Expected difference in mm between background and cube

dev, ctx = test.find_first_device_or_exit()
depth_sensor = dev.first_depth_sensor()


def detect_roi_with_exposure(marker_ids):
    # Set increasingly high exposure to be able to detect ArUco markers
    global pipeline, depth_sensor
    exposure = 10000
    max_exposure = 30000
    step = 10000
    while exposure <= max_exposure:
        start_time = time.time()
        depth_sensor.set_option(rs.option.exposure, exposure)
        try:
            find_roi_location(pipeline, marker_ids, DEBUG_MODE,
                              timeout=15)  # extended timeout for some cases like low fps
            log.d("Page found within ", time.time() - start_time)
            return True
        except Exception as e:
            log.d("Got an exception:", str(e), "within", time.time() - start_time)
            exposure += step
            log.d("Failed to detect markers with exposure", exposure - step,
                  ", trying with exposure", exposure)

    raise Exception("Page not found")


def draw_debug(depth_frame, cube_x, cube_y, bg_x, bg_y,
               depth_cube, depth_bg, measured_diff):
    # original debug visualization moved here, with added sampled-region rectangles
    colorizer = rs.colorizer()
    colorized_frame = colorizer.colorize(depth_frame)
    roi_img_disp = get_roi_from_frame(colorized_frame)

    # Draw points for cube and background (cv2.circle uses (x, y) order)
    cv2.circle(roi_img_disp, (cube_x, cube_y), 6, (0, 0, 255), -1)  # Red for cube
    cv2.circle(roi_img_disp, (bg_x, bg_y), 6, (0, 255, 0), -1)  # Green for background

    # Draw sampled region rectangles on top (requested)
    half = SAMPLE_REGION_SIZE // 2
    cv2.rectangle(roi_img_disp,
                  (cube_x - half, cube_y - half),
                  (cube_x + half, cube_y + half),
                  (0, 0, 255), 2)
    cv2.rectangle(roi_img_disp,
                  (bg_x - half, bg_y - half),
                  (bg_x + half, bg_y + half),
                  (0, 255, 0), 2)

    # Add labels for each point with their measured distance
    font = cv2.FONT_HERSHEY_SIMPLEX
    font_scale = 0.5
    thickness = 1
    cube_label = f"cube: {depth_cube:.2f}mm"
    bg_label = f"bg: {depth_bg:.2f}mm"
    diff_label = f"diff: {measured_diff:.3f}mm (exp: {EXPECTED_DEPTH_DIFF:.2f}mm)"

    cv2.putText(roi_img_disp, cube_label, (cube_x + 10, cube_y - 10),
                font, font_scale, (0, 0, 255), thickness, cv2.LINE_AA)
    cv2.putText(roi_img_disp, bg_label, (bg_x + 10, bg_y - 10),
                font, font_scale, (0, 255, 0), thickness, cv2.LINE_AA)
    cv2.putText(roi_img_disp, diff_label, (10, 30),
                font, font_scale, (255, 255, 255), thickness, cv2.LINE_AA)

    cv2.imshow("ROI with Sampled Points", roi_img_disp)
    cv2.waitKey(1)


def run_test(resolution, fps):
    try:
        global pipeline
        pipeline = rs.pipeline(ctx)
        profile = None
        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth, resolution[0], resolution[1], rs.format.z16, fps)
        cfg.enable_stream(rs.stream.infrared, 1, resolution[0], resolution[1], rs.format.y8,
                          fps)  # needed for finding the ArUco markers
        if not cfg.can_resolve(pipeline):
            log.i(f"Configuration {resolution[0]}x{resolution[1]} @ {fps}fps is not supported by the device")
            return
        profile = pipeline.start(cfg)
        time.sleep(2)

        depth_sensor = profile.get_device().first_depth_sensor()
        depth_scale = depth_sensor.get_depth_scale()

        # find region of interest (page) and get the transformation matrix
        # markers in the lab for this test are 4,5,6,7
        detect_roi_with_exposure((4, 5, 6, 7))

        # Known pixel positions - center of cube and left edge to sample background
        cube_x, cube_y = WIDTH // 2, HEIGHT // 2
        bg_x, bg_y = int(WIDTH * 0.1), HEIGHT // 2

        pass_count = 0
        for i in range(NUM_FRAMES):
            frames = pipeline.wait_for_frames()
            depth_frame = frames.get_depth_frame()
            infrared_frame = frames.get_infrared_frame()
            if not depth_frame:
                continue

            # Get the warped ROI from the filtered depth frame
            depth_image = get_roi_from_frame(depth_frame)

            # Sample depths using region averaging
            raw_cube = sample_depth_region(depth_image, cube_x, cube_y)
            raw_bg = sample_depth_region(depth_image, bg_x, bg_y)
            if not raw_bg or not raw_cube:
                i -= 1
                continue
            depth_cube = raw_cube  # * depth_scale
            depth_bg = raw_bg  # * depth_scale
            measured_diff = depth_bg - depth_cube  # background should be further than cube

            if abs(measured_diff - EXPECTED_DEPTH_DIFF) <= DEPTH_TOLERANCE:
                pass_count += 1
            else:
                log.d(f"Frame {i} - Depth diff: {measured_diff:.3f}mm too far from "
                      f"{EXPECTED_DEPTH_DIFF:.3f}mm (cube: {depth_cube:.3f}mm, bg: {depth_bg:.3f}mm)")

            if DEBUG_MODE:
                draw_debug(depth_frame, cube_x, cube_y, bg_x, bg_y, depth_cube, depth_bg, measured_diff)

        # wait for close
        # if DEBUG_MODE:
        #     cv2.waitKey(0)

        min_passes = int(NUM_FRAMES * FRAMES_PASS_THRESHOLD)
        log.i(f"Depth diff passed in {pass_count}/{NUM_FRAMES} frames")
        test.check(pass_count >= min_passes)

    except Exception as e:
        test.fail()
        raise e
    finally:
        cv2.destroyAllWindows()
        if profile:
            pipeline.stop()


log.d("context:", test.context)

configurations = [((1280, 720), 30)]
# on nightly we check additional arbitrary configurations
if "nightly" in test.context:
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

for resolution, fps in configurations:
    test.start("Basic Depth Image Quality Test", f"{resolution[0]}x{resolution[1]} @ {fps}fps")
    run_test(resolution, fps)
    test.finish()

test.print_results_and_exit()
