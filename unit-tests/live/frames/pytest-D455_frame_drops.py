# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test D455 RGB frame drops at 90fps by checking HW timestamp deltas.
Uses producer-consumer thread pattern for high-throughput frame processing.
"""

import pytest
import pyrealsense2 as rs
import time
import threading
from queue import Queue
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D455"),
    pytest.mark.skip(reason="Test disabled (donotrun)"),
]

ITERATIONS = 60
STREAM_DURATION = 30  # seconds per iteration


class FrameDropDetector:
    def __init__(self, rgb_sensor):
        self._stop = False
        self.count_drops = 0
        self.frame_drops_info = {}
        self.prev_hw_timestamp = 0.0
        self.prev_fnum = 0
        self.first_frame = True
        self.lrs_queue = rs.frame_queue(capacity=100000, keep_frames=True)
        self.post_process_queue = Queue(maxsize=1000000)
        self.rgb_sensor = rgb_sensor

    def start_rgb_sensor(self):
        self.rgb_sensor.start(self.lrs_queue)

    def stop(self):
        self._stop = True

    def produce_frames(self, timeout=1):
        while not self._stop:
            try:
                lrs_frame = self.lrs_queue.wait_for_frame(timeout_ms=timeout * 1000)
            except Exception:
                continue
            self.post_process_queue.put(lrs_frame, block=True, timeout=timeout)

    def consume_frames(self):
        while not self._stop:
            element = self.post_process_queue.get(block=True)
            self._process_frame(element)
            del element
            self.post_process_queue.task_done()

    def _process_frame(self, f):
        if not f:
            return
        delta_tolerance_percent = 95.0
        ideal_delta = round(1000000.0 / 90, 2)
        delta_tolerance_in_us = ideal_delta * delta_tolerance_percent / 100.0

        if self.first_frame:
            self.prev_hw_timestamp = f.get_frame_metadata(rs.frame_metadata_value.frame_timestamp)
            self.prev_fnum = f.get_frame_number()
            self.first_frame = False
            return

        curr_hw_timestamp = f.get_frame_metadata(rs.frame_metadata_value.frame_timestamp)
        delta = curr_hw_timestamp - self.prev_hw_timestamp
        fnum = f.get_frame_number()
        if delta > ideal_delta + delta_tolerance_in_us:
            self.count_drops += 1
            self.frame_drops_info[fnum] = fnum - self.prev_fnum
        self.prev_hw_timestamp = curr_hw_timestamp
        self.prev_fnum = fnum


def test_d455_frame_drops(test_device):
    dev, ctx = test_device
    product_line = dev.get_info(rs.camera_info.product_line)

    sensors = dev.query_sensors()
    rgb_sensor = next(s for s in sensors if s.get_info(rs.camera_info.name) == 'RGB Camera')

    rgb_profiles = rgb_sensor.profiles
    rgb_profile = next(p for p in rgb_profiles
                       if p.fps() == 90
                       and p.stream_type() == rs.stream.color
                       and p.format() == rs.format.yuyv
                       and ((p.as_video_stream_profile().width() == 424 and p.as_video_stream_profile().height() == 240)
                            or (p.as_video_stream_profile().width() == 480 and p.as_video_stream_profile().height() == 270)
                            or (p.as_video_stream_profile().width() == 640 and p.as_video_stream_profile().height() == 360)))

    log.info(f"Testing D455 frame drops on {product_line} device")

    failures = []
    for ii in range(ITERATIONS):
        log.info(f"================ Iteration {ii} ================")
        detector = FrameDropDetector(rgb_sensor)
        rgb_sensor.set_option(rs.option.global_time_enabled, 0)
        rgb_sensor.open([rgb_profile])

        producer_thread = threading.Thread(target=detector.produce_frames, name="producer_thread")
        producer_thread.start()
        consumer_thread = threading.Thread(target=detector.consume_frames, name="consumer_thread")
        consumer_thread.start()

        detector.start_rgb_sensor()
        time.sleep(STREAM_DURATION)
        detector.stop()

        producer_thread.join(timeout=60)
        consumer_thread.join(timeout=60)

        log.info(f"Number of frame drops: {detector.count_drops}")
        if detector.count_drops > 0:
            failures.append(f"Iteration {ii}: {detector.count_drops} drops")

        rgb_sensor.stop()
        rgb_sensor.close()

    assert not failures, "D455 frame drops detected:\n" + "\n".join(failures)
