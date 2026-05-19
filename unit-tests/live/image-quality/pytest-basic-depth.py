# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import cv2
import time
import logging
from iq_helper import (find_roi_location, get_roi_from_frame, get_median_depth_from_region,
                       sample_bg_depth, make_depth_filter_chain, save_failure_snapshot,
                       SAMPLE_REGION_SIZE, BG_SAMPLE_POINTS, CUBE_CENTER, WIDTH, HEIGHT)

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.context("image-quality"),
    pytest.mark.device_each("D400*"),
    pytest.mark.device_exclude("D401"),
    pytest.mark.timeout(400),  # extra time for page detection
]

NUM_FRAMES = 100  # Number of frames to check
DEPTH_TOLERANCE = 100  # Acceptable deviation from expected depth in mm
FRAMES_PASS_THRESHOLD = 0.75  # Percentage of frames that needs to pass
DEBUG_MODE = False

EXPECTED_DEPTH_DIFF = 120  # Expected difference in mm between background and cube


def detect_roi_with_exposure(pipeline, depth_sensor, marker_ids):
    # Set increasingly high exposure to be able to detect ArUco markers
    exposure = 10000
    max_exposure = 30000
    step = 10000
    while exposure <= max_exposure:
        start_time = time.time()
        depth_sensor.set_option(rs.option.exposure, exposure)
        try:
            find_roi_location(pipeline, marker_ids, DEBUG_MODE,
                              timeout=15)  # extended timeout for some cases like low fps
            log.debug(f"Page found within {time.time() - start_time}")
            return True
        except Exception as e:
            log.debug(f"Got an exception: {e} within {time.time() - start_time}")
            exposure += step
            log.debug(f"Failed to detect markers with exposure {exposure - step}"
                      f", trying with exposure {exposure}")

    raise Exception("Page not found")


def draw_debug(depth_frame, cube_xy, depth_cube, depth_bg, measured_diff):
    colorizer = rs.colorizer()
    colorized_frame = colorizer.colorize(depth_frame)
    # Warp the colorized depth with INTER_NEAREST too so the debug view
    # reflects the exact pixels we sampled (no smoothing across the cube edge).
    roi_img_disp = get_roi_from_frame(colorized_frame, interpolation=cv2.INTER_NEAREST)

    half = SAMPLE_REGION_SIZE // 2
    cube_x, cube_y = cube_xy

    cv2.circle(roi_img_disp, (cube_x, cube_y), 6, (0, 0, 255), -1)
    cv2.rectangle(roi_img_disp,
                  (cube_x - half, cube_y - half),
                  (cube_x + half, cube_y + half),
                  (0, 0, 255), 2)

    for (bx, by) in BG_SAMPLE_POINTS:
        cv2.circle(roi_img_disp, (bx, by), 6, (0, 255, 0), -1)
        cv2.rectangle(roi_img_disp,
                      (bx - half, by - half),
                      (bx + half, by + half),
                      (0, 255, 0), 2)

    font = cv2.FONT_HERSHEY_SIMPLEX
    font_scale = 0.5
    thickness = 1
    cube_label = f"cube: {depth_cube:.2f}mm"
    bg_label = f"bg(med): {depth_bg:.2f}mm"
    diff_label = f"diff: {measured_diff:.3f}mm (exp: {EXPECTED_DEPTH_DIFF:.2f}mm)"

    cv2.putText(roi_img_disp, cube_label, (cube_x + 10, cube_y - 10),
                font, font_scale, (0, 0, 255), thickness, cv2.LINE_AA)
    cv2.putText(roi_img_disp, bg_label, (10, 50),
                font, font_scale, (0, 255, 0), thickness, cv2.LINE_AA)
    cv2.putText(roi_img_disp, diff_label, (10, 30),
                font, font_scale, (255, 255, 255), thickness, cv2.LINE_AA)

    return roi_img_disp


