# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

# Test multiple set_pu commands checking that the set control event polling works as expected.
# We expect no exception thrown - See [RSDSO-17185]
# Moving bug check to weekly run frequency

import pytest
import pyrealsense2 as rs
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.context("weekly"),
    pytest.mark.timeout(600),
]

TEST_ITERATIONS = 200
GAIN_VALUES = [16, 74, 132, 190, 248]


def test_set_gain_stress(test_device_wrapped):
    """Stress test for setting a PU (gain) option — no exception should be thrown [RSDSO-17185]."""
    dev, ctx = test_device_wrapped
    time.sleep(3)  # The device starts at D0 (Operational) state, allow time for it to get into idle state

    depth_ir_sensor = dev.first_depth_sensor()

    if not depth_ir_sensor.supports(rs.option.gain):
        pytest.skip("Device does not support gain option")

    for i in range(TEST_ITERATIONS):
        log.debug(f"{'=' * 50} Iteration {i} {'=' * 50}")
        log.debug("Resetting Controls...")

        if depth_ir_sensor.supports(rs.option.enable_auto_exposure):
            depth_ir_sensor.set_option(rs.option.enable_auto_exposure, 0)
        if depth_ir_sensor.supports(rs.option.exposure):
            depth_ir_sensor.set_option(rs.option.exposure, 1)
            depth_ir_sensor.set_option(rs.option.gain, 248)

        log.debug("Resetting Controls Done")
        time.sleep(0.1)

        for val in GAIN_VALUES:
            log.debug(f"Setting Gain To: {val}")
            depth_ir_sensor.set_option(rs.option.gain, val)
            get_val = depth_ir_sensor.get_option(rs.option.gain)
            assert val == get_val, f"Gain mismatch at iteration {i}: set {val}, got {get_val}"
            log.debug(f"Gain Set To: {get_val}")
