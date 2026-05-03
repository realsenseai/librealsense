# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import time
import pyrealsense2 as rs
from pytest_check import check
from rspy.timer import Timer
import logging
log = logging.getLogger(__name__)

pytestmark = [pytest.mark.device_each("D585S")]


MAX_TIME_TO_WAIT_FOR_FRAMES = 5  # [sec]


def test_frame_number_equal_to_counter(test_device):
    dev, _ = test_device

    state = {"waiting_for_test": False}

    def check_frame_number_equal_to_counter(frame):
        frame_number = frame.get_frame_number()
        frame_counter = frame.get_frame_metadata(rs.frame_metadata_value.frame_counter)
        check.equal(frame_number, frame_counter)
        state["waiting_for_test"] = False

    wait_for_frames_timer = Timer(MAX_TIME_TO_WAIT_FOR_FRAMES)

    depth_mapping_sensor = next(s for s in dev.query_sensors() if s.get_info(rs.camera_info.name) == "Depth Mapping Camera")
    occupancy_stream_profile = next(p for p in depth_mapping_sensor.profiles if p.stream_type() == rs.stream.occupancy)
    labeled_points_stream_profile = next(p for p in depth_mapping_sensor.profiles if p.stream_type() == rs.stream.labeled_point_cloud)
    safety_sensor = dev.first_safety_sensor()
    safety_stream_profile = next(p for p in safety_sensor.profiles if p.stream_type() == rs.stream.safety)

    sensors_and_stream_profiles = [[depth_mapping_sensor, occupancy_stream_profile],
                                   [depth_mapping_sensor, labeled_points_stream_profile],
                                   [safety_sensor, safety_stream_profile]]

    for ssp in sensors_and_stream_profiles:
        sensor = ssp[0]
        stream_profile = ssp[1]
        log.info("testing: %s, %s", sensor.get_info(rs.camera_info.name), repr(stream_profile.stream_type()))
        sensor.open(stream_profile)
        sensor.start(check_frame_number_equal_to_counter)
        wait_for_frames_timer.start()
        state["waiting_for_test"] = True
        while state["waiting_for_test"] and not wait_for_frames_timer.has_expired():
            time.sleep(0.5)
        check.is_true(not wait_for_frames_timer.has_expired())
        sensor.stop()
        sensor.close()
