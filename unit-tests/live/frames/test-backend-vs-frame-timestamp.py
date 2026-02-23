# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# test:device D400*
# test:device each(D500*)

import pyrealsense2 as rs
from rspy import test, log
import time

dev, _ = test.find_first_device_or_exit()
depth_sensor = dev.first_depth_sensor()
is_global_time_enabled_orig = False
fps = 30
has_at_least_one_frame_arrived = False


def check_backend_ts_greater_than_frame_ts(frame):
    global has_at_least_one_frame_arrived
    has_at_least_one_frame_arrived = True
    backend_ts_supported = frame.supports_frame_metadata(rs.frame_metadata_value.backend_timestamp)
    test.check(backend_ts_supported)
    frame_ts = frame.get_frame_timestamp()
    backend_ts = frame.get_frame_metadata(rs.frame_metadata_value.backend_timestamp)
    delta = backend_ts - frame_ts
    if not test.check(delta > 0):
        log.e("frame_ts = " + repr(frame_ts) + ", backend_ts = " + repr(backend_ts) + ", delta = " + repr(delta) + ", this should be positive")


######################### DEPTH SENSOR ######################################################
#############################################################################################
with test.closure("Set Depth stream time domain to Global"):
    is_global_time_enabled_orig = depth_sensor.get_option(rs.option.global_time_enabled)
    if not is_global_time_enabled_orig:
        depth_sensor.set_option(rs.option.global_time_enabled, 1)
    test.check_equal(int(depth_sensor.get_option(rs.option.global_time_enabled)), 1)

#############################################################################################
with test.closure("Get Depth frame timestamp and compare it to backend timestamp"):
    depth_profile = next(p for p in
                         depth_sensor.profiles if p.fps() == fps
                         and p.stream_type() == rs.stream.depth
                         and p.format() == rs.format.z16
                         and p.as_video_stream_profile().width() == 1280
                         and p.as_video_stream_profile().height() == 720)

    depth_sensor.open(depth_profile)
    depth_sensor.start(check_backend_ts_greater_than_frame_ts)
    time.sleep(1)
    test.check(has_at_least_one_frame_arrived)
    depth_sensor.stop()
    depth_sensor.close()

#############################################################################################
with test.closure("Restore Depth original time domain"):
    if not is_global_time_enabled_orig:
        depth_sensor.set_option(rs.option.global_time_enabled, 0)
    global_time_orig_val = 1 if is_global_time_enabled_orig else 0
    test.check_equal(int(depth_sensor.get_option(rs.option.global_time_enabled)), global_time_orig_val)

#############################################################################################


color_sensor = dev.first_color_sensor()
is_global_time_enabled_orig = False
fps = 30
has_at_least_one_frame_arrived = False

##################### COLOR SENSOR ##########################################################
#############################################################################################
with test.closure("Set Color stream time domain to HW"):
    is_global_time_enabled_orig = color_sensor.get_option(rs.option.global_time_enabled)
    if not is_global_time_enabled_orig:
        color_sensor.set_option(rs.option.global_time_enabled, 1)
    test.check_equal(int(color_sensor.get_option(rs.option.global_time_enabled)), 1)

#############################################################################################
with test.closure("Get Color frame timestamp and compare it to backend timestamp"):
    color_profile = next(p for p in
                         color_sensor.profiles if p.fps() == fps
                         and p.stream_type() == rs.stream.color
                         and p.format() == rs.format.rgb8
                         and p.as_video_stream_profile().width() == 1280
                         and p.as_video_stream_profile().height() == 720)

    color_sensor.open(color_profile)
    color_sensor.start(check_backend_ts_greater_than_frame_ts)
    time.sleep(1)
    test.check(has_at_least_one_frame_arrived)
    color_sensor.stop()
    color_sensor.close()

#############################################################################################
with test.closure("Restore Color original time domain"):
    if not is_global_time_enabled_orig:
        color_sensor.set_option(rs.option.global_time_enabled, 0)
    global_time_orig_val = 1 if is_global_time_enabled_orig else 0
    test.check_equal(int(color_sensor.get_option(rs.option.global_time_enabled)), global_time_orig_val)
test.print_results_and_exit()