def run_test(dev, ctx, resolution, fps):
    log.info(f"Basic Depth Image Quality Test: {resolution[0]}x{resolution[1]} @ {fps}fps")
    depth_sensor = dev.first_depth_sensor()
    last_depth_frame = None
    last_depth_cube = 0
    last_depth_bg = 0
    last_measured_diff = 0
    pipeline = rs.pipeline(ctx)
    profile = None
    try:
        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth, resolution[0], resolution[1], rs.format.z16, fps)
        cfg.enable_stream(rs.stream.infrared, 1, resolution[0], resolution[1], rs.format.y8,
                          fps)  # needed for finding the ArUco markers
        if not cfg.can_resolve(pipeline):
            log.info(f"Configuration {resolution[0]}x{resolution[1]} @ {fps}fps is not supported by the device")
            return
        profile = pipeline.start(cfg)
        time.sleep(2)

        depth_filters = make_depth_filter_chain()

        # find region of interest (page) and get the transformation matrix
        # markers in the lab for this test are 4,5,6,7
        detect_roi_with_exposure(pipeline, depth_sensor, (4, 5, 6, 7))

        cube_xy = CUBE_CENTER

        pass_count = 0
        for i in range(NUM_FRAMES):
            frames = pipeline.wait_for_frames()
            depth_frame = frames.get_depth_frame()
            if not depth_frame:
                continue

            depth_frame = depth_filters(depth_frame)

            # Warp depth with nearest-neighbor: each output pixel takes the
            # value of the closest source pixel rather than a weighted blend
            # of neighbors. The default INTER_LINEAR is wrong for depth — at
            # the cube/paper boundary it averages two physically disjoint
            # surfaces (e.g. 1050 mm cube + 1225 mm paper -> 1137 mm ghost
            # pixels) which then contaminate the region medians.
            depth_image = get_roi_from_frame(depth_frame, interpolation=cv2.INTER_NEAREST)

            raw_cube = get_median_depth_from_region(depth_image, cube_xy[0], cube_xy[1])
            depth_bg, bg_readings = sample_bg_depth(depth_image)
            if not raw_cube or not depth_bg:
                continue
            depth_cube = raw_cube
            measured_diff = depth_bg - depth_cube  # background should be further than cube

            last_depth_frame = depth_frame
            last_depth_cube = depth_cube
            last_depth_bg = depth_bg
            last_measured_diff = measured_diff

            if abs(measured_diff - EXPECTED_DEPTH_DIFF) <= DEPTH_TOLERANCE:
                pass_count += 1
            else:
                log.debug(f"Frame {i} - Depth diff: {measured_diff:.3f}mm too far from "
                          f"{EXPECTED_DEPTH_DIFF:.3f}mm (cube: {depth_cube:.3f}mm, bg: {depth_bg:.3f}mm, "
                          f"bg_samples: {[f'{v:.1f}' for v in bg_readings]})")

            if DEBUG_MODE:
                dbg = draw_debug(depth_frame, cube_xy, depth_cube, depth_bg, measured_diff)
                cv2.imshow("ROI with Sampled Points", dbg)
                cv2.waitKey(1)

        # wait for close
        # if DEBUG_MODE:
        #     cv2.waitKey(0)

        min_passes = int(NUM_FRAMES * FRAMES_PASS_THRESHOLD)
        log.info(f"Depth diff passed in {pass_count}/{NUM_FRAMES} frames")
        check_ok = check.is_true(pass_count >= min_passes)

        if not check_ok and last_depth_frame:
            save_failure_snapshot(__file__, pipeline,
                                 draw_debug(last_depth_frame, cube_xy,
                                            last_depth_cube, last_depth_bg, last_measured_diff))

    except Exception as e:
        save_failure_snapshot(__file__, pipeline)
        raise e
    finally:
        cv2.destroyAllWindows()
        if profile:
            pipeline.stop()


def test_basic_depth(test_device, test_context_var):
    dev, ctx = test_device

    configurations = [((1280, 720), 30)]
    # on nightly we check additional arbitrary configurations
    if "nightly" in test_context_var:
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
        run_test(dev, ctx, resolution, fps)
