# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import time
import pyrealsense2 as rs
from pytest_check import check
from rspy.timer import Timer
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585S"),
    pytest.mark.context("nightly"),
]


MAX_TIME_TO_WAIT_FOR_FRAMES = 5  # [sec]
NUM_OF_FRAMES_BEFORE_EACH_CHECK = 50


def test_global_ts_for_depth(test_device):
    state = {
        "global_ts_value": None,
        "global_ts_changed": False,
        "global_frame_num": 0,
    }

    def callback(frame):
        state["global_frame_num"] += 1
        if state["global_ts_changed"]:
            state["global_ts_changed"] = False
            domain = frame.get_frame_timestamp_domain()
            if state["global_ts_value"] == True:
                check.equal(domain, rs.time_domain.global_time)
            else:
                check.not_equal(domain, rs.time_domain.global_time)

    wait_for_frames_timer = Timer(MAX_TIME_TO_WAIT_FOR_FRAMES)

    def wait_and_check(num_of_frames_before_check):
        while not wait_for_frames_timer.has_expired() and state["global_frame_num"] < num_of_frames_before_check:
            time.sleep(0.5)
        if wait_for_frames_timer.has_expired():
            pytest.fail(f"timer expired: {num_of_frames_before_check} frames did not arrive before {MAX_TIME_TO_WAIT_FOR_FRAMES} sec")

    dev, _ = test_device
    depth_sensor = dev.first_depth_sensor()

    assert depth_sensor.supports(rs.option.global_time_enabled)

    gt_value_to_set = 1
    depth_sensor.set_option(rs.option.global_time_enabled, gt_value_to_set)
    state["global_ts_value"] = int(depth_sensor.get_option(rs.option.global_time_enabled))
    assert state["global_ts_value"] == gt_value_to_set

    # Start streaming
    depth_profile = next(p for p in depth_sensor.profiles if p.stream_type() == rs.stream.depth and p.is_default())
    depth_sensor.open(depth_profile)
    depth_sensor.start(callback)

    wait_for_frames_timer.start()
    wait_and_check(NUM_OF_FRAMES_BEFORE_EACH_CHECK)
    gt_value_to_set = 0
    depth_sensor.set_option(rs.option.global_time_enabled, gt_value_to_set)
    state["global_ts_changed"] = True

    wait_for_frames_timer.start()
    wait_and_check(2 * NUM_OF_FRAMES_BEFORE_EACH_CHECK)
    gt_value_to_set = 1
    depth_sensor.set_option(rs.option.global_time_enabled, gt_value_to_set)
    state["global_ts_changed"] = True

    time.sleep(0.5)
    depth_sensor.stop()
    depth_sensor.close()
