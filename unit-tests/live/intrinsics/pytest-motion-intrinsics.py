# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# This test checks for existence of motion intrinsic data in accel and gyro profiles.
# This validates a bug fix for code that seldom changes

import pytest
from pytest_check import check
import pyrealsense2 as rs
from rspy.pytest.device_helpers import is_jetson_platform
import logging
log = logging.getLogger(__name__)

if is_jetson_platform():
    pytestmark = [pytest.mark.context("weekly"), pytest.mark.device("D457")]
else:
    pytestmark = [pytest.mark.context("weekly"),
                  pytest.mark.device("D455"),
                  pytest.mark.device_each("D500*")]


def test_motion_intrinsics(test_device):
    device, _ = test_device
    motion_sensor = device.first_motion_sensor()

    assert motion_sensor

    if rs.stream.motion in [p.stream_type() for p in motion_sensor.profiles]: # D555 works with combined motion instead of accel and gyro
        motion_profile = next(p for p in motion_sensor.profiles if p.stream_type() == rs.stream.motion)
        motion_profiles = [motion_profile]
    else:
        motion_profile_accel = next(p for p in motion_sensor.profiles if p.stream_type() == rs.stream.accel)
        motion_profile_gyro = next(p for p in motion_sensor.profiles if p.stream_type() == rs.stream.gyro)
        check.is_true(motion_profile_accel and motion_profile_gyro)
        motion_profiles = [motion_profile_accel, motion_profile_gyro]

    print(motion_profiles)
    for motion_profile in motion_profiles:
        motion_profile = motion_profile.as_motion_stream_profile()
        intrinsics = motion_profile.get_motion_intrinsics()

        log.debug(str(intrinsics))
        check.is_true(len(str(intrinsics)) > 0)  # Checking if intrinsics has data
