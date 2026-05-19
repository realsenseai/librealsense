# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test FPS accuracy on D585S with various sensor permutations
(Depth+Color, Depth+Color+Safety, etc.).
"""

import pytest
import pyrealsense2 as rs
from rspy import tests_wrapper as tw
import fps_helper
import logging
log = logging.getLogger(__name__)

VGA_RESOLUTION = (640, 360)
HD_RESOLUTION = (1280, 720)

pytestmark = [
    pytest.mark.device_each("D585S"),
    pytest.mark.context("nightly"),
]


def get_sensors_and_profiles(device):
    """Returns an array of pairs of a (sensor, profile) for each of its profiles."""
    sensor_profiles_arr = []
    for sensor in device.query_sensors():
        profile = None
        if sensor.is_depth_sensor():
            if sensor.supports(rs.option.enable_auto_exposure):
                sensor.set_option(rs.option.enable_auto_exposure, 1)
            profile = fps_helper.get_profile(sensor, rs.stream.depth, VGA_RESOLUTION, 30)
        elif sensor.is_color_sensor():
            if sensor.supports(rs.option.enable_auto_exposure):
                sensor.set_option(rs.option.enable_auto_exposure, 1)
            if sensor.supports(rs.option.auto_exposure_priority):
                sensor.set_option(rs.option.auto_exposure_priority, 0)
            profile = fps_helper.get_profile(sensor, rs.stream.color, HD_RESOLUTION, 30)
        elif sensor.is_motion_sensor():
            sensor_profiles_arr.append((sensor, fps_helper.get_profile(sensor, rs.stream.accel)))
            sensor_profiles_arr.append((sensor, fps_helper.get_profile(sensor, rs.stream.gyro)))
        elif sensor.is_safety_sensor():
            profile = fps_helper.get_profile(sensor, rs.stream.safety)
        elif sensor.name == "Depth Mapping Camera":
            sensor_profiles_arr.append((sensor, fps_helper.get_profile(sensor, rs.stream.labeled_point_cloud)))
            sensor_profiles_arr.append((sensor, fps_helper.get_profile(sensor, rs.stream.occupancy)))

        if profile is not None:
            sensor_profiles_arr.append((sensor, profile))
    return sensor_profiles_arr


PERMUTATIONS = [
    ["Depth", "Color"],
    ["Depth", "Color", "Safety"],
    ["Depth", "Color", "Safety", "Occupancy"],
    ["Depth", "Color", "Safety", "Labeled Point Cloud"],
    ["Depth", "Color", "Accel", "Gyro"],
]


def test_ah_configurations(test_device):
    dev, ctx = test_device

    tw.start_wrapper(dev)
    try:
        sensor_profiles_array = get_sensors_and_profiles(dev)
        fps_helper.perform_fps_test(sensor_profiles_array, PERMUTATIONS)
    finally:
        tw.stop_wrapper(dev)
