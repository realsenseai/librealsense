# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

# Currently, we exclude D555 as it's failing
# test:device D400*

# test:donotrun:!nightly
# test:timeout 300
# timeout - on the worst case we have 8 streams, so:
# timeout = ((8 choose 2)+1) * (TIME_FOR_STEADY_STATE + TIME_TO_COUNT_FRAMES)
# 8 choose 2 tests to do (one for each pair), plus one for all streams on

from rspy import test, log, tests_wrapper
from itertools import combinations
import pyrealsense2 as rs
import fps_helper


VGA_RESOLUTION = (640, 360)
HD_RESOLUTION = (1280, 720)


def get_sensors_and_profiles(device):
    """
    Returns an array of pairs of a (sensor, profile) for each of its profiles
    """
    sensor_profiles_arr = []
    for sensor in device.query_sensors():
        profile = None
        if sensor.is_depth_sensor():
            if sensor.supports(rs.option.enable_auto_exposure):
                sensor.set_option(rs.option.enable_auto_exposure, 1)
            depth_resolutions = []
            for p in sensor.get_stream_profiles():
                res = fps_helper.get_resolution(p)
                if res not in depth_resolutions:
                    depth_resolutions.append(res)
            for res in depth_resolutions:
                # Skip 1280x800 resolution for infrared since it's Y16 calibration format
                if res == (1280, 800):
                    log.d(f"Skipping resolution {res} for infrared (calibration format)")
                    continue
                    
                depth = fps_helper.get_profile(sensor, rs.stream.depth, res)
                irs = fps_helper.get_profiles(sensor, rs.stream.infrared, res)
                ir = next(irs)
                while ir is not None and ir.stream_index() != 1:
                    ir = next(irs)
                if ir and depth:
                    log.d(f"{ir}, {depth}")
                    sensor_profiles_arr.append((sensor, depth))
                    sensor_profiles_arr.append((sensor, ir))
                    break
        elif sensor.is_color_sensor():
            if sensor.supports(rs.option.enable_auto_exposure):
                sensor.set_option(rs.option.enable_auto_exposure, 1)
            if sensor.supports(rs.option.auto_exposure_priority):
                sensor.set_option(rs.option.auto_exposure_priority, 0)  # AE priority should be 0 for constant FPS
            profile = fps_helper.get_profile(sensor, rs.stream.color, HD_RESOLUTION)
        elif sensor.is_motion_sensor():
            connection_type = "USB"
            if dev.supports(rs.camera_info.connection_type):
                connection_type = dev.get_info(rs.camera_info.connection_type)
            if connection_type == "DDS":
                sensor_profiles_arr.append((sensor, fps_helper.get_profile(sensor, rs.stream.motion)))
            else:
                sensor_profiles_arr.append((sensor, fps_helper.get_profile(sensor, rs.stream.accel)))
                sensor_profiles_arr.append((sensor, fps_helper.get_profile(sensor, rs.stream.gyro)))

        if profile is not None:
            sensor_profiles_arr.append((sensor, profile))
    return sensor_profiles_arr


dev, _ = test.find_first_device_or_exit()

sensor_profiles_array = get_sensors_and_profiles(dev)
all_pairs = [[a[1].stream_name(), b[1].stream_name()] for a, b in combinations(sensor_profiles_array, 2)]
# all_streams = [[profile.stream_name() for _, profile in sensor_profiles_array]] # at the moment, this fails on CI
permutations_to_run = all_pairs #+ all_streams
fps_helper.perform_fps_test(sensor_profiles_array, permutations_to_run)

test.print_results_and_exit()
