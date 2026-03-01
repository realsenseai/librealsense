# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

# This test checks for existence of motion intrinsic data in accel and gyro profiles.
# This validates a bug fix for code that seldom changes
# test:donotrun:!weekly
# test:device:jetson D457
# test:device:!jetson D455
# test:device:!jetson each(D500*)

import pyrealsense2 as rs
from rspy import test, log

device, _ = test.find_first_device_or_exit()
motion_sensor = device.first_motion_sensor()

test.start('Check intrinsics in motion sensor:')

test.check(motion_sensor)

if motion_sensor:
    if rs.stream.motion in [p.stream_type() for p in motion_sensor.profiles]: # D555 works with combined motion instead of accel and gyro
        motion_profile = next(p for p in motion_sensor.profiles if p.stream_type() == rs.stream.motion)
        motion_profiles = [motion_profile]
    else:
        motion_profile_accel = next(p for p in motion_sensor.profiles if p.stream_type() == rs.stream.accel)
        motion_profile_gyro = next(p for p in motion_sensor.profiles if p.stream_type() == rs.stream.gyro)
        test.check(motion_profile_accel and motion_profile_gyro)
        motion_profiles = [motion_profile_accel, motion_profile_gyro]

    print(motion_profiles)
    for motion_profile in motion_profiles:
        motion_profile = motion_profile.as_motion_stream_profile()
        intrinsics = motion_profile.get_motion_intrinsics()

        log.d(str(intrinsics))
        test.check(len(str(intrinsics)) > 0)  # Checking if intrinsics has data

test.finish()
test.print_results_and_exit()
