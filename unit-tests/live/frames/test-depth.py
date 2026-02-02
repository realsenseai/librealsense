# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

# test:device each(D400*)
# test:device each(D500*) 

import pyrealsense2 as rs
from rspy import test, log
from rspy import tests_wrapper as tw
import numpy as np
import os
import time

# Start depth + color streams and go through the frame to make sure it is showing a depth image
# Color stream is only used to display the way the camera is facing
# Verify that the frame does indeed have variance - therefore it is showing a depth image

# In debug mode, an image with the frames found will be displayed and saved
# Make sure you have the required packages installed
DEBUG_MODE = False

# Defines how far in cm do pixels have to be, to be considered in a different distance
# for example, 5 for 5cm, will define the range 100-104 cm as one (as 100)
DETAIL_LEVEL = 5 
BLACK_PIXEL_THRESHOLD = 0.5 # Fail if more than 50% pixels are zero
FRAMES_TO_CHECK = 30 # Number of frames to check for meaningful depth

dev, ctx = test.find_first_device_or_exit()
tw.start_wrapper(dev)

cfg = rs.config()
cfg.enable_stream(rs.stream.depth, rs.format.z16, 30)
if DEBUG_MODE:
    cfg.enable_stream(rs.stream.color, rs.format.bgr8, 30)

pipeline = rs.pipeline(ctx)
pipeline_profile = pipeline.start(cfg)
pipeline.wait_for_frames()
time.sleep(2)


def frames_to_image(depth, color, save, display):
    """
    This function gets depth and color frames, transforms them to an image (numpy array)
    and then save and/or display

    If color frame is given, it will also concatenate it with the depth frame before doing the given action
    """

    def display_image(img):
        """
        Display a given image and exits when the x button or esc key are pressed
        """
        import cv2

        window_title = "Output Stream"
        cv2.imshow(window_title, img)
        while cv2.getWindowProperty(window_title, cv2.WND_PROP_VISIBLE) > 0:
            k = cv2.waitKey(33)
            if k == 27:  # Esc key to stop
                cv2.destroyAllWindows()
                break
            elif k == -1:  # normally -1 returned
                pass

    import cv2

    colorizer = rs.colorizer()
    depth_image = np.asanyarray(colorizer.colorize(depth).get_data())
    img = depth_image

    if color:  # if color frame was successfully captured, merge it and the depth frame
        from scipy.ndimage import zoom
        color_image = np.asanyarray(color.get_data())
        depth_rows, _, _ = depth_image.shape
        color_rows, _, _ = color_image.shape
        # resize the image with the higher resolution to look like the smaller one
        if depth_rows < color_rows:
            color_image = zoom(color_image, (depth_rows / color_rows, depth_rows / color_rows, 1))
        elif color_rows < depth_rows:
            depth_image = zoom(depth_image, (color_rows / depth_rows, color_rows / depth_rows, 1))
        img = np.concatenate((depth_image, color_image), axis=1)

    if save:
        file_name = f"output_stream.png"
        log.i("Saved image in", os.getcwd() + "\\" + file_name)
        cv2.imwrite(file_name, img)
    if display:
        display_image(img)


def get_distances(depth_frame):
    MAX_METERS = 10  # max distance that can be detected, in meters

    depth_m = np.asanyarray(depth_frame.get_data()).astype(np.float32) * depth_frame.get_units()

    valid_mask = (depth_m < MAX_METERS)
    valid_depths = depth_m[valid_mask]  # ignore invalid pixels

    # convert to cm and round according to DETAIL_LEVEL
    rounded_depths = (np.floor(valid_depths * 100.0 / DETAIL_LEVEL) * DETAIL_LEVEL).astype(np.int32)

    unique_vals, counts = np.unique(rounded_depths, return_counts=True)

    dists = dict(zip(unique_vals.tolist(), counts.tolist()))
    total = valid_depths.size

    log.d("Distances detected in frame are:", dists)
    return dists, total


def is_depth_fill_rate_enough(save_image=False, show_image=False):
    """
    Checks if the camera is showing a frame with a enough depth fill rate.

    returns true if frame depth fill rate is enough
    """
    frames = pipeline.wait_for_frames()
    depth = frames.get_depth_frame()
    color = frames.get_color_frame()
    if not depth:
        log.f("Error getting depth frame")
        return False
    if DEBUG_MODE and not color:
        log.e("Error getting color frame")

    dists, total = get_distances(depth)
    num_blank_pixels = dists.get(0, 0)
    # Check for mostly black image
    if num_blank_pixels > total * BLACK_PIXEL_THRESHOLD:
        percent_blank = 100.0 * num_blank_pixels / total if total > 0 else 0
        log.f(f"Too many blank pixels: {num_blank_pixels}/{total} ({percent_blank:.1f}%)")
        return False, num_blank_pixels
    # Remove zero values from dists for meaningful depth check
    dists_no_zero = {k: v for k, v in dists.items() if k != 0}
    if save_image or show_image:
        frames_to_image(depth, color, save_image, show_image)
    # Find the largest non-zero depth bin
    max_nonzero_count = max(dists_no_zero.values()) if dists_no_zero else 0
    max_nonzero_percent = 100.0 * max_nonzero_count / total if total > 0 else 0
    fill_rate = 100.0 * (total - num_blank_pixels) / total if total > 0 else 0
    fill_rate_ok = fill_rate > (1 - BLACK_PIXEL_THRESHOLD) * 100.0
    log.i(f"Depth fill rate: {fill_rate:.1f}% (blank pixels: {num_blank_pixels}/{total}")
    return fill_rate_ok, num_blank_pixels


################################################################################################

test.start("Testing depth frame - laser ON -", dev.get_info(rs.camera_info.name))
# enable laser
sensor = pipeline_profile.get_device().first_depth_sensor()
if sensor.supports(rs.option.laser_power):
    sensor.set_option(rs.option.laser_power, sensor.get_option_range(rs.option.laser_power).max)
sensor.set_option(rs.option.emitter_enabled, 1)  # should be set to 0 for laser off

has_depth = False

# we check a few different frames to try and detect depth
for frame_num in range(FRAMES_TO_CHECK):
    has_depth, laser_black_pixels = is_depth_fill_rate_enough(save_image=DEBUG_MODE, show_image=DEBUG_MODE)
    if has_depth:
        break

test.check(has_depth is True)
test.finish()

pipeline.stop()
tw.stop_wrapper(dev)
test.print_results_and_exit()