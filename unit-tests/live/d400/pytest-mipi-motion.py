# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

import pytest
import platform
import pyrealsense2 as rs
from pytest_check import check
import logging
log = logging.getLogger(__name__)
import time

pytestmark = [
    pytest.mark.device_each("D457"),
    pytest.mark.skipif(platform.machine() != "aarch64", reason="Jetson only"),
    pytest.mark.flaky(retries=3),
]

gyro_frame_count = 0
accel_frame_count = 0


def test_frame_index_mipi_imu(test_device):
    global gyro_frame_count, accel_frame_count
    gyro_frame_count = 0
    accel_frame_count = 0

    dev, _ = test_device

    def frame_callback(f):
        global gyro_frame_count, accel_frame_count
        stream_type = f.get_profile().stream_type()
        if stream_type == rs.stream.gyro:
            gyro_frame_count += 1
            check.equal(f.get_frame_number(), gyro_frame_count)
        elif stream_type == rs.stream.accel:
            accel_frame_count += 1
            check.equal(f.get_frame_number(), accel_frame_count)

    seconds_to_count_frames = 10
    sensor = dev.first_motion_sensor()
    motion_profile_accel = next(p for p in sensor.profiles if p.stream_type() == rs.stream.accel and p.fps() == 100)
    motion_profile_gyro = next(p for p in sensor.profiles if p.stream_type() == rs.stream.gyro and p.fps() == 100)
    sensor.open([motion_profile_accel, motion_profile_gyro])
    sensor.start(frame_callback)
    try:
        time.sleep(seconds_to_count_frames)  # Time to count frames
    finally:
        sensor.stop()
        sensor.close()
    assert gyro_frame_count > 0
    assert accel_frame_count > 0
