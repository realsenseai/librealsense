# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# See RSDSO-19304 or Github #10482 for background

import pytest
import pyrealsense2 as rs
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.context("nightly"),
    # The deadlock causes this test to time out but we don't want to wait the full 200 seconds
    pytest.mark.timeout(12),
]


def test_deadlock_after_hw_reset(test_device):
    ctx = rs.context({"dds": {"enabled": False}})  # the original deadlock is in the non-DDS path

    device_list = ctx.query_devices()
    log.info("%d RealSense devices connected", len(device_list))

    log.info("Resetting")
    for dev_idx in range(len(device_list)):
        device_list[dev_idx].hardware_reset()

    log.info("Sleeping 6 seconds")
    time.sleep(6)

    device_list = ctx.query_devices()
    log.info("%d devices after reset", len(device_list))

    log.info("This caused deadlock")
    for dev_idx in range(len(device_list)):
        d = device_list[dev_idx]
        # releasing the handle right away is what triggered the deadlock (C++ scope-end destructor)
        del d
