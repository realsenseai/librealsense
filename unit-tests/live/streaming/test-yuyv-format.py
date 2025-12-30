# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

# test:device D400*
# This test checks YUYV format streaming and compares color accuracy with RGB8 format

import time
import numpy as np
import pyrealsense2 as rs
from rspy import test, log
from rspy.timer import Timer


yuyv_frame = None
rgb8_frame = None
yuyv_streamed = False
rgb8_streamed = False


def close_resources(sensor):
    """
    Stop and Close sensor.
    :sensor: sensor of device
    """
    if len(sensor.get_active_streams()) > 0:
        sensor.stop()
        sensor.close()


def yuyv_callback(frame):
    global yuyv_frame, yuyv_streamed
    yuyv_frame = frame
    yuyv_streamed = True


def rgb8_callback(frame):
    global rgb8_frame, rgb8_streamed
    rgb8_frame = frame
    rgb8_streamed = True


def yuyv_to_rgb8(yuyv_data):
    """
    Convert YUYV format to RGB8.
    YUYV: Y0 U Y1 V Y2 U Y3 V ...
    Output: RGB8 for each pixel
    """
    yuyv_array = np.frombuffer(yuyv_data, dtype=np.uint8)
    
    # Extract Y, U, V components
    y = yuyv_array[0::2]  # Y values at even indices
    u = yuyv_array[1::4]  # U values
    v = yuyv_array[3::4]  # V values
    
    # Handle proper pairing of U and V
    u_expanded = np.repeat(yuyv_array[1::4], 2)[:len(y)]
    v_expanded = np.repeat(yuyv_array[3::4], 2)[:len(y)]
    
    # YUV to RGB conversion
    r = np.clip(y + 1.402 * (v_expanded - 128), 0, 255).astype(np.uint8)
    g = np.clip(y - 0.344136 * (u_expanded - 128) - 0.714136 * (v_expanded - 128), 0, 255).astype(np.uint8)
    b = np.clip(y + 1.772 * (u_expanded - 128), 0, 255).astype(np.uint8)
    
    # Stack into RGB8
    rgb8 = np.stack([r, g, b], axis=1).flatten()
    return rgb8.astype(np.uint8)


def compare_colors(yuyv_rgb, rgb8_data, tolerance=10):
    """
    Compare converted YUYV RGB with native RGB8.
    :yuyv_rgb: converted YUYV to RGB
    :rgb8_data: native RGB8 data
    :tolerance: acceptable color difference per channel
    :return: average color difference and pass/fail
    """
    yuyv_rgb_array = np.frombuffer(yuyv_rgb, dtype=np.uint8)
    rgb8_array = np.frombuffer(rgb8_data, dtype=np.uint8)
    
    # Ensure same size
    min_size = min(len(yuyv_rgb_array), len(rgb8_array))
    yuyv_rgb_array = yuyv_rgb_array[:min_size]
    rgb8_array = rgb8_array[:min_size]
    
    # Calculate per-channel differences
    differences = np.abs(yuyv_rgb_array.astype(int) - rgb8_array.astype(int))
    avg_difference = np.mean(differences)
    max_difference = np.max(differences)
    
    passed = avg_difference <= tolerance
    
    return avg_difference, max_difference, passed


timer = Timer(10)

device, _ = test.find_first_device_or_exit()
color_sensor = device.first_color_sensor()

test.start('Check that YUYV and RGB8 formats stream and colors match:')

# Find YUYV and RGB8 profiles
try:
    profile_yuyv = next(p for p in color_sensor.profiles if p.format() == rs.format.yuyv)
    profile_rgb8 = next(p for p in color_sensor.profiles if p.format() == rs.format.rgb8)
    test.check(profile_yuyv and profile_rgb8, "YUYV and RGB8 profiles available")
    log.d(f"YUYV Profile: {profile_yuyv}")
    log.d(f"RGB8 Profile: {profile_rgb8}")
except StopIteration:
    test.check(False, "YUYV or RGB8 format not available")
    close_resources(color_sensor)
    test.finish()
    test.print_results_and_exit()

# Stream YUYV format
color_sensor.open(profile_yuyv)
color_sensor.start(yuyv_callback)

timer.start()
while not timer.has_expired():
    if yuyv_streamed:
        break
    time.sleep(0.1)

test.check(yuyv_streamed, "YUYV frame streamed successfully")
close_resources(color_sensor)

# Stream RGB8 format
color_sensor.open(profile_rgb8)
color_sensor.start(rgb8_callback)

timer = Timer(10)
timer.start()
while not timer.has_expired():
    if rgb8_streamed:
        break
    time.sleep(0.1)

test.check(rgb8_streamed, "RGB8 frame streamed successfully")
close_resources(color_sensor)

# Compare colors
if yuyv_streamed and rgb8_streamed:
    yuyv_rgb_converted = yuyv_to_rgb8(yuyv_frame.get_data())
    rgb8_data = rgb8_frame.get_data()
    
    avg_diff, max_diff, colors_match = compare_colors(yuyv_rgb_converted, rgb8_data, tolerance=10)
    
    log.d(f"Average color difference: {avg_diff:.2f}")
    log.d(f"Maximum color difference: {max_diff:.2f}")
    test.check(colors_match, "Colors in YUYV format match RGB8 format (within tolerance)")

test.finish()
test.print_results_and_exit()
