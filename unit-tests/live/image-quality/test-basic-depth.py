# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# test:device each(D400*)

import pyrealsense2 as rs
from rspy import log, test
import numpy as np
import cv2
import time
from iq_helper import find_roi_location, get_roi_from_frame, WIDTH, HEIGHT
import os
import tempfile
import shutil
import uuid
from datetime import datetime

NUM_FRAMES = 100 # Number of frames to check
DEPTH_TOLERANCE = 0.08  # Acceptable deviation from expected depth in meters
FRAMES_PASS_THRESHOLD = 0.75 # Percentage of frames that needs to pass
DEBUG_MODE = False

EXPECTED_DEPTH_DIFF = 0.10  # Expected difference in meters between background and cube

dev, ctx = test.find_first_device_or_exit()
depth_sensor = dev.first_depth_sensor()

def detect_roi_with_exposure(marker_ids):
    # Set increasingly high exposure to be able to detect ArUco markers
    global pipeline, depth_sensor
    exposure = 10000
    max_exposure = 30000
    step = 10000
    while exposure <= max_exposure:
        depth_sensor.set_option(rs.option.exposure, exposure)
        try:
            find_roi_location(pipeline, marker_ids, DEBUG_MODE)  # markers in the lab are 4,5,6,7
            return True
        except Exception:
            exposure += step
            log.d("Failed to detect markers with exposure", exposure - step,
                  ", trying with exposure", exposure)

    raise Exception("Page not found")

def average_depth_in_region(depth_image, center_x, center_y, region_size=5):
    """
    Samples a square region around (center_x, center_y) in depth_image,
    ignores zero values, and returns the average depth value.
    """
    half = region_size // 2
    h, w = depth_image.shape
    values = []
    for dy in range(-half, half + 1):
        for dx in range(-half, half + 1):
            x = center_x + dx
            y = center_y + dy
            if 0 <= x < w and 0 <= y < h:
                v = depth_image[y, x]
                if v > 0:
                    values.append(v)
    if values:
        return np.mean(values)
    else:
        return 0

def run_test(resolution, fps):
    try:
        global pipeline
        pipeline = rs.pipeline(ctx)
        profile = None
        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth, resolution[0], resolution[1], rs.format.z16, fps)
        cfg.enable_stream(rs.stream.infrared, 1, resolution[0], resolution[1], rs.format.y8, fps)  # needed for finding the ArUco markers
        if not cfg.can_resolve(pipeline):
            log.i(f"Configuration {resolution[0]}x{resolution[1]} @ {fps}fps is not supported by the device")
            return

        # --- Recording logic ---
        temp_dir = tempfile.gettempdir()
        temp_bag = os.path.join(temp_dir, f"test-basic-depth-{uuid.uuid4()}.bag")
        cfg.enable_record_to_file(temp_bag)
        # --- End recording logic ---

        profile = pipeline.start(cfg)
        time.sleep(2)

        depth_sensor = profile.get_device().first_depth_sensor()
        depth_scale = depth_sensor.get_depth_scale()

        # find region of interest (page) and get the transformation matrix
        detect_roi_with_exposure((4,5,6,7))

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

            # Get the warped ROI from the depth frame
            depth_image = get_roi_from_frame(depth_frame)

            # Sample depths (average over region, ignore zeros)
            raw_cube = average_depth_in_region(depth_image, cube_x, cube_y, region_size=5)
            raw_bg = average_depth_in_region(depth_image, bg_x, bg_y, region_size=5)
            depth_cube = raw_cube * depth_scale
            depth_bg = raw_bg * depth_scale
            measured_diff = depth_bg - depth_cube  # background should be further than cube

            if raw_cube == 0 or raw_bg == 0:
                log.d(f"Frame {i} - Not enough valid depth points in region (cube or bg)")
                continue

            if abs(measured_diff - EXPECTED_DEPTH_DIFF) <= DEPTH_TOLERANCE:
                pass_count += 1
            else:
                log.d(f"Frame {i} - Depth diff: {measured_diff:.3f}m too far from "
                      f"{EXPECTED_DEPTH_DIFF:.3f}m (cube: {depth_cube:.3f}m, bg: {depth_bg:.3f}m)")

            if DEBUG_MODE:
                colorizer = rs.colorizer()
                colorized_frame = colorizer.colorize(depth_frame)
                roi_img_disp = get_roi_from_frame(colorized_frame)

                # Draw region rectangles for cube and background
                region = 5
                half = region // 2
                cv2.rectangle(roi_img_disp, (cube_x - half, cube_y - half), (cube_x + half, cube_y + half), (0, 0, 255), 1)
                cv2.rectangle(roi_img_disp, (bg_x - half, bg_y - half), (bg_x + half, bg_y + half), (0, 255, 0), 1)

                # Draw points for cube and background (cv2.circle uses (x, y) order)
                cv2.circle(roi_img_disp, (cube_x, cube_y), 3, (0, 0, 255), -1)  # Red for cube
                cv2.circle(roi_img_disp, (bg_x, bg_y), 3, (0, 255, 0), -1)      # Green for background

                # Add labels for each point with their measured distance
                font = cv2.FONT_HERSHEY_SIMPLEX
                font_scale = 0.5
                thickness = 1
                cube_label = f"cube: {depth_cube:.2f}m"
                bg_label = f"bg: {depth_bg:.2f}m"
                diff_label = f"diff: {measured_diff:.3f}m (exp: {EXPECTED_DEPTH_DIFF:.2f}m)"

                cv2.putText(roi_img_disp, cube_label, (cube_x + 10, cube_y - 10),
                           font, font_scale, (0, 0, 255), thickness, cv2.LINE_AA)
                cv2.putText(roi_img_disp, bg_label, (bg_x + 10, bg_y - 10),
                           font, font_scale, (0, 255, 0), thickness, cv2.LINE_AA)
                cv2.putText(roi_img_disp, diff_label, (10, 30),
                           font, font_scale, (255, 255, 255), thickness, cv2.LINE_AA)

                cv2.imshow("ROI with Sampled Points", roi_img_disp)
                cv2.waitKey(1)

        # wait for close
        # if DEBUG_MODE:
        #     cv2.waitKey(0)

        min_passes = int(NUM_FRAMES * FRAMES_PASS_THRESHOLD)
        log.i(f"Depth diff passed in {pass_count}/{NUM_FRAMES} frames")
        test.check(pass_count >= min_passes)

        # --- Recording file handling: success ---
        if os.path.exists(temp_bag):
            os.remove(temp_bag)
        # --- End recording file handling ---

    except Exception as e:
        test.fail()
        # --- Recording file handling: failure ---
        if 'temp_bag' in locals() and os.path.exists(temp_bag):
            # Save with unique name in current directory
            unique_name = f"failed-basic-depth-{datetime.now().strftime('%Y%m%d-%H%M%S')}-{uuid.uuid4().hex[:8]}.bag"
            dest_path = os.path.join(os.getcwd(), unique_name)
            shutil.move(temp_bag, dest_path)
            log.i(f"Recording saved to {dest_path}")
        # --- End recording file handling ---
        raise e
    finally:
        cv2.destroyAllWindows()
        if profile:
            pipeline.stop()
        # Clean up temp file if it still exists
        if 'temp_bag' in locals() and os.path.exists(temp_bag):
            try:
                os.remove(temp_bag)
            except Exception:
                pass


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
    test.start("Basic Depth Image Quality Test", f"{resolution[0]}x{resolution[1]} @ {fps}fps")
    run_test(resolution, fps)
    test.finish()

test.print_results_and_exit()
