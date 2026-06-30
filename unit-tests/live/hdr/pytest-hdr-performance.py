# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import json
import logging
import threading
import time

import pytest
import pyrealsense2 as rs
from pytest_check import check

import hdr_helper
from hdr_helper import HDR_CONFIGURATIONS

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D455"),
    pytest.mark.device_each("D457"),
    pytest.mark.context("nightly"),
]

EXPECTED_FPS = 30
ACCEPTABLE_FPS = EXPECTED_FPS * 0.9
TIME_FOR_STEADY_STATE = 3
TIME_TO_COUNT_FRAMES = 5


class FrameCounter:
    def __init__(self):
        self.count = 0
        self.counting = False
        self._lock = threading.Lock()

    def callback(self, frame):
        # Runs on librealsense's sensor callback thread, while counting/count are
        # mutated from the main test thread — guard with a lock.
        with self._lock:
            if not self.counting:
                return
            self.count += 1
        log.debug("Frame callback called, frame number: %s", frame.get_frame_number())

    def reset(self):
        with self._lock:
            self.count = 0
            self.counting = False

    def start(self):
        with self._lock:
            self.counting = True

    def stop(self):
        with self._lock:
            self.counting = False

    def get_count(self):
        with self._lock:
            return self.count


def test_hdr_performance(test_device):
    """
    Test HDR performance with various configurations
    """
    hdr_helper.setup_for_device(test_device)
    sensor = hdr_helper.sensor

    counter = FrameCounter()

    for i, config in enumerate(HDR_CONFIGURATIONS):
        config_type = "Auto" if "depth-ae" in json.dumps(config) else "Manual"
        num_items = len(config["hdr-preset"]["items"])
        test_name = f"Config {i + 1} ({config_type}, {num_items} items)"
        hdr_helper.test_json_load(config, test_name)

        counter.reset()
        depth_profile = next(p for p in sensor.get_stream_profiles() if p.stream_type() == rs.stream.depth)
        sensor.open(depth_profile)
        sensor.start(counter.callback)

        time.sleep(TIME_FOR_STEADY_STATE)
        counter.start()  # Start counting frames
        time.sleep(TIME_TO_COUNT_FRAMES)
        counter.stop()  # Stop counting

        sensor.stop()
        sensor.close()

        final_count = counter.get_count()
        measured_fps = final_count / TIME_TO_COUNT_FRAMES
        log.debug("Test %s: Counted frames = %d, Measured FPS = %.2f", test_name, final_count, measured_fps)
        check.greater(measured_fps, ACCEPTABLE_FPS, f"Measured FPS {measured_fps:.2f}")
