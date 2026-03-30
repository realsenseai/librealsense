# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test color frame drops by checking HW timestamp deltas.
Streams color at 60fps and verifies no frame drops across multiple iterations.
"""

import pytest
import pyrealsense2 as rs
from lrs_frame_queue_manager import LRSFrameQueueManager
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D400*"),
    pytest.mark.skip(reason="Test disabled (donotrun)"),
]

ITERATIONS = 4
SLEEP_PER_ITERATION = 10
FPS = 60
WIDTH, HEIGHT = 640, 480
FORMAT = rs.format.rgb8


def test_color_frame_drops(test_device):
    dev, ctx = test_device
    product_line = dev.get_info(rs.camera_info.product_line)
    color_sensor = dev.first_color_sensor()
    if color_sensor.supports(rs.option.auto_exposure_priority):
        color_sensor.set_option(rs.option.auto_exposure_priority, 0)

    hw_ts = []

    def cb(frame, ts):
        hw_ts.append(frame.get_frame_metadata(rs.frame_metadata_value.frame_timestamp))

    lrs_fq = LRSFrameQueueManager()
    lrs_fq.register_callback(cb)

    pipe = rs.pipeline(ctx)
    log.info(f"Testing color frame drops on {product_line} device")

    failures = []
    for i in range(ITERATIONS):
        lrs_fq.start()
        log.info(f"Iteration #{i + 1}")
        hw_ts.clear()

        cfg = rs.config()
        cfg.enable_stream(rs.stream.color, WIDTH, HEIGHT, FORMAT, FPS)
        pipe.start(cfg, lrs_fq.lrs_queue)
        time.sleep(SLEEP_PER_ITERATION)
        pipe.stop()

        expected_delta = 1000 / FPS
        deltas_ms = [(ts1 - ts2) / 1000 for ts1, ts2 in zip(hw_ts[1:], hw_ts[:-1])]
        drops = [(idx, delta) for idx, delta in enumerate(deltas_ms, 1)
                 if delta > (expected_delta * 1.95)]
        lrs_fq.stop()

        if drops:
            for idx, delta in drops:
                log.warning(f"Iteration {i+1}: drop #{idx} actual delta {delta:.1f} vs expected {expected_delta:.1f}")
            failures.append(f"Iteration {i+1}: {len(drops)} frame drops")

    assert not failures, "Color frame drops detected:\n" + "\n".join(failures)
