# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test FPS accuracy for all pairwise sensor stream combinations.
Generates all (N choose 2) pairs of streams and verifies FPS for each.
"""

import pytest
import pyrealsense2 as rs
from itertools import combinations
import fps_helper
import logging
from rspy.snippets import is_dds_dev
log = logging.getLogger(__name__)

VGA_RESOLUTION = (640, 360)
HD_RESOLUTION = (1280, 720)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D555"),
    pytest.mark.device_exclude("D401"),
    pytest.mark.context("nightly"),
]


def get_sensors_and_profiles(dev):
    """Returns an array of pairs of a (sensor, profile) for each of its profiles."""
    sensor_profiles_arr = []
    for sensor in dev.query_sensors():
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
                    log.debug(f"Skipping resolution {res} for infrared (calibration format)")
                    continue

                depth = fps_helper.get_profile(sensor, rs.stream.depth, res)
                irs = fps_helper.get_profiles(sensor, rs.stream.infrared, res)
                ir = next(irs)
                while ir is not None and ir.stream_index() != 1:
                    ir = next(irs)
                if ir and depth:
                    log.debug(f"{ir}, {depth}")
                    sensor_profiles_arr.append((sensor, depth))
                    sensor_profiles_arr.append((sensor, ir))
                    break
        elif sensor.is_color_sensor():
            if sensor.supports(rs.option.enable_auto_exposure):
                sensor.set_option(rs.option.enable_auto_exposure, 1)
            if sensor.supports(rs.option.auto_exposure_priority):
                sensor.set_option(rs.option.auto_exposure_priority, 0)
            profile = fps_helper.get_profile(sensor, rs.stream.color, HD_RESOLUTION)
        elif sensor.is_motion_sensor():
            if is_dds_dev(dev):
                sensor_profiles_arr.append((sensor, fps_helper.get_profile(sensor, rs.stream.motion)))
            else:
                sensor_profiles_arr.append((sensor, fps_helper.get_profile(sensor, rs.stream.accel)))
                sensor_profiles_arr.append((sensor, fps_helper.get_profile(sensor, rs.stream.gyro)))

        if profile is not None:
            sensor_profiles_arr.append((sensor, profile))
    return sensor_profiles_arr


@pytest.mark.timeout(300)
def test_fps_permutations(test_device):
    dev, ctx = test_device

    sensor_profiles_array = get_sensors_and_profiles(dev)
    all_pairs = [[a[1].stream_name(), b[1].stream_name()] for a, b in combinations(sensor_profiles_array, 2)]
    fps_helper.perform_fps_test(sensor_profiles_array, all_pairs)
