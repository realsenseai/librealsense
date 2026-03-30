# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test pipeline start/stop reliability. Runs multiple iterations of start/stop
and verifies frames arrive each time.

On D455 and other units with IMU it takes ~4 seconds per iteration.
"""

import pytest
import pyrealsense2 as rs
from rspy.stopwatch import Stopwatch
import logging
log = logging.getLogger(__name__)

# Relaxed to 3 as 50 was failing often, See [LRS-1213]
ITERATIONS_COUNT = 3

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.context("nightly"),
]


@pytest.mark.timeout(220)
def test_pipeline_start_stop(test_device):
    dev, ctx = test_device

    pipe = rs.pipeline(ctx)
    pipe.set_device(dev)

    iteration_stopwatch = Stopwatch()
    for i in range(ITERATIONS_COUNT):
        iteration_stopwatch.reset()
        log.info(f"Starting iteration #{i + 1}/{ITERATIONS_COUNT}")

        start_call_stopwatch = Stopwatch()
        pipe.start()
        # wait_for_frames will throw if no frames received so no assert is needed
        f = pipe.wait_for_frames()
        delay = start_call_stopwatch.get_elapsed()
        log.info(f"After {delay:.3f} [sec] got first frame of {f}")
        pipe.stop()

        iteration_time = iteration_stopwatch.get_elapsed()
        log.info(f"Iteration took {iteration_time:.3f} [sec]")
