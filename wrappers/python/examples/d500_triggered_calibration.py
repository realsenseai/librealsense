## License: Apache 2.0. See LICENSE file in root directory.
## Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

import os
import time
import zlib

import pyrealsense2 as rs
import numpy as np

'''
aim of this script is to show how to use the triggered calibration api with safety camera,
and to check the improvement of the depth stream before / after the running of the calibration
'''

is_safety_camera = False


def check_d500_camera_found(dev):
    global is_safety_camera
    product_line = dev.get_info(rs.camera_info.product_line)
    if product_line != "D500":
        print("Device is not D500 Product Line - please check")
        exit(1)
    device_name = dev.get_info(rs.camera_info.name)
    if "D585S" in device_name:
        is_safety_camera = True
    print("D500 Product Line Camera found")


def compute_fill_rate(frame):
    frame_array = np.hstack(np.asarray(frame.get_data(), dtype=np.uint16))
    num_of_pixels = len(frame_array)
    fill_rate = 0
    for pixel in frame_array:
        if pixel > 0:
            fill_rate += 1
    return fill_rate / num_of_pixels * 100


def stream_and_get_fill_rate():
    print("Streaming frames...")
    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth, 0, 1280, 720, rs.format.z16, 30)
    pipe = rs.pipeline()
    pipe.start(cfg)
    frames_captured = 0
    fill_rate_total = 0
    num_of_frames = 3
    try:
        while frames_captured < num_of_frames:
            frames = pipe.wait_for_frames()

            depth_frame = frames.get_depth_frame()
            fill_rate = compute_fill_rate(depth_frame)
            fill_rate_total += fill_rate

            frames_captured += 1

    finally:
        pipe.stop()
    print("frames streamed!")
    fill_rate_mean_value = fill_rate_total / num_of_frames
    print("fill rate found = " + repr(fill_rate_mean_value) + "%")
    return fill_rate_mean_value


def build_header(new_calib_array):
    version_major = 0
    version_minor = 4
    depth_table_id = 0xb4
    depth_table_size = 496
    counter = 0
    test_cal_version = 0
    crc_computed = zlib.crc32(new_calib_array)
    version_major_ints = np.frombuffer(version_major.to_bytes(1, 'little'), dtype=np.uint8)
    version_minor_ints = np.frombuffer(version_minor.to_bytes(1, 'little'), dtype=np.uint8)
    depth_table_id_ints = np.frombuffer(depth_table_id.to_bytes(2, 'little'), dtype=np.uint8)
    depth_table_size_ints = np.frombuffer(depth_table_size.to_bytes(4, 'little'), dtype=np.uint8)
    counter_ints = np.frombuffer(counter.to_bytes(2, 'little'), dtype=np.uint8)
    test_cal_version_ints = np.frombuffer(test_cal_version.to_bytes(2, 'little'), dtype=np.uint8)
    crc_ints = np.frombuffer(crc_computed.to_bytes(4, 'little'), dtype=np.uint8)

    version_ints = np.hstack((version_major_ints, version_minor_ints))
    with_table_id = np.hstack((version_ints, depth_table_id_ints))
    with_table_size = np.hstack((with_table_id, depth_table_size_ints))
    with_counter = np.hstack((with_table_size, counter_ints))
    with_test_cal_version = np.hstack((with_counter, test_cal_version_ints))
    header_ready = np.hstack((with_test_cal_version, crc_ints))

    return header_ready


def get_depth_calib_from_device():
    print("Downloading Depth Calibration from device")
    global dev
    hwm_dev = dev.as_debug_protocol()
    get_calib_opcode = 0xa7
    flash_mem_enum = 1
    depth_calibration_id = 0xb4
    d500_calib_dynamic = 0
    cmd = hwm_dev.build_command(opcode=get_calib_opcode,
                                param1=flash_mem_enum,
                                param2=depth_calibration_id,
                                param3=d500_calib_dynamic)
    ans = hwm_dev.send_and_receive_raw_data(cmd)
    # slicing:
    # the 4 first bytes - opcode
    # the 16 following bytes = header
    ans_data = ans[20:]
    return ans_data


def save_calib_to_file(depth_calib, filename):
    depth_calib_bytes = bytearray(depth_calib)
    curr_path = os.path.dirname(os.path.realpath(__file__))
    depth_calib_file_path = os.path.join(curr_path, filename)
    with open(depth_calib_file_path, mode="wb") as calib_file:
        calib_file.write(depth_calib_bytes)
        print("Calibration written to " + repr(depth_calib_file_path))


def progress_cb(progress):
    print("Calibration in process - progress = " + repr(progress), end='\r')


def run_calibration():
    print("Triggering Calibration")
    ac_device = dev.as_auto_calibrated_device()
    depth_calib_after, health_tuple = ac_device.run_on_chip_calibration("calib run", progress_cb, 100000)
    print("Calibration done")
    # offset to trim:
    # - 3 bytes for state, progress and result
    # - 16 bytes for hwm header
    offset_to_trim = 19
    depth_calib_after = depth_calib_after[offset_to_trim:]
    return depth_calib_after


def switch_device_to_service_mode(device):
    sensors = device.query_sensors()
    safety_sensor = next(s for s in
                         sensors if s.get_info(rs.camera_info.name) == "Safety Camera")
    safety_mode = safety_sensor.get_option(rs.option.safety_mode)
    if safety_mode != rs.safety_mode.service:
        safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.service)
    time.sleep(1)
    safety_mode = safety_sensor.get_option(rs.option.safety_mode)
    if safety_mode != rs.safety_mode.service:
        print("Could not switch Safety Sensor to Service Mode")
    else:
        print("Safety Sensor is now in Service Mode")


def exit_if_calibrations_are_equal():
    global calib_before, calib_after
    is_equal_check = (calib_before == calib_after)
    if is_equal_check:
        print("Calibrations before and after are the same!!!")
        exit(1)
    else:
        print("Calibration after is different than Calibration before")


def compare_results():
    global fill_rate_before, fill_rate_after
    print("Comparing Results:")
    delta_fill_rate_percentage = (fill_rate_after - fill_rate_before) / fill_rate_before
    if delta_fill_rate_percentage > 0:
        print("Fill rate improved by " + repr(delta_fill_rate_percentage) + "%")
    else:
        print("Fill rate decreased by " + repr(delta_fill_rate_percentage) + "% !!!")


ctx = rs.context()
time.sleep(2)
dev = ctx.query_devices().front()

# Check this device is Safety Camera
check_d500_camera_found(dev)

# Stream and get Depth Fill Rate
fill_rate_before = stream_and_get_fill_rate()

# Get existing depth calibration
calib_before = get_depth_calib_from_device()

# switching to service mode
if is_safety_camera:
    switch_device_to_service_mode(dev)

# Save existing calibration to file
save_calib_to_file(calib_before, "depth_calib_before.bin")

# Trigger Calibration - log progress
calib_after = run_calibration()

# Save new depth calibration to file
save_calib_to_file(calib_after, "depth_calib_after.bin")

# Stopping the script if the calibration received from the calibration operation
# is the same as before
exit_if_calibrations_are_equal()

# switching to service mode if safety camera
if is_safety_camera:
    switch_device_to_service_mode(dev)

# Stream and get Depth Fill Rate
fill_rate_after = stream_and_get_fill_rate()

# Compare
# check fill rate after is better than before
compare_results()
